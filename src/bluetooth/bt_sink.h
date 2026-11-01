#pragma once
#include <Arduino.h>

enum BTSinkState {
  BT_STATE_STANDBY,     // 广播中，等待手机连接
  BT_STATE_CONNECTING,  // 正在连接
  BT_STATE_CONNECTED,   // 已连接手机，待机
  BT_STATE_PLAYING      // 正在播放手机音乐
};

void bt_sink_init();
void bt_sink_start();
void bt_sink_stop();
bool bt_sink_is_active();

BTSinkState bt_sink_get_state();
const char* bt_sink_get_device_name();
const char* bt_sink_get_title();
const char* bt_sink_get_artist();
const char* bt_sink_get_album();

void bt_sink_play();
void bt_sink_pause();
void bt_sink_play_pause();
void bt_sink_next();
void bt_sink_prev();
void bt_sink_set_volume(uint8_t vol);
uint8_t bt_sink_get_volume();

bool bt_sink_has_audio_stream();
