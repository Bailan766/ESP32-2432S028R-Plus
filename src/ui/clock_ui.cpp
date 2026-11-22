#include "clock_ui.h"
#include "ui.h"
#include "ui_theme.h"
#include "hardware/display.h"
#include "hardware/battery.h"
#include "hardware/audio.h"
#include "font_cn.h"
#include "system/net_manager.h"
#include "system/weather.h"
#include "system/settings_manager.h"
#include "system/power_manager.h"
#include "player/player.h"
#include "radio/web_radio.h"
#include "config.h"
#include <time.h>
#include <WiFi.h>

static unsigned long s_lastClockSec = 0;
static int s_lastMin = -1;
static int s_lastSec = -1;
static bool s_isFlipMode = true; // 默认采用复古拟物翻页时钟模式
static bool s_nightDim = false;  // 桌面夜光低亮模式 (15% 亮度护眼)

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

// 绘制单张 M3 拟物翻页大卡片 (Split-Flap Plate)
static void drawFlipCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* str) {
  TFT_eSPI& tft = display_get_tft();
  int midY = y + h / 2;

  // 上半卡片 (M3 SurfaceContainerHigh) + 下半卡片 (M3 SurfaceContainer), 大圆角 14px 无细边框
  tft.fillRoundRect(x, y, w, h / 2 + 1, 14, UI_COL_SURFACE_HIGH);
  tft.fillRoundRect(x, midY, w, h / 2 + 1, 14, UI_COL_SURFACE);

  // 绘制大号数字文本 (Font 7 为 48px 大数码字体)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(M3_HERO_START); // 柔和玫瑰象牙粉数字
  tft.setTextFont(7);
  tft.drawString(str, x + w / 2, midY + 1);
  tft.setTextFont(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);

  // 机械横切缝与金属转轴槽
  tft.drawFastHLine(x + 4, midY - 1, w - 8, UI_COL_BG);
  tft.drawFastHLine(x + 4, midY,     w - 8, UI_COL_BG);
  tft.fillRoundRect(x, midY - 3, 4, 6, 2, M3_PRIMARY_CONTAINER);
  tft.fillRoundRect(x + w - 4, midY - 3, 4, 6, 2, M3_PRIMARY_CONTAINER);
}

// 渲染 M3 复古机械翻页时钟界面
static void renderFlipClock(const struct tm& timeinfo) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  // 1. 顶部小状态栏
  char ipBuf[32];
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(ipBuf, sizeof(ipBuf), "NTP已校准 · %s", WiFi.SSID().c_str());
    font_draw_string_clipped(14, 6, 198, ipBuf, M3_SAGE, UI_COL_BG, 1);
  } else {
    font_draw_string(14, 6, "离线时钟 (点击此处配网)", UI_COL_TEXT_MUTED, UI_COL_BG, 1);
  }

  // 右侧电量显示
  char bBuf[24];
  snprintf(bBuf, sizeof(bBuf), "%d%% %.2fV", battery_get_percent(), battery_get_voltage());
  font_draw_string(216, 6, bBuf, M3_PRIMARY, UI_COL_BG, 1);

  // 2. 机械翻页双大卡片 (小时 + 分钟)
  char hourStr[4], minStr[4];
  snprintf(hourStr, sizeof(hourStr), "%02d", timeinfo.tm_hour);
  snprintf(minStr, sizeof(minStr), "%02d", timeinfo.tm_min);

  int cardW = 126, cardH = 108, cardY = 28;
  drawFlipCard(18, cardY, cardW, cardH, hourStr);
  drawFlipCard(176, cardY, cardW, cardH, minStr);

  // 中间闪烁冒号
  bool secEven = (timeinfo.tm_sec % 2 == 0);
  uint16_t colonCol = secEven ? M3_PRIMARY : UI_COL_SURFACE;
  tft.fillCircle(160, cardY + 36, 4, colonCol);
  tft.fillCircle(160, cardY + 72, 4, colonCol);

  // 3. 秒数 M3 动感刻度进度条 (y: 142)
  int secW = map(timeinfo.tm_sec, 0, 59, 0, 284);
  tft.fillRoundRect(18, 142, 284, 5, 2, UI_COL_SURFACE);
  if (secW > 0) {
    tft.fillRoundRect(18, 142, secW, 5, 2, M3_PRIMARY);
  }

  // 4. 底部 M3 大圆角桌面信息卡片 (y: 153, w: 288, h: 42, r: 12)
  ui_theme_draw_panel(16, 153, 288, 42, 12, UI_COL_SURFACE, UI_COL_SURFACE);

  // 日期与星期 + 右侧摆件状态（睡眠定时 / 正在播放）
  char dateBuf[48];
  snprintf(dateBuf, sizeof(dateBuf), "%d年%02d月%02d日 %s",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, getWeekDayName(timeinfo.tm_wday));
  font_draw_string_bold(24, 158, 198, dateBuf, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

  uint16_t remSec = power_get_sleep_timer_remaining_sec();
  if (remSec > 0) {
    char tBuf[20];
    snprintf(tBuf, sizeof(tBuf), "定时%um", (unsigned)((remSec + 59) / 60));
    ui_theme_draw_badge(228, 156, 68, 18, tBuf, M3_TERTIARY, M3_TERTIARY_CONTAINER);
  } else if (player_get_state() == STATE_PLAYING || web_radio_is_playing()) {
    ui_theme_draw_badge(228, 156, 68, 18, "▶ 播放", M3_ON_PRIMARY, M3_PRIMARY);
  }

  // 城市与天气
  const WeatherInfo& winfo = weather_get_info();
  if (winfo.valid) {
    char wBuf[56];
    snprintf(wBuf, sizeof(wBuf), "%s %s %s %s", winfo.city, winfo.temp, winfo.desc, winfo.humidity);
    font_draw_string_clipped(24, 175, 272, wBuf, M3_SAGE, UI_COL_SURFACE, 1);
  } else {
    font_draw_string(24, 175, "点击此处刷新天气 (点顶部可配网)", UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
  }

  // 5. 底部 M3 悬浮胶囊功能栏 (x: 12, y: 201, w: 296, h: 34, r: 17)
  tft.fillRoundRect(12, 201, 296, 34, 17, UI_COL_SURFACE_HIGH);
  ui_theme_draw_badge(16, 204, 66, 28, "桌面", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_PRESS);
  ui_theme_draw_badge(88, 204, 66, 28, "看板", M3_TERTIARY, M3_TERTIARY_CONTAINER);

  bool alwaysOn = settings_get().clockAlwaysOn;
  ui_theme_draw_badge(160, 204, 72, 28, alwaysOn ? "常亮:开" : "常亮:关",
                      alwaysOn ? M3_PRIMARY : UI_COL_TEXT_MUTED,
                      alwaysOn ? M3_PRIMARY_CONTAINER : UI_COL_SURFACE);

  if (WiFi.status() == WL_CONNECTED) {
    ui_theme_draw_badge(238, 204, 66, 28, s_nightDim ? "夜光:开" : "夜光:关",
                        s_nightDim ? M3_ON_PRIMARY : UI_COL_TEXT_PRIMARY,
                        s_nightDim ? M3_TERTIARY : UI_COL_SURFACE_PRESS);
  } else {
    ui_theme_draw_badge(238, 204, 66, 28, "配网", M3_PRIMARY, M3_PRIMARY_CONTAINER);
  }
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
        // 仅在开启整点报时且白天 08:00~22:00 整点时轻微提示，平时保持安静桌面
        if (settings_get().hourlyChime && timeinfo.tm_min == 0 &&
            timeinfo.tm_hour >= 8 && timeinfo.tm_hour <= 22) {
          audio_test_beep(1400, 20);
        }
        renderCurrentClock();
      } else {
        // 秒级小刷新
        if (s_isFlipMode) {
          TFT_eSPI& tft = display_get_tft();
          // 更新中间冒号闪烁
          bool secEven = (timeinfo.tm_sec % 2 == 0);
          uint16_t colonCol = secEven ? M3_PRIMARY : UI_COL_SURFACE;
          int cardY = 28;
          tft.fillCircle(160, cardY + 36, 4, colonCol);
          tft.fillCircle(160, cardY + 72, 4, colonCol);

          // 更新秒级刻度条
          int secW = map(timeinfo.tm_sec, 0, 59, 0, 284);
          tft.fillRoundRect(18, 142, 284, 5, 2, UI_COL_SURFACE);
          if (secW > 0) {
            tft.fillRoundRect(18, 142, secW, 5, 2, M3_PRIMARY);
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
  // 0. 顶部 Wi-Fi 状态栏 (y <= 26, x <= 200) -> 随时触发 Wi-Fi 配网
  if (y <= 26 && x <= 200) {
    audio_test_beep(1800, 50);
    net_start_config_portal();
    return;
  }

  // 1. 中间天气卡片点击刷新天气
  if ((s_isFlipMode && y >= 152 && y <= 196) || (!s_isFlipMode && y >= 110 && y <= 176 && x >= 240)) {
    audio_test_beep(1500, 40);
    weather_fetch();
    renderCurrentClock();
    return;
  }

  // 2. 底部功能按键
  if (s_isFlipMode && y >= 197) {
    if (x >= 10 && x <= 84) {
      audio_test_beep(1200, 30);
      if (s_nightDim) {
        s_nightDim = false;
        display_set_brightness(settings_get().brightness);
      }
      ui_switch_to(UI_PAGE_HOME);
    } else if (x >= 86 && x <= 158) {
      audio_test_beep(1600, 40);
      s_isFlipMode = false;
      renderCurrentClock();
    } else if (x >= 160 && x <= 236) {
      audio_test_beep(1400, 35);
      settings_get().clockAlwaysOn = !settings_get().clockAlwaysOn;
      settings_save();
      renderCurrentClock();
    } else if (x >= 238 && x <= 312) {
      if (WiFi.status() == WL_CONNECTED) {
        audio_test_beep(1100, 30);
        s_nightDim = !s_nightDim;
        display_set_brightness(s_nightDim ? 15 : settings_get().brightness);
        renderCurrentClock();
      } else {
        audio_test_beep(1800, 50);
        net_start_config_portal();
      }
    }
    return;
  }

  if (!s_isFlipMode && y >= 180) {
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

    // [息屏休眠] (x: 214..310)
    if (x >= 214 && x <= 310) {
      audio_test_beep(1000, 40);
      display_sleep();
      return;
    }
  }

  // 点击上方卡片区域：快速切换翻页/看板模式
  if (y > 26 && y < 152) {
    audio_test_beep(1600, 30);
    s_isFlipMode = !s_isFlipMode;
    renderCurrentClock();
    return;
  }
}
