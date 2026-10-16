#pragma once
#include <Arduino.h>
#include "config.h"

void audio_init();
void audio_set_volume(uint8_t volume_percent);
uint8_t audio_get_volume();

// MP3/WAV Playback Interface
bool audio_play_mp3(const char* filepath);
bool audio_play_demo_wav();
void audio_pause();
void audio_resume();
void audio_stop();
void audio_test_beep(uint16_t freq, uint16_t duration_ms);
void audio_update(); // Call constantly in loop()

bool audio_is_playing();
uint32_t audio_get_mp3_position(); // seconds
uint32_t audio_get_mp3_duration(); // seconds
