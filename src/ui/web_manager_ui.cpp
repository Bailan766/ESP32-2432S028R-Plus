#include "web_manager_ui.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "storage/sdcard.h"
#include "system/web_file_manager.h"
#include "system/net_manager.h"
#include "config.h"
#include <WiFi.h>

static unsigned long s_lastUpdate = 0;
static String s_lastActionStr = "";

static void renderStaticUI() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("空中无线文件管理", true);

  // 1. IP 地址展示卡片 (x: 14, y: 36, w: 292, h: 72)
  tft.fillRoundRect(14, 36, 292, 72, 6, 0x10A2);
  tft.drawRoundRect(14, 36, 292, 72, 6, COLOR_ACCENT_GREEN);

  font_draw_string(24, 42, "网页管理地址 (浏览器访问):", COLOR_TEXT_MUTED, 0x10A2, 1);

  String ip = web_file_manager_get_ip();
  if (WiFi.status() == WL_CONNECTED) {
    char urlBuf[48];
    snprintf(urlBuf, sizeof(urlBuf), "http://%s", ip.c_str());
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_ACCENT_GREEN, 0x10A2);
    tft.drawString(urlBuf, 24, 60, 4);
  } else {
    font_draw_string(24, 62, "Wi-Fi 未连接 (请点下方配网)", TFT_YELLOW, 0x10A2, 1);
  }

  font_draw_string_clipped(24, 88, 274, "同WiFi下浏览器打开网址即可传歌/传书", COLOR_ACCENT_CYAN, 0x10A2, 1);

  // 2. 状态信息卡片 (x: 14, y: 114, w: 292, h: 68)
  tft.fillRoundRect(14, 114, 292, 68, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(14, 114, 292, 68, 6, COLOR_PANEL_BORDER);

  font_draw_string(24, 120, "当前状态:", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);

  // 3. 底部两大按钮 (y: 190, h: 38)
  // [退出并保存] (x: 14, w: 140)
  tft.fillRoundRect(14, 190, 140, 38, 5, 0x10A2);
  tft.drawRoundRect(14, 190, 140, 38, 5, COLOR_ACCENT_GREEN);
  font_draw_string_center(84, 201, "退出并保存", COLOR_ACCENT_GREEN, 0x10A2, 1);

  // [WiFi配网] (x: 166, w: 140)
  tft.fillRoundRect(166, 190, 140, 38, 5, 0x10A2);
  tft.drawRoundRect(166, 190, 140, 38, 5, COLOR_ACCENT_CYAN);
  font_draw_string_center(236, 201, "WiFi 配网", COLOR_ACCENT_CYAN, 0x10A2, 1);
}

static void renderDynamicInfo() {
  TFT_eSPI& tft = display_get_tft();

  const char* act = web_file_manager_get_last_action();
  uint32_t count = web_file_manager_get_upload_count();

  // 清除旧内容区域 (x: 24, y: 136, w: 270, h: 42)
  tft.fillRect(24, 136, 270, 42, COLOR_PANEL_BG);

  font_draw_string_clipped(24, 138, 270, act, TFT_WHITE, COLOR_PANEL_BG, 1);

  char statBuf[48];
  snprintf(statBuf, sizeof(statBuf), "累计接收: %u 个 | TF卡: %s", count, sdcard_get_state_str());
  font_draw_string_clipped(24, 158, 270, statBuf, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
}

void web_manager_ui_enter() {
  web_file_manager_start();
  renderStaticUI();
  renderDynamicInfo();
  s_lastUpdate = millis();
}

void web_manager_ui_update() {
  web_file_manager_loop();

  if (millis() - s_lastUpdate >= 500) {
    s_lastUpdate = millis();
    renderDynamicInfo();
  }
}

void web_manager_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    web_file_manager_stop();
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 底部功能按钮 (y: 185..235)
  if (y >= 185 && y <= 235) {
    // [退出并保存] (x: 14..154)
    if (x >= 14 && x <= 154) {
      audio_test_beep(1200, 30);
      web_file_manager_stop();
      ui_switch_to(UI_PAGE_HOME);
      return;
    }

    // [WiFi配网] (x: 160..306)
    if (x >= 160 && x <= 306) {
      audio_test_beep(1800, 50);
      net_start_config_portal();
      renderStaticUI();
      renderDynamicInfo();
      return;
    }
  }
}
