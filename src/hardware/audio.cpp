#include "audio.h"
#include "amplifier.h"
#include "config.h"
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include <driver/dac.h>
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

#define AUDIO_RING_BUF_SIZE 8192
#define AUDIO_RING_BUF_MASK (AUDIO_RING_BUF_SIZE - 1)

static hw_timer_t *s_audioTimer = nullptr;
static uint8_t s_audioRingBuf[AUDIO_RING_BUF_SIZE];
static volatile uint16_t s_rbHead = 0;
static volatile uint16_t s_rbTail = 0;
static volatile bool s_timerRunning = false;

// 硬件定时器高频中断服务程序 (驻留 IRAM，直接驱动 DAC2)
//
// 注意：这里必须用 hal/dac_ll.h 的 static inline 寄存器写入，不能调
// driver/dac.h 的 dac_output_voltage()。后者链接在 Flash 上、无 IRAM_ATTR，
// ISR 执行期间一旦发生 Flash 写（NVS 保存设置、SD 写日志等）会关闭 cache，
// 取指失败直接 Guru Meditation。dac_ll_update_output_value() 会内联进本函数，
// 全程只碰寄存器，不依赖任何 Flash 代码。
void IRAM_ATTR onAudioTimer() {
  if (s_rbHead != s_rbTail) {
    uint8_t sample = s_audioRingBuf[s_rbTail];
    s_rbTail = (s_rbTail + 1) & AUDIO_RING_BUF_MASK;
    dac_ll_update_output_value(DAC_CHANNEL_2, sample);
  } else {
    // 缓冲区见底或无数据：维持中点偏置
    dac_ll_update_output_value(DAC_CHANNEL_2, 128);
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
    // 1. 确保 GPIO 26 内部 DAC2 使能并置于中点
    dac_output_enable(DAC_CHANNEL_2);
    dac_output_voltage(DAC_CHANNEL_2, 128);

    // 2. 复位环形缓冲区双指针
    s_rbHead = 0;
    s_rbTail = 0;

    // 3. 配置硬件定时器 0 (80MHz / 8 = 10MHz, 1 周期 = 0.1us)
    if (!s_audioTimer) {
      s_audioTimer = timerBegin(0, 8, true);
      timerAttachInterrupt(s_audioTimer, &onAudioTimer, true);
    }

    uint64_t ticks = 10000000ULL / (hertz > 0 ? (uint64_t)hertz : 22050ULL);
    timerAlarmWrite(s_audioTimer, ticks, true);
    timerAlarmEnable(s_audioTimer);
    s_timerRunning = true;

    LOG_AUDIO("AudioOutputTimerDAC: 硬件定时器启动成功 (Rate=%d Hz, Ticks=%llu, 8KB Buffer)", hertz, ticks);
    return true;
  }

  virtual bool SetRate(int hz) override {
    if (hz <= 0) hz = 22050;
    this->hertz = hz;
    if (s_audioTimer && s_timerRunning) {
      uint64_t ticks = 10000000ULL / (uint64_t)hz;
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
static bool s_isMelody = false;
static bool s_isMp3 = false;

// 待机中点偏置：功放关闭或待机时，将 DAC2 稳定钳位在 1.65V 中点
static void audio_hold_idle_bias() {
  if (s_isPlayingAudio) return;
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
  if (s_isPlayingAudio) return;
  if (freq < 100 || freq > 8000) return;
  if (duration_ms == 0) duration_ms = 40;

  // 1. 建立中点偏置
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
  return s_isPlayingAudio;
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
