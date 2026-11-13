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

// 外部实时 PCM 音频流直通接口（用于蓝牙 A2DP 音响等，复用硬件定时器 0 + DAC2 环形缓冲）
void audio_stream_start(uint32_t sample_rate);
void audio_stream_set_rate(uint32_t sample_rate);
void audio_stream_write_pcm16_stereo(const uint8_t* data, uint32_t len, uint8_t gain_percent);
void audio_stream_stop();
uint8_t audio_stream_get_level(); // 0..100 实时音频能量峰值（供 UI 频谱/呼吸灯律动）
