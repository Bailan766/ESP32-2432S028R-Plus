#include "audio.h"
#include "amplifier.h"
#include "config.h"
#include <AudioFileSourceSD.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>
#include <driver/dac.h>
#include "../demo_music.h"

// 专为 ESP32-2432S028R-Plus (CYD Plus) 硬件定制的音频输出驱动
// CYD 的板载扬声器/功放连接在 GPIO 26 (内部 DAC2 / 左声道)
// 而 GPIO 25 则是电阻触摸屏 XPT2046 独立软件 SPI 的关键时钟引脚 TOUCH_SCK_PIN！
// 标准 ESP8266Audio::AudioOutputI2S(0, INTERNAL_DAC) 默认调用 i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN)，
// 会强行霸占 GPIO 25 和 GPIO 26 作为模拟输出引脚，导致 GPIO 25 无法再输出数字 SPI 时钟脉冲，
// 从而导致播放音乐/BGM 时触控完全失灵、死机且无法退出！
// AudioOutputCYD 继承并覆写 begin/stop，强制限定 DAC 仅使用左声道(DAC2, GPIO 26)，彻底释放 GPIO 25！
class AudioOutputCYD : public AudioOutputI2S {
public:
  AudioOutputCYD(int port = 0) : AudioOutputI2S(port, INTERNAL_DAC) {
    SetOutputModeMono(true); // 左右声道自动混音至单声道，确保通过 DAC2(GPIO 26) 听到完整左右声道声音
  }

  virtual bool begin() override {
    bool res = AudioOutputI2S::begin(true);
    if (res) {
      // 关键修复：强制将 DAC 模式设置为仅左声道 (DAC2, GPIO 26)，并关闭 DAC1 (GPIO 25)
      i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
      restoreTouchPin();
      LOG_AUDIO("AudioOutputCYD: 已成功锁定仅启用 DAC2(GPIO 26)，彻底释放 GPIO 25(TOUCH_SCK) 给触控！");
    }
    return res;
  }

  virtual bool stop() override {
    bool res = AudioOutputI2S::stop();
    restoreTouchPin();
    dac_output_disable(DAC_CHANNEL_2);
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
static AudioFileSourcePROGMEM *s_fileProgmem = nullptr;
static AudioGeneratorMP3 *s_mp3 = nullptr;
static AudioGeneratorWAV *s_wav = nullptr;
static AudioOutputCYD *s_out = nullptr;

static bool s_isPlayingAudio = false;
static bool s_isWav = false;

void audio_init() {
  LOG_AUDIO("初始化音频子系统 (内部 DAC2/GPIO26 单声道, 释放 GPIO25 触摸时钟)...");
  
  amp_init();
  AudioOutputCYD::restoreTouchPin();
  
  s_out = new AudioOutputCYD(0);
  s_mp3 = new AudioGeneratorMP3();
  s_wav = new AudioGeneratorWAV();
  s_file = new AudioFileSourceSD();
  s_fileProgmem = new AudioFileSourcePROGMEM(demo_wav, demo_wav_len);
  
  audio_set_volume(s_volume);
}

void audio_set_volume(uint8_t volume_percent) {
  s_volume = constrain(volume_percent, 0, 100);
  LOG_AUDIO("音量全局设置: %d%%", s_volume);
  if (s_out) {
    float gain = s_volume / 100.0f;
    s_out->SetGain(gain);
  }
}

uint8_t audio_get_volume() {
  return s_volume;
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
    return false;
  }
  AudioOutputCYD::restoreTouchPin();
  s_isPlayingAudio = true;
  s_isWav = false;
  LOG_AUDIO("开始播放MP3: %s", filepath);
  return true;
}

bool audio_play_demo_wav() {
  audio_stop();
  s_fileProgmem->close();
  s_fileProgmem->open(demo_wav, demo_wav_len);
  
  amp_enable();
  if (!s_wav->begin(s_fileProgmem, s_out)) {
    LOG_AUDIO("Demo WAV解码器启动失败");
    s_fileProgmem->close();
    amp_disable();
    s_isPlayingAudio = false;
    AudioOutputCYD::restoreTouchPin();
    return false;
  }
  AudioOutputCYD::restoreTouchPin();
  s_isPlayingAudio = true;
  s_isWav = true;
  LOG_AUDIO("开始播放内部闪存 Demo WAV");
  return true;
}

void audio_pause() {
  if (s_isPlayingAudio) {
    s_out->stop();
    amp_disable();
    AudioOutputCYD::restoreTouchPin();
  }
  s_isPlayingAudio = false;
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
  if (s_mp3 && s_mp3->isRunning()) s_mp3->stop();
  if (s_wav && s_wav->isRunning()) s_wav->stop();
  if (s_file && s_file->isOpen()) s_file->close();
  if (s_fileProgmem && s_fileProgmem->isOpen()) s_fileProgmem->close();
  
  amp_disable();
  AudioOutputCYD::restoreTouchPin();
  s_isPlayingAudio = false;
}

void audio_test_beep(uint16_t freq, uint16_t duration_ms) {
  // Ignored in media mode
}

void audio_update() {
  if (s_isPlayingAudio) {
    if (s_isWav) {
      if (s_wav && s_wav->isRunning()) {
        if (!s_wav->loop()) {
          s_wav->stop();
          amp_disable();
          AudioOutputCYD::restoreTouchPin();
          s_isPlayingAudio = false;
          LOG_AUDIO("WAV 播放结束");
        }
      }
    } else {
      if (s_mp3 && s_mp3->isRunning()) {
        if (!s_mp3->loop()) {
          s_mp3->stop();
          s_file->close();
          amp_disable();
          AudioOutputCYD::restoreTouchPin();
          s_isPlayingAudio = false;
          LOG_AUDIO("MP3 播放结束");
        }
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
