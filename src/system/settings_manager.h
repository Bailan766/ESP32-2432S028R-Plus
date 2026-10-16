#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "player/playback_state.h"

struct SystemSettings {
  uint8_t volume;            // 0 ~ 100
  uint8_t brightness;        // 10 ~ 100
  uint16_t autoSleepSeconds; // 0=关闭, 30, 60, 180, 300
  uint8_t playMode;          // PlayMode 枚举
  bool resumeEnable;         // 断点续播开关
  uint8_t screenRotation;    // 1 或 3 (默认 3: USB居左正向握持)
  bool bootCalibrate;        // 开机自动进入触屏校准 (默认 true, 适配安装外壳挤压)
  int lastTrackId;           // 上次播放的曲目 ID
  uint16_t lastPositionSec;  // 上次播放的进度秒数
};

void settings_init();
SystemSettings& settings_get();
void settings_save();
void settings_save_playback_position(int trackId, uint16_t positionSec);
