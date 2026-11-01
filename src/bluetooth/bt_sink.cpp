#include "bt_sink.h"
#include "hardware/amplifier.h"
#include "hardware/audio.h"
#include "player/player.h"
#include "config.h"
#include <BluetoothA2DPSink.h>
#include <driver/i2s.h>
#include <driver/dac.h>

static BluetoothA2DPSink* s_a2dpSink = nullptr;
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
    amp_enable();
  } else if (state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
    s_state = BT_STATE_CONNECTING;
  } else {
    s_state = BT_STATE_STANDBY;
    strcpy(s_title, "等待连接...");
    strcpy(s_artist, "手机蓝牙搜索: Bailan-Speaker");
    s_album[0] = '\0';
  }
}

static void avrc_playstatus_cb(esp_avrc_playback_stat_t playback) {
  if (playback == ESP_AVRC_PLAYBACK_PLAYING) {
    s_state = BT_STATE_PLAYING;
    amp_enable();
  } else {
    if (s_state == BT_STATE_PLAYING) {
      s_state = BT_STATE_CONNECTED;
    }
  }
}

static void on_data_received_cb() {
  s_lastDataTime = millis();
  if (s_state != BT_STATE_PLAYING) {
    s_state = BT_STATE_PLAYING;
    amp_enable();
  }
}

void bt_sink_init() {
  s_isActive = false;
  s_state = BT_STATE_STANDBY;
}

void bt_sink_start() {
  if (s_isActive) return;

  // 1. 停止本体 MP3 播放，避免与蓝牙 I2S/DAC 资源冲突
  player_stop();
  amp_enable();

  if (!s_a2dpSink) {
    s_a2dpSink = new BluetoothA2DPSink();
  }

  // 2. 配置 I2S DAC 内置模式 (只开启 GPIO26 上的 DAC 通道2，完整保留 GPIO25 给触屏 SPI CLK)
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
    .sample_rate = 44100,
    .bits_per_sample = (i2s_bits_per_sample_t)16,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };
  s_a2dpSink->set_i2s_config(i2s_config);

  // 3. 注册 AVRCP 元数据与状态回调
  s_a2dpSink->set_avrc_metadata_callback(avrc_metadata_cb);
  s_a2dpSink->set_on_connection_state_changed(connection_state_cb);
  s_a2dpSink->set_avrc_rn_playstatus_callback(avrc_playstatus_cb);
  s_a2dpSink->set_on_data_received(on_data_received_cb);

  // 4. 启动蓝牙 A2DP 广播服务
  s_a2dpSink->start(s_deviceName);

  s_isActive = true;
  s_state = BT_STATE_STANDBY;
  strcpy(s_title, "等待手机连接...");
  strcpy(s_artist, "手机蓝牙搜索: Bailan-Speaker");
  s_album[0] = '\0';
}

void bt_sink_stop() {
  if (!s_isActive) return;

  if (s_a2dpSink) {
    s_a2dpSink->end(true); // 释放 I2S 与 Bluedroid 堆内存
    delete s_a2dpSink;
    s_a2dpSink = nullptr;
  }

  amp_disable();

  // 恢复 DAC2 中点偏置（避免喇叭待机直流偏置）
  dac_output_enable(DAC_CHANNEL_2);
  dacWrite(AUDIO_DAC_PIN, 128);

  s_isActive = false;
  s_state = BT_STATE_STANDBY;
}

bool bt_sink_is_active() {
  return s_isActive;
}

BTSinkState bt_sink_get_state() {
  // 若超过 1.5 秒无数据流入且状态是播放中，切回已连接待机
  if (s_state == BT_STATE_PLAYING && (millis() - s_lastDataTime > 1500)) {
    s_state = BT_STATE_CONNECTED;
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
