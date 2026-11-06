#pragma once
#include <Arduino.h>
#include "config.h"

class AudioOutput;
AudioOutput* audio_get_output();

void audio_init();
void audio_set_volume(uint8_t volume_percent);
uint8_t audio_get_volume();

// 音频播放核心接口
bool audio_play_mp3(const char* filepath);
bool audio_play_demo(int songIndex);
bool audio_play_demo_wav(); // 兼容历史调用，默认播放第一首
void audio_pause();
void audio_resume();
void audio_stop();
void audio_test_beep(uint16_t freq, uint16_t duration_ms);
void audio_update(); // 在 loop() 中高频调用

bool audio_is_playing();
uint32_t audio_get_mp3_position(); // 秒
uint32_t audio_get_mp3_duration(); // 秒
uint16_t audio_get_current_freq(); // 当前播放音符的基频 (用于频谱/RGB律动)
