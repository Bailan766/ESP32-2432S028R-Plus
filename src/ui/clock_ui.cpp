#include "clock_ui.h"
#include "ui.h"
#include "hardware/display.h"
#include "hardware/battery.h"
#include "hardware/audio.h"
#include "font_cn.h"
#include "system/net_manager.h"
#include "system/weather.h"
#include "config.h"
#include <time.h>
#include <WiFi.h>

static unsigned long s_lastClockSec = 0;
static int s_lastMin = -1;

static const char* getWeekDayName(int wday) {
  switch (wday) {
    case 0: return "星期日";
    case 1: return "星期一";
    case 2: return "星期二";
    case 3: return "星期三";
    case 4: return "星期四";
    case 5: return "星期五";
    case 6: return "星期六";
    default: return "星期五";
  }
}

static void renderClockDashboard() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  // 1. 顶部状态栏 (高度 24px, 底色 0x10A2)
  tft.fillRect(0, 0, 320, 24, 0x10A2);
  tft.drawLine(0, 24, 320, 24, COLOR_PANEL_BORDER);

  // WiFi 状态 (左侧)
  if (WiFi.status() == WL_CONNECTED) {
    char ipBuf[32];
    snprintf(ipBuf, sizeof(ipBuf), "WiFi: %s", WiFi.localIP().toString().c_str());
    font_draw_string(8, 4, ipBuf, COLOR_ACCENT_GREEN, 0x10A2, 1);
  } else {
    font_draw_string(8, 4, "WiFi: 未连接 (可点下方配网)", COLOR_TEXT_MUTED, 0x10A2, 1);
  }

  // 电池电量与电压状态指示 (右侧: 194..274)
  char bBuf[24];
  snprintf(bBuf, sizeof(bBuf), "%d%% %.2fV", battery_get_percent(), battery_get_voltage());
  font_draw_string(194, 4, bBuf, COLOR_ACCENT_CYAN, 0x10A2, 1);

  // 电池外框与填充条 (右侧: bx=276, by=5, bw=30, bh=14)
  int bx = 276, by = 5, bw = 30, bh = 14;
  tft.drawRect(bx, by, bw, bh, COLOR_TEXT_PRIMARY);
  tft.fillRect(bx + bw, by + 4, 3, 6, COLOR_TEXT_PRIMARY);
  int fillW = map(battery_get_percent(), 0, 100, 0, bw - 4);
  fillW = constrain(fillW, 0, bw - 4);
  uint16_t bCol = battery_is_low() ? TFT_RED : COLOR_ACCENT_GREEN;
  tft.fillRect(bx + 2, by + 2, fillW, bh - 4, bCol);
  tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, 0x10A2);

  // 2. 超大实时数字时钟与日期
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 10);

  char timeStr[16];
  if (timeValid) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    strcpy(timeStr, "12:00:00");
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG_DARK);
  tft.setTextFont(7); // 48 像素 7 段数码管大字体
  tft.drawString(timeStr, 20, 32);

  // 日期与星期显示 (y: 86)
  if (timeValid) {
    char dateBuf[48];
    snprintf(dateBuf, sizeof(dateBuf), "%d年%02d月%02d日  %s", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, getWeekDayName(timeinfo.tm_wday));
    font_draw_string(22, 86, dateBuf, COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);
  } else {
    font_draw_string(22, 86, "未同步网络时间 (请连接 WiFi)", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  }

  // 3. 实时天气信息卡片 (x: 14, y: 110, w: 292, h: 62)
  tft.fillRoundRect(14, 110, 292, 62, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(14, 110, 292, 62, 6, 0x05E0);

  const WeatherInfo& winfo = weather_get_info();
  if (winfo.valid) {
    // 气温 (左侧醒目展示, 大号 32px 高度, scale 2)
    font_draw_string(22, 124, winfo.temp, COLOR_ACCENT_GREEN, COLOR_PANEL_BG, 2);

    // 第一行: 城市 与 天气状况 (y: 120)
    font_draw_string(96, 120, winfo.city, COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
    font_draw_string(156, 120, winfo.desc, COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);

    // 第二行: 湿度信息 (y: 144)
    char humBuf[32];
    if (strstr(winfo.humidity, "湿度")) {
      snprintf(humBuf, sizeof(humBuf), "%s", winfo.humidity);
    } else {
      snprintf(humBuf, sizeof(humBuf), "湿度: %s", winfo.humidity);
    }
    font_draw_string(96, 144, humBuf, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      font_draw_string(24, 124, "富平县 (同步天气中...)", COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
      font_draw_string(24, 146, "点击右侧[刷新]重试同步", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
    } else {
      font_draw_string(24, 124, "富平县 (Wi-Fi 未连接)", COLOR_ACCENT_ORANGE, COLOR_PANEL_BG, 1);
      font_draw_string(24, 146, "请点击下方 [WiFi配网]", COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
    }
  }

  // 天气卡片右侧刷新按钮图标 (x: 258, y: 118, w: 42, h: 46)
  tft.fillRoundRect(258, 118, 42, 46, 4, 0x2124);
  tft.drawRoundRect(258, 118, 42, 46, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(279, 133, "刷新", COLOR_TEXT_PRIMARY, 0x2124, 1);

  // 4. 底部三大快捷功能按键 (y: 182, h: 46)
  // [进入桌面]
  tft.fillRoundRect(14, 182, 90, 46, 6, 0x10A2);
  tft.drawRoundRect(14, 182, 90, 46, 6, COLOR_ACCENT_GREEN);
  font_draw_string_center(59, 196, "进入桌面", COLOR_ACCENT_GREEN, 0x10A2, 1);

  // [WiFi配网]
  tft.fillRoundRect(114, 182, 94, 46, 6, 0x10A2);
  tft.drawRoundRect(114, 182, 94, 46, 6, COLOR_ACCENT_CYAN);
  font_draw_string_center(161, 196, "WiFi配网", COLOR_ACCENT_CYAN, 0x10A2, 1);

  // [息屏休眠]
  tft.fillRoundRect(218, 182, 88, 46, 6, 0x10A2);
  tft.drawRoundRect(218, 182, 88, 46, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(262, 196, "息屏休眠", COLOR_TEXT_MUTED, 0x10A2, 1);
}

void clock_ui_enter() {
  s_lastMin = -1;
  renderClockDashboard();
}

void clock_ui_update() {
  net_update();

  if (millis() - s_lastClockSec >= 1000) {
    s_lastClockSec = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      if (timeinfo.tm_min != s_lastMin) {
        s_lastMin = timeinfo.tm_min;
        renderClockDashboard();
      } else {
        // 仅局部微刷新秒钟区域
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        TFT_eSPI& tft = display_get_tft();
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG_DARK);
        tft.setTextFont(7);
        tft.drawString(timeStr, 20, 32);
      }
    }
  }
}

void clock_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 天气卡片点击刷新 (x: 250..305, y: 110..172)
  if (x >= 250 && x <= 305 && y >= 110 && y <= 172) {
    audio_test_beep(1500, 40);
    if (WiFi.status() != WL_CONNECTED) {
      TFT_eSPI& tft = display_get_tft();
      tft.fillRect(24, 144, 230, 20, COLOR_PANEL_BG);
      font_draw_string(24, 146, "请先点击下方[WiFi配网]!", TFT_RED, COLOR_PANEL_BG, 1);
      return;
    }
    font_draw_string_center(279, 133, "同步", COLOR_ACCENT_GREEN, 0x2124, 1);
    weather_fetch();
    renderClockDashboard();
    return;
  }

  // 2. 底部功能按键 (y >= 180)
  if (y >= 180) {
    // [进入桌面] (x: 14..104)
    if (x >= 14 && x <= 104) {
      audio_test_beep(1200, 30);
      ui_switch_to(UI_PAGE_HOME);
      return;
    }

    // [WiFi配网] (x: 114..208)
    if (x >= 114 && x <= 208) {
      audio_test_beep(1800, 50);
      net_start_config_portal();
      renderClockDashboard();
      return;
    }

    // [息屏休眠] (x: 218..306)
    if (x >= 218 && x <= 306) {
      audio_test_beep(1000, 40);
      display_sleep();
      return;
    }
  }

  // 点击屏幕其他任意位置默认快速解锁进入主界面
  audio_test_beep(1200, 30);
  ui_switch_to(UI_PAGE_HOME);
}
