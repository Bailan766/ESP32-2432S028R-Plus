#include "web_radio_ui.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "radio/web_radio.h"
#include "config.h"
#include <WiFi.h>

static unsigned long s_lastAnimUpdate = 0;
static int8_t s_wavePhase = 0;
static int s_lastStationIdx = -1;
static bool s_lastRadioPlaying = false;
static char s_lastStatusStr[48] = {0};

static void renderWaveBars() {
  TFT_eSPI& tft = display_get_tft();
  bool playing = web_radio_is_playing();
  int waveX = 180, waveY = 152;
  tft.fillRect(waveX, waveY - 22, 48, 24, COLOR_PANEL_BG);
  for (int i = 0; i < 6; i++) {
    int h = playing ? (4 + abs((int)(sin((s_wavePhase + i * 40) * 0.08f) * 16.0f))) : 3;
    tft.fillRect(waveX + i * 8, waveY - h, 5, h, playing ? COLOR_ACCENT_GREEN : 0x2965);
  }
}

static void renderStaticUI() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("网络收音机", true);

  // 1. 复古收音机调频刻度盘 (x: 16, y: 34, w: 288, h: 32)
  tft.fillRoundRect(16, 34, 288, 32, 4, 0x10A2);
  tft.drawRoundRect(16, 34, 288, 32, 4, COLOR_PANEL_BORDER);

  // 刻度线
  for (int i = 0; i < 27; i++) {
    int lx = 26 + i * 10;
    int lh = (i % 5 == 0) ? 12 : 6;
    tft.drawFastVLine(lx, 36, lh, (i % 5 == 0) ? COLOR_TEXT_PRIMARY : COLOR_TEXT_MUTED);
  }

  // 2. 主电台信息卡片底框 (x: 16, y: 72, w: 288, h: 96)
  tft.fillRoundRect(16, 72, 288, 96, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(16, 72, 288, 96, 6, COLOR_PANEL_BORDER);

  // 3. 底部三大控制按键 (y: 178, h: 46)
  // [上一台] (x: 16, w: 76)
  tft.fillRoundRect(16, 178, 76, 46, 6, 0x10A2);
  tft.drawRoundRect(16, 178, 76, 46, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(54, 192, "< 上台", COLOR_TEXT_PRIMARY, 0x10A2, 1);

  // [播放/暂停] (x: 100, w: 120)
  tft.fillRoundRect(100, 178, 120, 46, 6, 0x1A26);
  tft.drawRoundRect(100, 178, 120, 46, 6, COLOR_ACCENT_GREEN);

  // [下一台] (x: 228, w: 76)
  tft.fillRoundRect(228, 178, 76, 46, 6, 0x10A2);
  tft.drawRoundRect(228, 178, 76, 46, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(266, 192, "下台 >", COLOR_TEXT_PRIMARY, 0x10A2, 1);
}

static void renderDynamicInfo() {
  TFT_eSPI& tft = display_get_tft();

  const RadioStation* st = web_radio_get_current_station();
  int idx = web_radio_get_station_index();
  int total = web_radio_get_total_stations();
  bool playing = web_radio_is_playing();

  // 更新调频刻度红线指针 (y: 35..63)
  int pointerX = 26 + (int)((float)idx / (float)max(1, total - 1) * 260.0f);
  tft.fillRect(17, 49, 286, 14, 0x10A2); // 清除指针下部文字区
  tft.drawFastVLine(pointerX, 35, 18, TFT_RED);
  tft.drawFastVLine(pointerX - 1, 35, 18, TFT_RED);

  char dialBuf[32];
  snprintf(dialBuf, sizeof(dialBuf), "FM %d/5", idx + 1);
  font_draw_string(24, 50, dialBuf, COLOR_ACCENT_CYAN, 0x10A2, 1);

  // 清除电台信息区 (x: 24, y: 78, w: 270, h: 84)
  tft.fillRect(24, 78, 270, 84, COLOR_PANEL_BG);

  // 电台名称与流派
  if (st) {
    uint8_t nameScale = (font_get_string_width(st->name, 2) <= 264) ? 2 : 1;
    font_draw_string_clipped(26, nameScale == 2 ? 82 : 88, 264, st->name, COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, nameScale);
    char genreBuf[32];
    snprintf(genreBuf, sizeof(genreBuf), "流派: %s", st->genre);
    font_draw_string_clipped(26, 116, 264, genreBuf, COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
  }

  // 状态与动感声波
  const char* status = web_radio_get_status_text();
  uint16_t statusCol = playing ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED;
  font_draw_string_clipped(26, 140, 148, status, statusCol, COLOR_PANEL_BG, 1);
  s_lastStationIdx = idx;
  s_lastRadioPlaying = playing;
  snprintf(s_lastStatusStr, sizeof(s_lastStatusStr), "%s", status ? status : "");

  // 动感电波条
  int waveX = 180, waveY = 152;
  for (int i = 0; i < 6; i++) {
    int h = playing ? (4 + abs((int)(sin((s_wavePhase + i * 40) * 0.08f) * 16.0f))) : 3;
    tft.fillRect(waveX + i * 8, waveY - h, 5, h, playing ? COLOR_ACCENT_GREEN : 0x2965);
  }

  // 播放/暂停按钮文字
  tft.fillRoundRect(100, 178, 120, 46, 6, playing ? 0x2900 : 0x0A42);
  tft.drawRoundRect(100, 178, 120, 46, 6, playing ? TFT_RED : COLOR_ACCENT_GREEN);
  const char* btnTxt = playing ? "■ 停止" : "▶ 播放";
  font_draw_string_center(160, 192, btnTxt, playing ? TFT_RED : COLOR_ACCENT_GREEN, playing ? 0x2900 : 0x0A42, 1);
}

void web_radio_ui_enter() {
  renderStaticUI();
  web_radio_start();
  renderDynamicInfo();
}

void web_radio_ui_update() {
  web_radio_loop();

  if (millis() - s_lastAnimUpdate >= 80) {
    s_lastAnimUpdate = millis();
    s_wavePhase += 5;
    s_wavePhase += 5;
    const char* curStatus = web_radio_get_status_text();
    if (web_radio_get_station_index() != s_lastStationIdx ||
        web_radio_is_playing() != s_lastRadioPlaying ||
        strcmp(curStatus ? curStatus : "", s_lastStatusStr) != 0) {
      renderDynamicInfo();
    } else {
      renderWaveBars();
    }
  }
}

void web_radio_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    web_radio_stop();
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 底部功能按键 (y: 172..235)
  if (y >= 172 && y <= 235) {
    // 上一台 (x: 16..95)
    if (x >= 16 && x <= 95) {
      audio_test_beep(1400, 30);
      web_radio_prev();
      s_wavePhase += 5;
    const char* curStatus = web_radio_get_status_text();
    if (web_radio_get_station_index() != s_lastStationIdx ||
        web_radio_is_playing() != s_lastRadioPlaying ||
        strcmp(curStatus ? curStatus : "", s_lastStatusStr) != 0) {
      renderDynamicInfo();
    } else {
      renderWaveBars();
    }
      return;
    }

    // 播放/停止 (x: 96..224)
    if (x >= 96 && x <= 224) {
      audio_test_beep(1600, 40);
      web_radio_toggle_play();
      s_wavePhase += 5;
    const char* curStatus = web_radio_get_status_text();
    if (web_radio_get_station_index() != s_lastStationIdx ||
        web_radio_is_playing() != s_lastRadioPlaying ||
        strcmp(curStatus ? curStatus : "", s_lastStatusStr) != 0) {
      renderDynamicInfo();
    } else {
      renderWaveBars();
    }
      return;
    }

    // 下一台 (x: 225..305)
    if (x >= 225 && x <= 305) {
      audio_test_beep(1400, 30);
      web_radio_next();
      s_wavePhase += 5;
    const char* curStatus = web_radio_get_status_text();
    if (web_radio_get_station_index() != s_lastStationIdx ||
        web_radio_is_playing() != s_lastRadioPlaying ||
        strcmp(curStatus ? curStatus : "", s_lastStatusStr) != 0) {
      renderDynamicInfo();
    } else {
      renderWaveBars();
    }
      return;
    }
  }

  // 3. 点击卡片中间：切换播放/停止
  if (x >= 16 && x <= 304 && y >= 72 && y <= 168) {
    audio_test_beep(1500, 30);
    web_radio_toggle_play();
    s_wavePhase += 5;
    const char* curStatus = web_radio_get_status_text();
    if (web_radio_get_station_index() != s_lastStationIdx ||
        web_radio_is_playing() != s_lastRadioPlaying ||
        strcmp(curStatus ? curStatus : "", s_lastStatusStr) != 0) {
      renderDynamicInfo();
    } else {
      renderWaveBars();
    }
    return;
  }
}
