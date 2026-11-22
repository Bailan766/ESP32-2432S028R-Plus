#include "hardware/rgb.h"
#include "bt_speaker_ui.h"
#include "bluetooth/bt_sink.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "system/settings_manager.h"
#include "system/power_manager.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "ui/ui_theme.h"
#include "config.h"

static BTSinkState s_lastState = (BTSinkState)-1;
static char s_lastTitle[64] = "";
static char s_lastArtist[64] = "";
static unsigned long s_lastAnimUpdate = 0;
static uint8_t s_barHeights[16] = {0};

static void renderQuickBar() {
  TFT_eSPI& tft = display_get_tft();
  int y = 150, h = 26;
  tft.fillRect(14, y, 292, h, UI_COL_BG);

  // 音量 - / 数值 / +
  ui_theme_draw_badge(14, y, 46, h, "音-", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);
  char volBuf[12];
  snprintf(volBuf, sizeof(volBuf), "%d%%", settings_get().volume);
  ui_theme_draw_badge(62, y, 40, h, volBuf, M3_PRIMARY, UI_COL_SURFACE);
  ui_theme_draw_badge(104, y, 46, h, "音+", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);

  // 音效 EQ 切换
  char eqBuf[24];
  snprintf(eqBuf, sizeof(eqBuf), "音效:%s", audio_get_eq_name(audio_get_eq()));
  ui_theme_draw_badge(156, y, 86, h, eqBuf, M3_PRIMARY, M3_PRIMARY_CONTAINER);

  // 睡眠定时切换
  uint16_t remSec = power_get_sleep_timer_remaining_sec();
  char tmBuf[16];
  if (remSec > 0) {
    uint16_t remMin = (remSec + 59) / 60;
    snprintf(tmBuf, sizeof(tmBuf), "定%um", (unsigned)remMin);
    ui_theme_draw_badge(248, y, 58, h, tmBuf, M3_TERTIARY, M3_TERTIARY_CONTAINER);
  } else {
    ui_theme_draw_badge(248, y, 58, h, "定时:关", UI_COL_TEXT_MUTED, UI_COL_SURFACE);
  }
}

static void renderStaticUI() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("蓝牙音箱模式", true);

  // 1. 中央 M3 大圆角色调卡片 (x: 14, y: 34, w: 292, h: 112, r: 14, 无生硬边框)
  ui_theme_draw_panel(14, 34, 292, 112, 14, UI_COL_SURFACE, UI_COL_SURFACE);

  // 左侧 M3 有机蓝牙徽标底衬
  ui_theme_draw_icon_badge(38, 60, 28, M3_PRIMARY_CONTAINER, M3_PRIMARY, M3_ICON_BT, true);

  // 2. 快捷功能栏 (y: 150, h: 26): 音量 / 音效 / 睡眠定时
  renderQuickBar();

  // 3. 底部 M3 胶囊控制按钮 (x: 14..306, y: 182..228, 全半圆胶囊)
  ui_theme_draw_badge(14, 182, 64, 46, "|<<", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);
  ui_theme_draw_badge(86, 182, 96, 46, "播放/暂停", M3_ON_PRIMARY, M3_PRIMARY);
  ui_theme_draw_badge(190, 182, 64, 46, ">>|", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);
  ui_theme_draw_badge(262, 182, 44, 46, "重连", M3_TERTIARY, M3_TERTIARY_CONTAINER);

  s_lastState = (BTSinkState)-1;
  s_lastTitle[0] = '\0';
  s_lastArtist[0] = '\0';
}

static void renderDynamicInfo() {
  TFT_eSPI& tft = display_get_tft();
  BTSinkState st = bt_sink_get_state();
  const char* curTitle = bt_sink_get_title();
  const char* curArtist = bt_sink_get_artist();

  // 1. M3 状态胶囊徽章更新 (避开左侧蓝牙图标)
  if (st != s_lastState) {
    s_lastState = st;
    tft.fillRect(58, 40, 236, 22, UI_COL_SURFACE);

    uint16_t badgeFg = M3_TERTIARY;
    uint16_t badgeBg = M3_TERTIARY_CONTAINER;
    const char* statusStr = "等待手机连接";
    if (st == BT_STATE_CONNECTING) {
      badgeFg = M3_ON_PRIMARY;
      badgeBg = M3_HERO_START;
      statusStr = "正在握手配对";
    } else if (st == BT_STATE_CONNECTED) {
      badgeFg = M3_PRIMARY;
      badgeBg = M3_PRIMARY_CONTAINER;
      statusStr = "手机已连接";
    } else if (st == BT_STATE_PLAYING) {
      badgeFg = M3_ON_PRIMARY;
      badgeBg = M3_SAGE;
      statusStr = "正在播放音频";
    }

    ui_theme_draw_badge(60, 41, 118, 20, statusStr, badgeFg, badgeBg);
  }

  // 2. 设备名与粗体歌曲标题
  if (strcmp(curTitle, s_lastTitle) != 0 || strcmp(curArtist, s_lastArtist) != 0) {
    strncpy(s_lastTitle, curTitle, sizeof(s_lastTitle) - 1);
    strncpy(s_lastArtist, curArtist, sizeof(s_lastArtist) - 1);

    tft.fillRect(60, 64, 236, 40, UI_COL_SURFACE);
    font_draw_string_bold(60, 64, 234, curTitle, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "%s · %s", curArtist, bt_sink_get_device_name());
    font_draw_string_clipped(60, 84, 234, infoBuf, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
  }
}

static void renderSpectrumBars() {
  TFT_eSPI& tft = display_get_tft();
  bool isStreaming = bt_sink_has_audio_stream();
  uint8_t audioLevel = isStreaming ? audio_stream_get_level() : 0;

  const int NUM_BARS = 16;
  const int BAR_W = 12;
  const int BAR_GAP = 5;
  const int START_X = 22;
  const int BASE_Y = 140;
  const int MAX_H = 30;

  for (int i = 0; i < NUM_BARS; i++) {
    uint8_t targetH = 3;
    if (isStreaming) {
      // 结合真实 PCM 包络能量与频段权重生成动态频谱柱
      int baseH = 4 + (audioLevel * (MAX_H - 6)) / 100;
      int jitter = (rand() % 41) - 15; // -15% ~ +25% 频段起伏
      int bandScale = (i < 4) ? 105 : ((i < 12) ? 95 : 80);
      int hCalc = (baseH * (bandScale + jitter)) / 100;
      targetH = (uint8_t)constrain(hCalc, 3, MAX_H);
    }

    // 平滑滤波
    s_barHeights[i] = (s_barHeights[i] * 3 + targetH) / 4;
    int h = s_barHeights[i];
    if (h < 2) h = 2;
    if (h > MAX_H) h = MAX_H;

    int bx = START_X + i * (BAR_W + BAR_GAP);
    int by = BASE_Y - h;

    // 清除上一次绘制的高处残影
    tft.fillRect(bx, BASE_Y - MAX_H, BAR_W, MAX_H - h, COLOR_PANEL_BG);

    // M3 暖玫瑰色阶频谱柱
    uint16_t barCol = M3_PRIMARY_CONTAINER;
    if (h > 22) barCol = M3_HERO_START;
    else if (h > 14) barCol = M3_HERO_END;
    else if (h > 8) barCol = M3_PRIMARY;

    tft.fillRoundRect(bx, by, BAR_W, h, 3, barCol);
  }

  if (bt_sink_get_state() == BT_STATE_PLAYING) {
    uint8_t energy = constrain(map(audioLevel, 0, 100, 20, 255), 0, 255);
    rgb_beat_pulse(energy, 300 + audioLevel * 12);
  }
}

void bt_speaker_ui_enter() {
  bt_sink_start();
  renderStaticUI();
  renderDynamicInfo();
}

void bt_speaker_ui_update() {
  static unsigned long s_lastTimerCheck = 0;
  static uint16_t s_lastRemMin = 0;
  renderDynamicInfo();

  if (millis() - s_lastTimerCheck >= 2000) {
    s_lastTimerCheck = millis();
    uint16_t remMin = (power_get_sleep_timer_remaining_sec() + 59) / 60;
    if (remMin != s_lastRemMin) {
      s_lastRemMin = remMin;
      renderQuickBar();
    }
  }

  if (millis() - s_lastAnimUpdate >= 60) {
    s_lastAnimUpdate = millis();
    renderSpectrumBars();
  }
}

void bt_speaker_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮 (x: 0..76, y: 0..32)
  if (x <= 76 && y <= 32) {
    bt_speaker_ui_leave();
    audio_test_beep(1000, 30);
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 快捷功能栏 (y: 146..178)
  if (y >= 146 && y <= 178) {
    SystemSettings& cfg = settings_get();
    if (x >= 10 && x <= 60) {
      // [音-]
      int v = (int)cfg.volume - 10;
      if (v < 0) v = 0;
      cfg.volume = (uint8_t)v;
      audio_set_volume(cfg.volume);
      settings_save();
      renderQuickBar();
    } else if (x >= 102 && x <= 152) {
      // [音+]
      int v = (int)cfg.volume + 10;
      if (v > 100) v = 100;
      cfg.volume = (uint8_t)v;
      audio_set_volume(cfg.volume);
      settings_save();
      renderQuickBar();
    } else if (x >= 154 && x <= 244) {
      // [音效 EQ]
      AudioEQMode eq = audio_cycle_eq();
      cfg.eqMode = (uint8_t)eq;
      settings_save();
      renderQuickBar();
    } else if (x >= 246 && x <= 310) {
      // [睡眠定时]
      power_cycle_sleep_timer();
      renderQuickBar();
    }
    return;
  }

  // 3. 底部控制按钮 (y: 180..234)
  if (y >= 180 && y <= 234) {
    if (x >= 14 && x <= 78) {
      // [上一曲]
      audio_test_beep(1500, 30);
      bt_sink_prev();
    } else if (x >= 86 && x <= 182) {
      // [播放/暂停]
      audio_test_beep(1800, 30);
      bt_sink_play_pause();
    } else if (x >= 190 && x <= 254) {
      // [下一曲]
      audio_test_beep(1500, 30);
      bt_sink_next();
    } else if (x >= 262 && x <= 306) {
      // [重置/重连]
      audio_test_beep(800, 50);
      bt_sink_stop();
      delay(80);
      bt_sink_start();
      renderStaticUI();
      renderDynamicInfo();
    }
  }
}

void bt_speaker_ui_leave() {
  bt_sink_stop();
}
