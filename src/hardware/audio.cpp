#include "audio.h"
#include "amplifier.h"
#include <AudioFileSourceSD.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include "../demo_music.h"

static uint8_t s_volume = 70; // 默认 70%

static AudioFileSourceSD *s_file = nullptr;
static AudioFileSourcePROGMEM *s_fileProgmem = nullptr;
static AudioGeneratorMP3 *s_mp3 = nullptr;
static AudioGeneratorWAV *s_wav = nullptr;
static AudioOutputI2S *s_out = nullptr;

static bool s_isPlayingAudio = false;
static bool s_isWav = false;

void audio_init() {
  LOG_AUDIO("初始化音频子系统 (内部 DAC, MP3/WAV解码)...");
  
  amp_init();
  
  // ESP32内部DAC模式 (0: port, 1: INTERNAL_DAC)
  s_out = new AudioOutputI2S(0, 1);
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
  s_mp3->begin(s_file, s_out);
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
  s_wav->begin(s_fileProgmem, s_out);
  s_isPlayingAudio = true;
  s_isWav = true;
  LOG_AUDIO("开始播放内部闪存 Demo WAV");
  return true;
}

void audio_pause() {
  if (s_isPlayingAudio) {
    s_out->stop();
    amp_disable();
  }
  s_isPlayingAudio = false;
}

void audio_resume() {
  if (!s_isPlayingAudio) {
    amp_enable();
    s_out->begin();
    s_isPlayingAudio = true;
  }
}

void audio_stop() {
  if (s_mp3 && s_mp3->isRunning()) s_mp3->stop();
  if (s_wav && s_wav->isRunning()) s_wav->stop();
  if (s_file && s_file->isOpen()) s_file->close();
  if (s_fileProgmem && s_fileProgmem->isOpen()) s_fileProgmem->close();
  
  amp_disable();
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
