#include "audio.h"
#include "amplifier.h"
#include "config.h"
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>
#include <driver/dac.h>
#include <math.h>
#include "player/melody_generator.h"
#include "songs.h"

// ==============================================================================
// 专为 ESP32-2432S028R-Plus (CYD Plus) 硬件定制的 I2S-DAC 音频驱动 (AudioOutputCYD)
//
// 硬件背景：
//   1. CYD Plus 的板载音频接口 CN1 经 8002 功放连接到 GPIO 26 (内部 DAC2 / 左声道)；
//   2. GPIO 25 则是电阻触摸屏 XPT2046 独立软件 SPI 的关键时钟引脚 TOUCH_SCK_PIN！
//   3. 原生 ESP8266Audio::AudioOutputI2S(0, INTERNAL_DAC) 默认会调用
//      i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN)，强行霸占 GPIO 25，导致触摸完全失效。
//
// 本驱动机制：
//   - 继承 AudioOutputI2S，配置 16 个 DMA 缓冲块 (约 46.4ms 硬件缓冲，彻底消灭 UI/触控阻塞卡顿)；
//   - 覆写 begin 与 SetRate，强制调用 i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN)，
//     将 DAC 严格限定在 GPIO 26，彻底释放 GPIO 25 给触摸屏软件 SPI；
//   - 开启单声道混音 SetOutputModeMono(true)，左右声道完美合并至 DAC2。
// ==============================================================================

class AudioOutputCYD : public AudioOutputI2S {
public:
  AudioOutputCYD(int port = 0) : AudioOutputI2S(port, INTERNAL_DAC, 16) {
    SetOutputModeMono(true);
  }

  virtual bool begin() override {
    // 启动前先将 DAC2 偏置稳定在中点，消除功放开启时的爆音
    dac_output_enable(DAC_CHANNEL_2);
    dacWrite(AUDIO_DAC_PIN, 128);
    delayMicroseconds(500);

    bool res = AudioOutputI2S::begin(true);
    if (res) {
      // 核心锁死：仅启用 DAC2 (GPIO 26)，杜绝占用 GPIO 25
      i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
      restoreTouchPin();
      LOG_AUDIO("AudioOutputCYD: 已成功锁定 DAC2(GPIO 26)，彻底释放 GPIO 25(TOUCH_SCK) 给触摸！");
    }
    return res;
  }

  virtual bool SetRate(int hz) override {
    bool res = AudioOutputI2S::SetRate(hz);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
    restoreTouchPin();
    return res;
  }

  virtual bool stop() override {
    bool res = AudioOutputI2S::stop();
    restoreTouchPin();
    dac_output_enable(DAC_CHANNEL_2);
    dacWrite(AUDIO_DAC_PIN, 128);
    return res;
  }

  static void restoreTouchPin() {
    dac_output_disable(DAC_CHANNEL_1);
    pinMode(TOUCH_SCK_PIN, OUTPUT);
    digitalWrite(TOUCH_SCK_PIN, LOW);
  }
};

static uint8_t s_volume = 70; // 默认 70%

static AudioFileSourceSD *s_file = nullptr;
static AudioGeneratorMP3 *s_mp3 = nullptr;
static AudioGeneratorMelody *s_melody = nullptr;
static AudioOutputCYD *s_out = nullptr;

static bool s_isPlayingAudio = false;
static bool s_isMelody = false;
static bool s_isMp3 = false;

// 待机中点偏置：功放关闭或待机时，将 DAC2 稳定钳位在中点
static void audio_hold_idle_bias() {
  if (s_isPlayingAudio) return;
  dac_output_enable(DAC_CHANNEL_2);
  dacWrite(AUDIO_DAC_PIN, 128);
}

void audio_init() {
  LOG_AUDIO("初始化音频子系统 (内部 DAC2/GPIO26 单声道, 释放 GPIO25 触摸时钟)...");

  amp_init();
  AudioOutputCYD::restoreTouchPin();

  s_out = new AudioOutputCYD(0);
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
    // 0.85 峰值增益系数，防止大动态音频造成 8002 削顶失真与喇叭共振
    s_out->SetGain((s_volume / 100.0f) * 0.85f);
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
    AudioOutputCYD::restoreTouchPin();
    audio_hold_idle_bias();
    return false;
  }

  AudioOutputCYD::restoreTouchPin();
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
    AudioOutputCYD::restoreTouchPin();
    audio_hold_idle_bias();
    return false;
  }

  AudioOutputCYD::restoreTouchPin();
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
    AudioOutputCYD::restoreTouchPin();
  }
  s_isPlayingAudio = false;
  audio_hold_idle_bias();
}

void audio_resume() {
  if (!s_isPlayingAudio) {
    amp_enable();
    s_out->begin();
    AudioOutputCYD::restoreTouchPin();
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
  AudioOutputCYD::restoreTouchPin();
  s_isPlayingAudio = false;
  s_isMelody = false;
  s_isMp3 = false;
  audio_hold_idle_bias();
}

void audio_test_beep(uint16_t freq, uint16_t duration_ms) {
  if (s_isPlayingAudio) return;
  if (duration_ms < 60) return; // 过滤过短的按键点击音，避免频繁开关功放产生爆音
  if (freq < 100 || freq > 8000) return;

  // 1. 建立中点偏置
  dac_output_enable(DAC_CHANNEL_2);
  dacWrite(AUDIO_DAC_PIN, 128);
  delay(5);
  amp_enable();
  delay(5);

  const uint32_t periodUs = 1000000UL / freq;
  const uint32_t stepUs = periodUs / 16;
  if (stepUs > 0) {
    const unsigned long start = millis();
    float phase = 0.0f;
    const float phaseStep = (2.0f * (float)M_PI) / 16.0f;

    while (millis() - start < duration_ms) {
      for (int i = 0; i < 16; i++) {
        uint8_t sample = (uint8_t)(128 + (int)(55.0f * sinf(phase)));
        dacWrite(AUDIO_DAC_PIN, sample);
        phase += phaseStep;
        if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        delayMicroseconds(stepUs);
      }
    }
  }

  amp_disable();
  delay(5);
  dacWrite(AUDIO_DAC_PIN, 128);
}

void audio_update() {
  if (!s_isPlayingAudio) return;

  if (s_isMelody) {
    if (s_melody && s_melody->isRunning()) {
      if (!s_melody->loop()) {
        s_melody->stop();
        s_out->stop();
        amp_disable();
        AudioOutputCYD::restoreTouchPin();
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
        AudioOutputCYD::restoreTouchPin();
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
