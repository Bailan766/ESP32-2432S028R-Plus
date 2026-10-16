#pragma once
#include <Arduino.h>

enum PlayerState {
  STATE_STOPPED,
  STATE_LOADING,
  STATE_PLAYING,
  STATE_PAUSED,
  STATE_SEEKING,
  STATE_FINISHED,
  STATE_ERROR
};

enum PlayMode {
  MODE_SEQUENTIAL,  // 顺序播放
  MODE_REPEAT_ONE,  // 单曲循环
  MODE_REPEAT_ALL,  // 全部循环
  MODE_SHUFFLE      // 随机播放
};

const char* player_state_to_string(PlayerState state);
const char* play_mode_to_string(PlayMode mode);
