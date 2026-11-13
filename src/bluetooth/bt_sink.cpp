#include "bt_sink.h"
#include "hardware/amplifier.h"
#include "hardware/audio.h"
#include "player/player.h"
#include "system/net_manager.h"
#include "system/settings_manager.h"
#include "config.h"
#include <BluetoothA2DPSink.h>
#include <A2DPVolumeControl.h>

static BluetoothA2DPSink* s_a2dpSink = nullptr;
static A2DPNoVolumeControl s_noVolCtrl;
static bool s_isActive = false;
static BTSinkState s_state = BT_STATE_STANDBY;

static char s_title[64] = "等待音频流...";
static char s_artist[64] = "手机搜索 Bailan-Speaker";
static char s_album[64] = "";
static const char s_deviceName[] = "Bailan-Speaker";

static unsigned long s_lastDataTime = 0;
static uint8_t s_volume = 100; // 0..100

static void avrc_metadata_cb(uint8_t id, const uint8_t *text) {
  if (text == nullptr) return;
  if (id == ESP_AVRC_MD_ATTR_TITLE) {
    strncpy(s_title, (const char*)text, sizeof(s_title) - 1);
    s_title[sizeof(s_title) - 1] = '\0';
  } else if (id == ESP_AVRC_MD_ATTR_ARTIST) {
    strncpy(s_artist, (const char*)text, sizeof(s_artist) - 1);
    s_artist[sizeof(s_artist) - 1] = '\0';
  } else if (id == ESP_AVRC_MD_ATTR_ALBUM) {
    strncpy(s_album, (const char*)text, sizeof(s_album) - 1);
    s_album[sizeof(s_album) - 1] = '\0';
  }
}

static void connection_state_cb(esp_a2d_connection_state_t state, void * /*obj*/) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    s_state = BT_STATE_CONNECTED;
  } else if (state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
    s_state = BT_STATE_CONNECTING;
  } else {
    s_state = BT_STATE_STANDBY;
    amp_disable();
    strcpy(s_title, "等待连接...");
    strcpy(s_artist, "手机蓝牙搜索: Bailan-Speaker");
    s_album[0] = '\0';
  }
}

static void avrc_playstatus_cb(esp_avrc_playback_stat_t playback) {
  if (playback == ESP_AVRC_PLAYBACK_PLAYING) {
    s_state = BT_STATE_PLAYING;
  } else {
    if (s_state == BT_STATE_PLAYING) {
      s_state = BT_STATE_CONNECTED;
    }
    amp_disable();
  }
}

static void bt_sample_rate_cb(uint16_t rate) {
  audio_stream_set_rate(rate);
}

static void bt_volume_change_cb(int vol) {
  if (vol < 0) vol = 0;
  if (vol > 127) vol = 127;
  s_volume = (uint8_t)((vol * 100 + 63) / 127);
}

static void bt_pcm_stream_cb(const uint8_t* data, uint32_t len) {
  if (!s_isActive || !data || len < 4) return;

  const int16_t* pcm = (const int16_t*)data;
  const uint32_t sampleCount = len / 2;
  int32_t peak = 0;
  for (uint32_t i = 0; i < sampleCount; i++) {
    int32_t v = pcm[i];
    if (v < 0) v = -v;
    if (v > peak) {
      peak = v;
      if (peak > 2) break;
    }
  }

  if (peak > 2) {
    s_lastDataTime = millis();
    if (s_state != BT_STATE_PLAYING) {
      s_state = BT_STATE_PLAYING;
    }
    amp_enable();
  }

  uint8_t sysVol = settings_get().volume;
  uint8_t effectiveVol = (uint8_t)(((uint32_t)s_volume * sysVol) / 100U);
  audio_stream_write_pcm16_stereo(data, len, effectiveVol);
}

void bt_sink_init() {
  s_isActive = false;
  s_state = BT_STATE_STANDBY;
}

void bt_sink_start() {
  if (s_isActive) return;

  // 1. 挂起 Wi-Fi 射频释放 2.4GHz 天线，停止本体 MP3 播放，启动硬件定时器 DAC2 流引擎
  net_suspend_for_bt();
  player_stop();
  audio_stream_start(44100);
  amp_disable(); // 待机/配对期间保持功放关闭，杜绝射频耦合嗡嗡底噪

  if (!s_a2dpSink) {
    s_a2dpSink = new BluetoothA2DPSink();
  }

  // 2. 彻底禁用 ESP32-A2DP 内置 I2S 驱动 (is_i2s=false)，避免占用 GPIO25 触控时钟及 I2S-DAC DMA 嗡嗡声
  s_a2dpSink->set_stream_reader(bt_pcm_stream_cb, false);
  s_a2dpSink->set_sample_rate_callback(bt_sample_rate_cb);
  s_a2dpSink->set_volume_control(&s_noVolCtrl);
  s_a2dpSink->set_on_volumechange(bt_volume_change_cb);

  // 3. 注册 AVRCP 元数据与状态回调
  s_a2dpSink->set_avrc_metadata_callback(avrc_metadata_cb);
  s_a2dpSink->set_on_connection_state_changed(connection_state_cb);
  s_a2dpSink->set_avrc_rn_playstatus_callback(avrc_playstatus_cb);

  s_isActive = true;
  s_state = BT_STATE_STANDBY;
  s_lastDataTime = 0;
  strcpy(s_title, "等待手机连接...");
  strcpy(s_artist, "手机蓝牙搜索: Bailan-Speaker");
  s_album[0] = '\0';

  // 4. 启动蓝牙 A2DP 广播服务
  s_a2dpSink->start(s_deviceName);
}

void bt_sink_stop() {
  if (!s_isActive) return;

  s_isActive = false;
  if (s_a2dpSink) {
    s_a2dpSink->end(true); // 释放 Bluedroid 堆内存
    delete s_a2dpSink;
    s_a2dpSink = nullptr;
  }

  audio_stream_stop();
  amp_disable();
  net_resume_after_bt();

  s_state = BT_STATE_STANDBY;
}

bool bt_sink_is_active() {
  return s_isActive;
}

BTSinkState bt_sink_get_state() {
  // 若超过 1500ms 无有效音频流且状态是播放中，关闭功放并切回已连接待机（避免弱音/句间间隙误关断咔嗒声）
  if (s_state == BT_STATE_PLAYING && (millis() - s_lastDataTime > 1500)) {
    s_state = BT_STATE_CONNECTED;
    amp_disable();
  }
  return s_state;
}

const char* bt_sink_get_device_name() {
  return s_deviceName;
}

const char* bt_sink_get_title() {
  return s_title;
}

const char* bt_sink_get_artist() {
  return s_artist;
}

const char* bt_sink_get_album() {
  return s_album;
}

void bt_sink_play() {
  if (s_a2dpSink && s_isActive) s_a2dpSink->play();
}

void bt_sink_pause() {
  if (s_a2dpSink && s_isActive) s_a2dpSink->pause();
}

void bt_sink_play_pause() {
  if (!s_a2dpSink || !s_isActive) return;
  if (s_state == BT_STATE_PLAYING) {
    s_a2dpSink->pause();
  } else {
    s_a2dpSink->play();
  }
}

void bt_sink_next() {
  if (s_a2dpSink && s_isActive) s_a2dpSink->next();
}

void bt_sink_prev() {
  if (s_a2dpSink && s_isActive) s_a2dpSink->previous();
}

void bt_sink_set_volume(uint8_t vol) {
  if (vol > 100) vol = 100;
  s_volume = vol;
  if (s_a2dpSink && s_isActive) {
    s_a2dpSink->set_volume((uint8_t)(vol * 127 / 100));
  }
}

uint8_t bt_sink_get_volume() {
  return s_volume;
}

bool bt_sink_has_audio_stream() {
  return (s_isActive && s_state == BT_STATE_PLAYING && (millis() - s_lastDataTime < 600));
}
