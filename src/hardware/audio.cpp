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
// 专为 ESP32-2432S028R-Plus (CYD Plus) 打造的统一 I2S0-DMA 高清 DAC 驱动引擎
//
// 所有音频源（本地 MP3、内置示范旋律、网络电台、蓝牙 A2DP 音箱）统一经过：
//   1) 一阶高通去直流 (~19 Hz) -> 2) 三档硬件自适应音效 EQ ->
//   3) 余量缩放与软限幅 -> 4) 2x 线性插值过采样 -> 5) 一阶 Sigma-Delta 噪声整形
// 仅启用 DAC_CHANNEL_2 (GPIO 26)，绝不调用 i2s_set_pin(NULL)，完整隔离 GPIO 25 触摸时钟！
// ==============================================================================

static uint8_t s_volume = 70; // 默认 70%
static AudioEQMode s_eqMode = AUDIO_EQ_FLAT;

static AudioFileSourceSD *s_file = nullptr;
static AudioGeneratorMP3 *s_mp3 = nullptr;
static AudioGeneratorMelody *s_melody = nullptr;
class AudioOutputI2SDAC;
static AudioOutputI2SDAC *s_out = nullptr;

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
static int32_t s_eqLpState = 0;
static int32_t s_eqMidLpState = 0;
static uint32_t s_streamBuf[512];
static SemaphoreHandle_t s_streamMutex = nullptr;
static StaticSemaphore_t s_streamMutexBuf;

static inline void audio_guard_touch_sck_pin() {
  dac_output_disable(DAC_CHANNEL_1);
  pinMode(TOUCH_SCK_PIN, OUTPUT);
  digitalWrite(TOUCH_SCK_PIN, LOW);
}

static inline int32_t dsp_apply_eq(int32_t x) {
  if (s_eqMode == AUDIO_EQ_WARM_BASS) {
    // 温暖低音：一阶低通 (~160Hz @ 44.1kHz) 增强低频 +45%，轻微平滑刺耳高频
    s_eqLpState += ((x - s_eqLpState) >> 5);
    int32_t boosted = x + ((s_eqLpState * 115) >> 8);
    s_eqMidLpState += ((boosted - s_eqMidLpState) >> 1);
    return (boosted * 3 + s_eqMidLpState) >> 2;
  } else if (s_eqMode == AUDIO_EQ_CLEAR_VOCAL) {
    // 清晰人声：轻削浑浊超低频，增强人声中频存在感
    s_eqLpState += ((x - s_eqLpState) >> 5);
    s_eqMidLpState += ((x - s_eqMidLpState) >> 2);
    int32_t noMud = x - (s_eqLpState >> 2);
    int32_t midBand = s_eqMidLpState - s_eqLpState;
    return noMud + ((midBand * 64) >> 8);
  }
  return x;
}

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

static inline void dsp_push_mono_frame(int32_t mono, int32_t gainScale, bool oversample,
                                       uint32_t* outBuf, size_t& outIdx) {
  int32_t y = mono - s_dcPrevX + (int32_t)(((int64_t)32680 * s_dcPrevY) >> 15);
  s_dcPrevX = mono;
  if (y > 65535) y = 65535;
  else if (y < -65535) y = -65535;
  s_dcPrevY = y;

  y = dsp_apply_eq(y);

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
    outBuf[outIdx++] = dsp_shape_to_i2s_dac(mid);
    outBuf[outIdx++] = dsp_shape_to_i2s_dac(v);
  } else {
    s_prevMonoSample = v;
    outBuf[outIdx++] = dsp_shape_to_i2s_dac(v);
  }
}

static void audio_i2s_reset_dsp_state() {
  s_prevMonoSample = 0;
  s_sdError = 0;
  s_dcPrevX = 0;
  s_dcPrevY = 0;
  s_eqLpState = 0;
  s_eqMidLpState = 0;
  s_streamLevel = 0;
}

static bool audio_i2s_install(uint32_t inRate) {
  if (inRate < 8000 || inRate > 96000) inRate = 44100;
  s_streamRate = inRate;
  const uint32_t dacRate = (s_streamRate <= 48000) ? (s_streamRate * 2) : s_streamRate;

  if (s_i2sStreamInstalled) {
    i2s_set_sample_rates(I2S_NUM_0, dacRate);
    audio_guard_touch_sck_pin();
    return true;
  }

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
    LOG_AUDIO("I2S0-DMA DAC 驱动安装失败");
    return false;
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

  audio_i2s_reset_dsp_state();
  s_i2sStreamInstalled = true;
  return true;
}

static void audio_i2s_set_rate(uint32_t inRate) {
  if (inRate < 8000 || inRate > 96000) return;
  if (inRate == s_streamRate) return;
  s_streamRate = inRate;
  if (s_i2sStreamInstalled) {
    const uint32_t dacRate = (s_streamRate <= 48000) ? (s_streamRate * 2) : s_streamRate;
    i2s_set_sample_rates(I2S_NUM_0, dacRate);
    audio_guard_touch_sck_pin();
  }
}

static void audio_i2s_uninstall() {
  if (s_i2sStreamInstalled) {
    i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    s_i2sStreamInstalled = false;
  }
  audio_i2s_reset_dsp_state();
  audio_guard_touch_sck_pin();
}

class AudioOutputI2SDAC : public AudioOutput {
public:
  AudioOutputI2SDAC() {
    hertz = 22050;
    bps = 16;
    channels = 1;
    gainF2P6 = 64;
  }

  virtual ~AudioOutputI2SDAC() {
    stop();
  }

  virtual bool begin() override {
    m_bufCount = 0;
    m_bufBytePos = 0;
    m_peakAbs = 0;
    m_peakFrames = 0;
    if (!audio_i2s_install(hertz > 0 ? (uint32_t)hertz : 22050UL)) {
      return false;
    }
    LOG_AUDIO("AudioOutputI2SDAC: I2S0-DMA 启动 (In=%d Hz -> DAC=%u Hz, EQ=%s)",
              hertz, (unsigned)((hertz <= 48000) ? (hertz * 2) : hertz),
              audio_get_eq_name(s_eqMode));
    return true;
  }

  virtual bool SetRate(int hz) override {
    if (hz < 8000 || hz > 96000) hz = 22050;
    this->hertz = hz;
    audio_i2s_set_rate((uint32_t)hz);
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
    if (!s_i2sStreamInstalled) return false;
    if (!flushPending()) return false;

    MakeSampleStereo16(sample);
    int32_t mono = ((int32_t)sample[0] + (int32_t)sample[1]) >> 1;

    int32_t absVal = (mono < 0) ? -mono : mono;
    if (absVal > m_peakAbs) m_peakAbs = absVal;
    if (++m_peakFrames >= 64) {
      uint8_t instLevel = (uint8_t)constrain((m_peakAbs * 100) / 24000, 0, 100);
      s_streamLevel = (uint8_t)((s_streamLevel * 2 + instLevel * 2) / 4);
      m_peakAbs = 0;
      m_peakFrames = 0;
    }

    int32_t gainScale = ((int32_t)gainF2P6 * 230) >> 6;
    if (gainScale > 256) gainScale = 256;
    const bool oversample = (hertz <= 48000);
    dsp_push_mono_frame(mono, gainScale, oversample, m_buf, m_bufCount);

    if (m_bufCount >= 60) {
      flushPending();
    }
    return true;
  }

  virtual bool stop() override {
    flushPending();
    m_bufCount = 0;
    m_bufBytePos = 0;
    m_peakAbs = 0;
    m_peakFrames = 0;
    audio_i2s_uninstall();
    return true;
  }

private:
  bool flushPending() {
    if (m_bufCount == 0) return true;
    const size_t totalBytes = m_bufCount * sizeof(uint32_t);
    while (m_bufBytePos < totalBytes) {
      size_t written = 0;
      i2s_write(I2S_NUM_0, ((const uint8_t*)m_buf) + m_bufBytePos,
                totalBytes - m_bufBytePos, &written, 0);
      m_bufBytePos += written;
      if (written == 0) return false;
    }
    m_bufCount = 0;
    m_bufBytePos = 0;
    return true;
  }

  uint32_t m_buf[64] = {0};
  size_t m_bufCount = 0;
  size_t m_bufBytePos = 0;
  int32_t m_peakAbs = 0;
  uint8_t m_peakFrames = 0;
};

// 待机中点偏置：功放关闭或待机时，将 DAC2 稳定钳位在 1.65V 中点
static void audio_hold_idle_bias() {
  if (s_isPlayingAudio || s_isStreamingAudio || s_i2sStreamInstalled) return;
  dac_ll_digi_enable_dma(false);
  dac_output_enable(DAC_CHANNEL_2);
  dac_output_voltage(DAC_CHANNEL_2, 128);
}

void audio_init() {
  LOG_AUDIO("初始化统一 I2S0-DMA DAC2 音频子系统 (GPIO 26 -> 8002 功放, 绝不干涉 GPIO 25 触控)...");

  amp_init();

  s_out = new AudioOutputI2SDAC();
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

void audio_set_eq(AudioEQMode mode) {
  if (mode > AUDIO_EQ_CLEAR_VOCAL) mode = AUDIO_EQ_FLAT;
  s_eqMode = mode;
  s_eqLpState = 0;
  s_eqMidLpState = 0;
  LOG_AUDIO("音效 EQ 切换: %s", audio_get_eq_name(s_eqMode));
}

AudioEQMode audio_get_eq() {
  return s_eqMode;
}

AudioEQMode audio_cycle_eq() {
  AudioEQMode next = (AudioEQMode)(((uint8_t)s_eqMode + 1) % 3);
  audio_set_eq(next);
  return next;
}

const char* audio_get_eq_name(AudioEQMode mode) {
  switch (mode) {
    case AUDIO_EQ_WARM_BASS:   return "低音";
    case AUDIO_EQ_CLEAR_VOCAL: return "人声";
    default:                   return "原声";
  }
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
  if (s_isStreamingAudio) {
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
  if (s_i2sStreamInstalled) {
    audio_i2s_uninstall();
  }

  amp_disable();
  s_isPlayingAudio = false;
  s_isMelody = false;
  s_isMp3 = false;
  s_streamLevel = 0;
  audio_hold_idle_bias();
}

void audio_test_beep(uint16_t freq, uint16_t duration_ms) {
  if (s_isPlayingAudio || s_isStreamingAudio || s_i2sStreamInstalled) return;
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
// 复用统一 I2S0-DMA + 2x 过采样 + DC 消除 + EQ + 软限幅 + Sigma-Delta 引擎
// ==============================================================================
void audio_stream_start(uint32_t sample_rate) {
  audio_stop();
  if (!s_streamMutex) {
    s_streamMutex = xSemaphoreCreateMutexStatic(&s_streamMutexBuf);
  }

  if (!audio_i2s_install(sample_rate)) return;
  s_isStreamingAudio = true;

  LOG_AUDIO("蓝牙 I2S0-DMA 高清音频流已启动 (In=%lu Hz -> DAC=%lu Hz, EQ=%s)",
            (unsigned long)s_streamRate,
            (unsigned long)((s_streamRate <= 48000) ? (s_streamRate * 2) : s_streamRate),
            audio_get_eq_name(s_eqMode));
}

void audio_stream_set_rate(uint32_t sample_rate) {
  if (sample_rate < 8000 || sample_rate > 96000) return;
  if (sample_rate == s_streamRate) return;
  if (s_i2sStreamInstalled && s_streamMutex &&
      xSemaphoreTake(s_streamMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    audio_i2s_set_rate(sample_rate);
    xSemaphoreGive(s_streamMutex);
  } else {
    s_streamRate = sample_rate;
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

    dsp_push_mono_frame(mono, gainScale, oversample, s_streamBuf, outIdx);

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
    audio_i2s_uninstall();
    xSemaphoreGive(s_streamMutex);
  } else {
    audio_i2s_uninstall();
  }

  amp_disable();
  audio_hold_idle_bias();
  LOG_AUDIO("蓝牙 I2S0-DMA 音频流已停止");
}

uint8_t audio_stream_get_level() {
  if (!s_isStreamingAudio && !s_isPlayingAudio) return 0;
  return s_streamLevel;
}
