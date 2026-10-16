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

  // 1. 顶部状态栏 (高度 24px)
  tft.fillRect(0, 0, 320, 24, 0x10A2);
  tft.drawLine(0, 24, 320, 24, COLOR_PANEL_BORDER);

  // WiFi 状态
  if (WiFi.status() == WL_CONNECTED) {
    char ipBuf[32];
    snprintf(ipBuf, sizeof(ipBuf), "WiFi: %s", WiFi.localIP().toString().c_str());
    font_draw_string(8, 4, ipBuf, COLOR_ACCENT_GREEN, 0x10A2, 1);
  } else {
    font_draw_string(8, 4, "WiFi: 未连接 (可点下方配网)", COLOR_TEXT_MUTED, 0x10A2, 1);
  }

  // 电池电压
  char bBuf[16];
  snprintf(bBuf, sizeof(bBuf), "%.2fV", battery_get_voltage());
  font_draw_string(270, 4, bBuf, COLOR_ACCENT_CYAN, 0x10A2, 1);

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
    // 气温大字
    font_draw_string(24, 118, winfo.temp, COLOR_ACCENT_GREEN, COLOR_PANEL_BG, 2);
    // 天气状况与湿度
    font_draw_string(118, 122, winfo.desc, COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);
    font_draw_string(118, 144, winfo.humidity, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
    // 城市
    font_draw_string(210, 134, winfo.city, COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
  } else {
    font_draw_string(24, 124, "实时天气: 待获取", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
    if (WiFi.status() == WL_CONNECTED) {
      font_draw_string(24, 146, "点击右侧刷新获取当前天气", COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
    } else {
      font_draw_string(24, 146, "连接 WiFi 后自动同步全国天气", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
    }
  }

  // 天气卡片右侧刷新按钮图标 (x: 260, y: 116, w: 38, h: 50)
  tft.fillRoundRect(260, 118, 38, 46, 4, 0x2124);
  tft.drawRoundRect(260, 118, 38, 46, 4, COLOR_PANEL_BORDER);
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
  // 1. 天气卡片点击刷新 (x: 250..300, y: 110..172)
  if (x >= 250 && x <= 300 && y >= 110 && y <= 172) {
    audio_test_beep(1500, 40);
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
