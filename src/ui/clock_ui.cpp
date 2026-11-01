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
static int s_lastSec = -1;
static bool s_isFlipMode = true; // 默认采用复古拟物翻页时钟模式

static const char* getWeekDayName(int wday) {
  switch (wday) {
    case 0: return "星期日";
    case 1: return "星期一";
    case 2: return "星期二";
    case 3: return "星期三";
    case 4: return "星期四";
    case 5: return "星期五";
    case 6: return "星期六";
    default: return "星期日";
  }
}

// 绘制单张复古拟物翻页卡片 (Split-Flap Plate)
static void drawFlipCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* str) {
  TFT_eSPI& tft = display_get_tft();
  int midY = y + h / 2;

  // 上半卡片 (稍亮微渐变黑灰)
  tft.fillRoundRect(x, y, w, h / 2 + 1, 6, 0x2124);
  // 下半卡片 (稍暗带阴影黑灰)
  tft.fillRoundRect(x, midY, w, h / 2 + 1, 6, 0x10A2);
  // 卡片立体外边框
  tft.drawRoundRect(x, y, w, h, 6, 0x4228);

  // 绘制大号数字文本 (Font 7 为 48px 大数码字体)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(0xFFDE); // 复古象牙白
  tft.setTextFont(7);
  tft.drawString(str, x + w / 2, midY + 1);
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);

  // 机械横切缝与固定转轴槽 (Split Seam)
  tft.drawFastHLine(x + 2, midY - 1, w - 4, 0x0000);
  tft.drawFastHLine(x + 2, midY,     w - 4, 0x0000);
  tft.drawFastHLine(x + 3, midY + 1, w - 6, 0x2965); // 缝隙微光下边缘

  // 左右两侧金属转轴扣 (Hinge Pins)
  tft.fillRect(x, midY - 3, 3, 6, 0xC618);
  tft.fillRect(x + w - 3, midY - 3, 3, 6, 0xC618);
}

// 渲染复古机械翻页时钟界面
static void renderFlipClock(const struct tm& timeinfo) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(0x0841); // 经典深灰底色

  // 1. 顶部小状态栏
  char ipBuf[32];
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(ipBuf, sizeof(ipBuf), "NTP已校准 | %s", WiFi.SSID().c_str());
    font_draw_string_clipped(14, 6, 198, ipBuf, COLOR_ACCENT_GREEN, 0x0841, 1);
  } else {
    font_draw_string(14, 6, "离线时钟 (点下方配网)", COLOR_TEXT_MUTED, 0x0841, 1);
  }

  // 右侧电量显示
  char bBuf[24];
  snprintf(bBuf, sizeof(bBuf), "%d%% %.2fV", battery_get_percent(), battery_get_voltage());
  font_draw_string(216, 6, bBuf, COLOR_ACCENT_CYAN, 0x0841, 1);

  // 2. 机械翻页双大卡片 (小时 + 分钟)
  char hourStr[4], minStr[4];
  snprintf(hourStr, sizeof(hourStr), "%02d", timeinfo.tm_hour);
  snprintf(minStr, sizeof(minStr), "%02d", timeinfo.tm_min);

  int cardW = 126, cardH = 108, cardY = 28;
  drawFlipCard(18, cardY, cardW, cardH, hourStr);
  drawFlipCard(176, cardY, cardW, cardH, minStr);

  // 中间拟物双点闪烁冒号
  bool secEven = (timeinfo.tm_sec % 2 == 0);
  uint16_t colonCol = secEven ? COLOR_ACCENT_CYAN : 0x2124;
  tft.fillCircle(160, cardY + 36, 4, colonCol);
  tft.fillCircle(160, cardY + 72, 4, colonCol);

  // 3. 秒数动感刻度进度线 (y: 144)
  int secW = map(timeinfo.tm_sec, 0, 59, 0, 284);
  tft.fillRoundRect(18, 142, 284, 5, 2, 0x10A2);
  if (secW > 0) {
    tft.fillRoundRect(18, 142, secW, 5, 2, COLOR_ACCENT_CYAN);
  }

  // 4. 底部桌面信息卡片 (y: 154, w: 288, h: 42)
  tft.fillRoundRect(16, 153, 288, 42, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(16, 153, 288, 42, 6, COLOR_PANEL_BORDER);

  // 日期与星期
  char dateBuf[48];
  snprintf(dateBuf, sizeof(dateBuf), "%d年%02d月%02d日 %s",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, getWeekDayName(timeinfo.tm_wday));
  font_draw_string(24, 158, dateBuf, 0xFFDE, COLOR_PANEL_BG, 1);

  // 城市与天气
  const WeatherInfo& winfo = weather_get_info();
  if (winfo.valid) {
    char wBuf[48];
    snprintf(wBuf, sizeof(wBuf), "%s %s %s", winfo.city, winfo.temp, winfo.desc);
    font_draw_string(24, 175, wBuf, COLOR_ACCENT_GREEN, COLOR_PANEL_BG, 1);
  } else {
    font_draw_string(24, 175, "点击此处可手动刷新天气", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 5. 底部三大按钮 (y: 202, h: 34)
  // [进入桌面]
  tft.fillRoundRect(16, 201, 88, 34, 5, 0x10A2);
  tft.drawRoundRect(16, 201, 88, 34, 5, COLOR_ACCENT_GREEN);
  font_draw_string_center(60, 209, "进入桌面", COLOR_ACCENT_GREEN, 0x10A2, 1);

  // [模式切换: 看板]
  tft.fillRoundRect(116, 201, 88, 34, 5, 0x10A2);
  tft.drawRoundRect(116, 201, 88, 34, 5, COLOR_ACCENT_ORANGE);
  font_draw_string_center(160, 209, "切换看板", COLOR_ACCENT_ORANGE, 0x10A2, 1);

  // [WiFi配网]
  tft.fillRoundRect(216, 201, 88, 34, 5, 0x10A2);
  tft.drawRoundRect(216, 201, 88, 34, 5, COLOR_ACCENT_CYAN);
  font_draw_string_center(260, 209, "WiFi配网", COLOR_ACCENT_CYAN, 0x10A2, 1);
}

// 渲染原有数字信息看板界面
static void renderClockDashboard(const struct tm& timeinfo) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  // 1. 顶部状态栏 (高度 24px, 底色 0x10A2)
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

  // 电池指示
  char bBuf[24];
  snprintf(bBuf, sizeof(bBuf), "%d%% %.2fV", battery_get_percent(), battery_get_voltage());
  font_draw_string(194, 4, bBuf, COLOR_ACCENT_CYAN, 0x10A2, 1);

  int bx = 276, by = 5, bw = 30, bh = 14;
  tft.drawRect(bx, by, bw, bh, COLOR_TEXT_PRIMARY);
  tft.fillRect(bx + bw, by + 4, 3, 6, COLOR_TEXT_PRIMARY);
  int fillW = map(battery_get_percent(), 0, 100, 0, bw - 4);
  fillW = constrain(fillW, 0, bw - 4);
  uint16_t bCol = battery_is_low() ? TFT_RED : COLOR_ACCENT_GREEN;
  tft.fillRect(bx + 2, by + 2, fillW, bh - 4, bCol);
  tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, 0x10A2);

  // 2. 超大实时数字时钟与日期
  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG_DARK);
  tft.setTextFont(7);
  tft.drawString(timeStr, 20, 32);
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);

  // 日期与星期显示
  char dateBuf[48];
  snprintf(dateBuf, sizeof(dateBuf), "%d年%02d月%02d日  %s", 
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, getWeekDayName(timeinfo.tm_wday));
  font_draw_string(22, 86, dateBuf, COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);

  // 3. 实时天气信息卡片
  tft.fillRoundRect(14, 110, 292, 62, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(14, 110, 292, 62, 6, 0x05E0);

  const WeatherInfo& winfo = weather_get_info();
  if (winfo.valid) {
    font_draw_string(22, 124, winfo.temp, COLOR_ACCENT_GREEN, COLOR_PANEL_BG, 2);
    font_draw_string(96, 120, winfo.city, COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
    font_draw_string(156, 120, winfo.desc, COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);

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

  // 刷新按键
  tft.fillRoundRect(258, 118, 42, 46, 4, 0x2124);
  tft.drawRoundRect(258, 118, 42, 46, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(279, 133, "刷新", COLOR_TEXT_PRIMARY, 0x2124, 1);

  // 4. 底部功能按钮
  tft.fillRoundRect(14, 182, 90, 46, 6, 0x10A2);
  tft.drawRoundRect(14, 182, 90, 46, 6, COLOR_ACCENT_GREEN);
  font_draw_string_center(59, 196, "进入桌面", COLOR_ACCENT_GREEN, 0x10A2, 1);

  tft.fillRoundRect(114, 182, 94, 46, 6, 0x10A2);
  tft.drawRoundRect(114, 182, 94, 46, 6, COLOR_ACCENT_ORANGE);
  font_draw_string_center(161, 196, "复古翻页", COLOR_ACCENT_ORANGE, 0x10A2, 1);

  tft.fillRoundRect(218, 182, 88, 46, 6, 0x10A2);
  tft.drawRoundRect(218, 182, 88, 46, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(262, 196, "息屏休眠", COLOR_TEXT_MUTED, 0x10A2, 1);
}

static void renderCurrentClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    timeinfo.tm_hour = 12;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_year = 126;
    timeinfo.tm_mon = 10;
    timeinfo.tm_mday = 1;
    timeinfo.tm_wday = 0;
  }

  if (s_isFlipMode) {
    renderFlipClock(timeinfo);
  } else {
    renderClockDashboard(timeinfo);
  }
}

void clock_ui_enter() {
  s_lastMin = -1;
  s_lastSec = -1;
  renderCurrentClock();
}

void clock_ui_update() {
  net_update();

  if (millis() - s_lastClockSec >= 1000) {
    s_lastClockSec = millis();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      if (timeinfo.tm_min != s_lastMin) {
        s_lastMin = timeinfo.tm_min;
        // 分钟翻转时发出轻微复古机械咔哒音效
        audio_test_beep(1400, 15);
        renderCurrentClock();
      } else {
        // 秒级小刷新
        if (s_isFlipMode) {
          TFT_eSPI& tft = display_get_tft();
          // 更新中间冒号闪烁
          bool secEven = (timeinfo.tm_sec % 2 == 0);
          uint16_t colonCol = secEven ? COLOR_ACCENT_CYAN : 0x2124;
          int cardY = 28;
          tft.fillCircle(160, cardY + 36, 4, colonCol);
          tft.fillCircle(160, cardY + 72, 4, colonCol);

          // 更新秒级刻度条
          int secW = map(timeinfo.tm_sec, 0, 59, 0, 284);
          tft.fillRoundRect(18, 142, 284, 5, 2, 0x10A2);
          if (secW > 0) {
            tft.fillRoundRect(18, 142, secW, 5, 2, COLOR_ACCENT_CYAN);
          }
        } else {
          char timeStr[16];
          snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
          TFT_eSPI& tft = display_get_tft();
          tft.setTextDatum(TL_DATUM);
          tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG_DARK);
          tft.setTextFont(7);
          tft.drawString(timeStr, 20, 32);
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
        }
      }
    }
  }
}

void clock_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 中间天气卡片点击刷新天气
  if ((s_isFlipMode && y >= 152 && y <= 196) || (!s_isFlipMode && y >= 110 && y <= 176 && x >= 240)) {
  if (true) {
      audio_test_beep(1500, 40);
      weather_fetch();
      renderCurrentClock();
      return;
    }
  }

  // 2. 底部功能按键 (y >= 196)
  if (y >= (s_isFlipMode ? 197 : 180)) {
    // [进入桌面] (x: 10..106)
    if (x >= 10 && x <= 106) {
      audio_test_beep(1200, 30);
      ui_switch_to(UI_PAGE_HOME);
      return;
    }

    // [模式切换: 翻页/看板] (x: 110..210)
    if (x >= 110 && x <= 210) {
      audio_test_beep(1600, 40);
      s_isFlipMode = !s_isFlipMode;
      renderCurrentClock();
      return;
    }

    // 右侧按键: [WiFi配网] 或 [息屏休眠] (x: 214..310)
    if (x >= 214 && x <= 310) {
      if (s_isFlipMode) {
        audio_test_beep(1800, 50);
        net_start_config_portal();
      } else {
        audio_test_beep(1000, 40);
        display_sleep();
      }
      return;
    }
  }

  // 点击上方卡片区域：快速切换翻页/看板模式
  if (y < 160) {
    audio_test_beep(1600, 30);
    s_isFlipMode = !s_isFlipMode;
    renderCurrentClock();
    return;
  }
}
