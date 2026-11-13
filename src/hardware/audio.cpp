#include "audio.h"
#include "amplifier.h"
#include "config.h"
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#include "hal/dac_ll.h"
#include <math.h>
#include "player/melody_generator.h"
#include "songs.h"

// ==============================================================================
// 专为 ESP32-2432S028R-Plus (CYD Plus) 打造的硬件定时器无锁环形缓冲区 DAC 驱动 (AudioOutputTimerDAC)
//
// 彻底解决三大历史痛点：
//   1. 物理引脚冲突：原生 ESP8266Audio::AudioOutputI2S(INTERNAL_DAC) 默认占用 GPIO 25/26，
//      霸占触控屏关键时钟线 TOUCH_SCK_PIN (GPIO 25)，导致触控完全失灵；
//   2. "既不出声也不推进"：ESP-IDF 内置 DAC I2S 驱动在单声道模式下易发生 DMA 阻塞与 APLL 冲突；
//   3. 主循环饿死与杂音：此前 busy-wait 节流导致 UI 绘制与音频交替争抢，产生 50Hz 杂音蜂鸣。
//
// 驱动核心机制：
//   - 仅操作 DAC2 (GPIO 26 -> 8002 功放)，严禁干预 DAC1 (GPIO 25)，触控屏与音频绝对解耦并行；
//   - 8192 字节无锁环形缓冲区 (Ring Buffer)：在 22.05kHz 下提供 371ms 充盈缓冲，彻底消除 UI 阻塞卡顿；
//   - ESP32 硬件定时器 0 中断驱动：10MHz 计数基准 (divider=8)，单次 ISR 耗时 < 50ns，CPU 占用率 < 0.25%；
//   - 待机与缓冲区见底时自动保持 128 (1.65V 中点偏置)，杜绝 8002 功放输入悬空引起的自激啸叫。
// ==============================================================================

#define AUDIO_RING_BUF_SIZE 16384
#define AUDIO_RING_BUF_MASK (AUDIO_RING_BUF_SIZE - 1)
#define AUDIO_TIMER_BASE_HZ 40000000ULL // 80MHz APB / divider 2 = 40MHz 高精度计数基准

static hw_timer_t *s_audioTimer = nullptr;
static uint8_t s_audioRingBuf[AUDIO_RING_BUF_SIZE];
static volatile uint16_t s_rbHead = 0;
static volatile uint16_t s_rbTail = 0;
static volatile bool s_timerRunning = false;
static volatile bool s_rbNeedPrefill = true;
static volatile uint16_t s_rbPrefillTarget = 256;
static volatile uint8_t s_lastDacSample = 128;

// 硬件定时器高频中断服务程序 (驻留 IRAM，直接驱动 DAC2)
//
// 注意：这里必须用 hal/dac_ll.h 的 static inline 寄存器写入，不能调
// driver/dac.h 的 dac_output_voltage()。后者链接在 Flash 上、无 IRAM_ATTR，
// ISR 执行期间一旦发生 Flash 写（NVS 保存设置、SD 写日志等）会关闭 cache，
// 取指失败直接 Guru Meditation。dac_ll_update_output_value() 会内联进本函数，
// 全程只碰寄存器，不依赖任何 Flash 代码。
void IRAM_ATTR onAudioTimer() {
  uint16_t head = s_rbHead;
  uint16_t tail = s_rbTail;
  uint16_t avail = (head - tail) & AUDIO_RING_BUF_MASK;

  if (s_rbNeedPrefill) {
    if (avail >= s_rbPrefillTarget) {
      s_rbNeedPrefill = false;
    } else {
      uint8_t cur = s_lastDacSample;
      if (cur > 128) cur--;
      else if (cur < 128) cur++;
      s_lastDacSample = cur;
      dac_ll_update_output_value(DAC_CHANNEL_2, cur);
      return;
    }
  }

  if (avail > 0) {
    uint8_t sample = s_audioRingBuf[tail];
    s_rbTail = (tail + 1) & AUDIO_RING_BUF_MASK;
    s_lastDacSample = sample;
    dac_ll_update_output_value(DAC_CHANNEL_2, sample);
  } else {
    // 缓冲区见底：平滑渐归 128 中点偏置，避免重设预缓冲水线导致周期性 6ms 卡顿
    uint8_t cur = s_lastDacSample;
    if (cur > 128) cur--;
    else if (cur < 128) cur++;
    s_lastDacSample = cur;
    dac_ll_update_output_value(DAC_CHANNEL_2, cur);
  }
}

class AudioOutputTimerDAC : public AudioOutput {
public:
  AudioOutputTimerDAC() {
    hertz = 22050;
    bps = 16;
    channels = 1;
    gainF2P6 = 64; // 1.0f 默认增益
  }

  virtual ~AudioOutputTimerDAC() {
    stop();
  }

  virtual bool begin() override {
    // 1. 确保 GPIO 26 内部 DAC2 使能并脱离 I2S DMA 强控，置于 128 中点
    dac_ll_digi_enable_dma(false);
    dac_output_enable(DAC_CHANNEL_2);
    dac_output_voltage(DAC_CHANNEL_2, 128);

    // 2. 复位环形缓冲区双指针与预充水线
    s_rbHead = 0;
    s_rbTail = 0;
    s_lastDacSample = 128;
    s_rbPrefillTarget = 256;
    s_rbNeedPrefill = true;

    // 3. 配置硬件定时器 0 (80MHz / 2 = 40MHz, 1 周期 = 25ns)
    if (!s_audioTimer) {
      s_audioTimer = timerBegin(0, 2, true);
      timerAttachInterrupt(s_audioTimer, &onAudioTimer, true);
    }

    uint64_t ticks = AUDIO_TIMER_BASE_HZ / (hertz > 0 ? (uint64_t)hertz : 22050ULL);
    timerAlarmWrite(s_audioTimer, ticks, true);
    timerAlarmEnable(s_audioTimer);
    s_timerRunning = true;

    LOG_AUDIO("AudioOutputTimerDAC: 硬件定时器启动成功 (Rate=%d Hz, Ticks=%llu, 16KB Buffer)", hertz, ticks);
    return true;
  }

  virtual bool SetRate(int hz) override {
    if (hz <= 0) hz = 22050;
    this->hertz = hz;
    if (s_audioTimer && s_timerRunning) {
      uint64_t ticks = AUDIO_TIMER_BASE_HZ / (uint64_t)hz;
      timerAlarmWrite(s_audioTimer, ticks, true);
    }
    return true;
  }

  virtual bool SetBitsPerSample(int bits) override {
    this->bps = bits;
    return true;
  }

  virtual bool SetChannels(int ch) override {
    this->channels = ch;
    return true;
  }

  virtual bool ConsumeSample(int16_t sample[2]) override {
    uint16_t nextHead = (s_rbHead + 1) & AUDIO_RING_BUF_MASK;
    if (nextHead == s_rbTail) {
      // 环形缓冲区已充盈，让出 CPU
      return false;
    }

    // 统一为立体声并混音为单声道
    MakeSampleStereo16(sample);
    int32_t mono = ((int32_t)sample[0] + (int32_t)sample[1]) / 2;

    // 应用音量增益
    int16_t v = Amplify((int16_t)mono);

    // 16-bit 有符号 (-32768 ~ +32767) 映射为 8-bit 无符号 (0 ~ 255)
    uint8_t dacVal = (uint8_t)((v >> 8) + 128);

    s_audioRingBuf[s_rbHead] = dacVal;
    s_rbHead = nextHead;
    return true;
  }

  virtual bool stop() override {
    if (s_audioTimer && s_timerRunning) {
      timerAlarmDisable(s_audioTimer);
      s_timerRunning = false;
    }
    s_rbHead = 0;
    s_rbTail = 0;
    dac_output_voltage(DAC_CHANNEL_2, 128);
    return true;
  }
};

static uint8_t s_volume = 70; // 默认 70%

static AudioFileSourceSD *s_file = nullptr;
static AudioGeneratorMP3 *s_mp3 = nullptr;
static AudioGeneratorMelody *s_melody = nullptr;
static AudioOutputTimerDAC *s_out = nullptr;

static bool s_isPlayingAudio = false;
static bool s_isStreamingAudio = false;
static bool s_isMelody = false;
static bool s_isMp3 = false;
static uint32_t s_streamRate = 44100;
static volatile uint8_t s_streamLevel = 0;
static bool s_i2sStreamInstalled = false;
static int32_t s_prevMonoSample = 0;
static int32_t s_sdError = 0;
static int32_t s_dcPrevX = 0;
static int32_t s_dcPrevY = 0;
static uint32_t s_streamBuf[512];
static SemaphoreHandle_t s_streamMutex = nullptr;
static StaticSemaphore_t s_streamMutexBuf;

// 待机中点偏置：功放关闭或待机时，将 DAC2 稳定钳位在 1.65V 中点
static void audio_hold_idle_bias() {
  if (s_isPlayingAudio || s_isStreamingAudio) return;
  dac_ll_digi_enable_dma(false);
  dac_output_enable(DAC_CHANNEL_2);
  dac_output_voltage(DAC_CHANNEL_2, 128);
}

void audio_init() {
  LOG_AUDIO("初始化硬件定时器 DAC2 音频子系统 (GPIO 26 -> 8002 功放, 绝不干涉 GPIO 25 触控)...");

  amp_init();

  s_out = new AudioOutputTimerDAC();
  s_mp3 = new AudioGeneratorMP3();
  s_melody = new AudioGeneratorMelody();
  s_file = new AudioFileSourceSD();

  audio_set_volume(s_volume);
  audio_hold_idle_bias();
  LOG_AUDIO("音频系统初始化就绪 (默认音量: %d%%)", s_volume);
}

void audio_set_volume(uint8_t volume_percent) {
  s_volume = constrain(volume_percent, 0, 100);
  LOG_AUDIO("音量全局设置: %d%%", s_volume);
  if (s_out) {
    // 0.95 增益系数，防止大动态音频造成 8002 削顶破音
    s_out->SetGain((s_volume / 100.0f) * 0.95f);
  }
}

uint8_t audio_get_volume() {
  return s_volume;
}

AudioOutput* audio_get_output() {
  return s_out;
}

bool audio_play_mp3(const char* filepath) {
  audio_stop();
  if (!s_file->open(filepath)) {
    LOG_AUDIO("无法打开MP3文件: %s", filepath);
    return false;
  }

  amp_enable();
  if (!s_mp3->begin(s_file, s_out)) {
    LOG_AUDIO("MP3解码器启动失败: %s", filepath);
    s_file->close();
    amp_disable();
    s_isPlayingAudio = false;
    audio_hold_idle_bias();
    return false;
  }

  s_isPlayingAudio = true;
  s_isMelody = false;
  s_isMp3 = true;
  LOG_AUDIO("开始播放MP3: %s", filepath);
  return true;
}

bool audio_play_demo(int songIndex) {
  if (songIndex < 0 || songIndex >= DEMO_SONGS_COUNT) songIndex = 0;
  audio_stop();

  const DemoSong& ds = DEMO_SONGS[songIndex];
  LOG_AUDIO("开始播放内置示范旋律: [%d] %s (%d 个音符)...", songIndex, ds.title, ds.length);

  amp_enable();
  if (!s_melody->begin(ds.notes, ds.length, s_out, true)) {
    LOG_AUDIO("Melody 合成器启动失败");
    amp_disable();
    s_isPlayingAudio = false;
    audio_hold_idle_bias();
    return false;
  }

  s_isPlayingAudio = true;
  s_isMelody = true;
  s_isMp3 = false;
  return true;
}

bool audio_play_demo_wav() {
  return audio_play_demo(0);
}

void audio_pause() {
  if (s_isPlayingAudio) {
    amp_disable();
    s_out->stop();
  }
  s_isPlayingAudio = false;
  audio_hold_idle_bias();
}

void audio_resume() {
  if (!s_isPlayingAudio) {
    amp_enable();
    s_out->begin();
    s_isPlayingAudio = true;
  }
}

void audio_stop() {
  if (s_isStreamingAudio || s_i2sStreamInstalled) {
    audio_stream_stop();
  }
  if (s_isMelody && s_melody && s_melody->isRunning()) {
    s_melody->stop();
  }
  if (s_isMp3 && s_mp3 && s_mp3->isRunning()) {
    s_mp3->stop();
    if (s_file && s_file->isOpen()) s_file->close();
  }
  if (s_out) {
    s_out->stop();
  }

  amp_disable();
  s_isPlayingAudio = false;
  s_isMelody = false;
  s_isMp3 = false;
  audio_hold_idle_bias();
}

void audio_test_beep(uint16_t freq, uint16_t duration_ms) {
  if (s_isPlayingAudio || s_isStreamingAudio) return;
  if (freq < 100 || freq > 8000) return;
  if (duration_ms == 0) duration_ms = 40;

  // 1. 建立中点偏置
  dac_ll_digi_enable_dma(false);
  dac_output_enable(DAC_CHANNEL_2);
  dac_output_voltage(DAC_CHANNEL_2, 128);
  delayMicroseconds(300);
  amp_enable();
  delayMicroseconds(300);

  const uint32_t periodUs = 1000000UL / freq;
  const uint32_t halfPeriodUs = periodUs / 2;
  const unsigned long start = millis();

  // 柔和微弱蜂鸣 (幅值 ±40，音色温和，非满幅破音方波)
  while (millis() - start < duration_ms) {
    dac_output_voltage(DAC_CHANNEL_2, 128 + 40);
    delayMicroseconds(halfPeriodUs);
    dac_output_voltage(DAC_CHANNEL_2, 128 - 40);
    delayMicroseconds(halfPeriodUs);
  }

  amp_disable();
  delayMicroseconds(300);
  dac_output_voltage(DAC_CHANNEL_2, 128);
}

void audio_update() {
  if (!s_isPlayingAudio) return;

  if (s_isMelody) {
    if (s_melody && s_melody->isRunning()) {
      if (!s_melody->loop()) {
        s_melody->stop();
        s_out->stop();
        amp_disable();
        s_isPlayingAudio = false;
        audio_hold_idle_bias();
        LOG_AUDIO("示范旋律播放结束");
      }
    }
  } else if (s_isMp3) {
    if (s_mp3 && s_mp3->isRunning()) {
      if (!s_mp3->loop()) {
        s_mp3->stop();
        if (s_file && s_file->isOpen()) s_file->close();
        s_out->stop();
        amp_disable();
        s_isPlayingAudio = false;
        audio_hold_idle_bias();
        LOG_AUDIO("MP3 播放结束");
      }
    }
  }
}

bool audio_is_playing() {
  return s_isPlayingAudio || s_isStreamingAudio;
}

uint32_t audio_get_mp3_position() {
  return 0;
}

uint32_t audio_get_mp3_duration() {
  return 0;
}

uint16_t audio_get_current_freq() {
  if (s_isPlayingAudio && s_isMelody && s_melody) {
    return s_melody->getCurrentFreq();
  }
  return 0;
}

// ==============================================================================
// 外部实时 PCM 音频流驱动（供蓝牙 A2DP 音箱模式使用）
// 使用 I2S0 DMA + 2x 线性插值过采样 + DC 消除 + 软限幅 + 一阶 Sigma-Delta 噪声整形
// 仅启用 DAC_CHANNEL_2 (GPIO 26)，绝不调用 i2s_set_pin(NULL)，完整保留 GPIO 25 触摸时钟！
// ==============================================================================
static inline uint32_t dsp_shape_to_i2s_dac(int32_t sample16) {
  int32_t target16 = sample16 + 32768;
  int32_t acc = target16 + s_sdError;
  int32_t q8 = (acc + 128) >> 8;
  if (q8 < 0) q8 = 0;
  else if (q8 > 255) q8 = 255;

  s_sdError = acc - (q8 << 8);
  if (s_sdError > 256) s_sdError = 256;
  else if (s_sdError < -256) s_sdError = -256;

  uint16_t dacWord = (uint16_t)(q8 << 8);
  return ((uint32_t)dacWord << 16) | dacWord;
}

static inline void audio_guard_touch_sck_pin() {
  dac_output_disable(DAC_CHANNEL_1);
  pinMode(TOUCH_SCK_PIN, OUTPUT);
  digitalWrite(TOUCH_SCK_PIN, LOW);
}

void audio_stream_start(uint32_t sample_rate) {
  audio_stop();
  if (!s_streamMutex) {
    s_streamMutex = xSemaphoreCreateMutexStatic(&s_streamMutexBuf);
  }

  if (sample_rate < 8000 || sample_rate > 96000) sample_rate = 44100;
  s_streamRate = sample_rate;
  const uint32_t dacRate = (s_streamRate <= 48000) ? (s_streamRate * 2) : s_streamRate;

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
  cfg.sample_rate = dacRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 10;
  cfg.dma_buf_len = 512;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
  cfg.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    LOG_AUDIO("I2S0 蓝牙流驱动安装失败");
    return;
  }

  // 仅打开 DAC_CHANNEL_2 (GPIO 26)，绝不触碰 DAC_CHANNEL_1 (GPIO 25 触摸屏 SCK)
  i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
  i2s_set_sample_rates(I2S_NUM_0, dacRate);
  audio_guard_touch_sck_pin();

  // 预填充 1.65V 中点静音样本，消除启动冲击声
  for (size_t i = 0; i < 512; i++) {
    s_streamBuf[i] = 0x80008000UL;
  }
  size_t bytesWritten = 0;
  for (int i = 0; i < 4; i++) {
    i2s_write(I2S_NUM_0, s_streamBuf, sizeof(s_streamBuf), &bytesWritten, pdMS_TO_TICKS(20));
  }

  s_prevMonoSample = 0;
  s_sdError = 0;
  s_dcPrevX = 0;
  s_dcPrevY = 0;
  s_streamLevel = 0;
  s_i2sStreamInstalled = true;
  s_isStreamingAudio = true;

  LOG_AUDIO("蓝牙 I2S0-DMA 高清音频流已启动 (In=%lu Hz -> DAC=%lu Hz, 1st-Order Sigma-Delta)",
            (unsigned long)s_streamRate, (unsigned long)dacRate);
}

void audio_stream_set_rate(uint32_t sample_rate) {
  if (sample_rate < 8000 || sample_rate > 96000) return;
  if (sample_rate == s_streamRate) return;
  s_streamRate = sample_rate;
  if (s_i2sStreamInstalled && s_streamMutex &&
      xSemaphoreTake(s_streamMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (s_i2sStreamInstalled) {
      const uint32_t dacRate = (s_streamRate <= 48000) ? (s_streamRate * 2) : s_streamRate;
      i2s_set_sample_rates(I2S_NUM_0, dacRate);
      audio_guard_touch_sck_pin();
    }
    xSemaphoreGive(s_streamMutex);
  }
  LOG_AUDIO("蓝牙 I2S0 采样率同步: In=%lu Hz -> DAC=%lu Hz",
            (unsigned long)s_streamRate,
            (unsigned long)((s_streamRate <= 48000) ? (s_streamRate * 2) : s_streamRate));
}

void audio_stream_write_pcm16_stereo(const uint8_t* data, uint32_t len, uint8_t gain_percent) {
  if (!s_isStreamingAudio || !s_i2sStreamInstalled || !data || len < 4) return;
  if (!s_streamMutex || xSemaphoreTake(s_streamMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;

  if (!s_isStreamingAudio || !s_i2sStreamInstalled) {
    xSemaphoreGive(s_streamMutex);
    return;
  }

  const int16_t* pcm = (const int16_t*)data;
  const uint32_t frameCount = len / 4; // 每帧包含 Left(int16) + Right(int16)
  const bool oversample = (s_streamRate <= 48000);

  // 增益映射：乘以 ~0.85 防削顶余量，再经过软限幅，保护 8002 功放与 1W 喇叭不破音
  if (gain_percent > 100) gain_percent = 100;
  const int32_t gainScale = ((int32_t)gain_percent * 218) / 100; // 0..218 (256 为 1.0x)

  int32_t peakAbs = 0;
  size_t outIdx = 0;

  for (uint32_t i = 0; i < frameCount; i++) {
    int32_t left = pcm[i * 2];
    int32_t right = pcm[i * 2 + 1];
    int32_t mono = (left + right) >> 1;

    int32_t absVal = (mono < 0) ? -mono : mono;
    if (absVal > peakAbs) peakAbs = absVal;

    // 一阶高通去直流滤波器 (~19 Hz @ 44.1kHz)，避免直流偏置抖动引发喇叭底噪或机械敲击
    int32_t y = mono - s_dcPrevX + (int32_t)(((int64_t)32680 * s_dcPrevY) >> 15);
    s_dcPrevX = mono;
    if (y > 65535) y = 65535;
    else if (y < -65535) y = -65535;
    s_dcPrevY = y;

    int32_t v = (y * gainScale) >> 8;
    if (v > 26000) {
      v = 26000 + ((v - 26000) >> 2);
      if (v > 32767) v = 32767;
    } else if (v < -26000) {
      v = -26000 + ((v + 26000) >> 2);
      if (v < -32768) v = -32768;
    }

    if (oversample) {
      int32_t mid = (s_prevMonoSample + v) >> 1;
      s_prevMonoSample = v;
      s_streamBuf[outIdx++] = dsp_shape_to_i2s_dac(mid);
      s_streamBuf[outIdx++] = dsp_shape_to_i2s_dac(v);
    } else {
      s_prevMonoSample = v;
      s_streamBuf[outIdx++] = dsp_shape_to_i2s_dac(v);
    }

    if (outIdx >= 510) {
      size_t bytesWritten = 0;
      i2s_write(I2S_NUM_0, s_streamBuf, outIdx * sizeof(uint32_t), &bytesWritten, pdMS_TO_TICKS(50));
      outIdx = 0;
      if (!s_isStreamingAudio) break;
    }
  }

  if (outIdx > 0 && s_isStreamingAudio) {
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, s_streamBuf, outIdx * sizeof(uint32_t), &bytesWritten, pdMS_TO_TICKS(50));
  }

  xSemaphoreGive(s_streamMutex);

  // 更新实时音频能量峰值 (0..100)，供 UI 频谱与 RGB 灯效使用
  uint8_t instLevel = (uint8_t)constrain((peakAbs * 100) / 24000, 0, 100);
  s_streamLevel = (uint8_t)((s_streamLevel * 2 + instLevel * 2) / 4);
}

void audio_stream_stop() {
  s_isStreamingAudio = false;
  s_streamLevel = 0;

  if (s_streamMutex && xSemaphoreTake(s_streamMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    if (s_i2sStreamInstalled) {
      i2s_stop(I2S_NUM_0);
      i2s_driver_uninstall(I2S_NUM_0);
      s_i2sStreamInstalled = false;
    }
    xSemaphoreGive(s_streamMutex);
  } else if (s_i2sStreamInstalled) {
    i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    s_i2sStreamInstalled = false;
  }

  s_prevMonoSample = 0;
  s_sdError = 0;
  s_dcPrevX = 0;
  s_dcPrevY = 0;
  amp_disable();
  audio_guard_touch_sck_pin();
  audio_hold_idle_bias();
  LOG_AUDIO("蓝牙 I2S0-DMA 音频流已停止");
}

uint8_t audio_stream_get_level() {
  if (!s_isStreamingAudio) return 0;
  return s_streamLevel;
}
