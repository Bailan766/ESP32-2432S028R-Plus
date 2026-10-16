#include "hardware_test_ui.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/touch.h"
#include "hardware/rgb.h"
#include "hardware/audio.h"
#include "hardware/amplifier.h"
#include "hardware/battery.h"
#include "hardware/buttons.h"
#include "storage/sdcard.h"

static int s_testRgbPhase = 0;
static unsigned long s_lastLiveRefresh = 0;
static int16_t s_liveTouchX = -1;
static int16_t s_liveTouchY = -1;

static void renderBase() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("硬件全面自检", true);

  // 1. LCD 测试状态
  font_draw_string(12, 34, "屏幕 LCD (ST7789):", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  font_draw_string(210, 34, "[正常]", TFT_GREEN, COLOR_BG_DARK, 1);
  // 小色块
  tft.fillRect(270, 34, 12, 14, TFT_RED);
  tft.fillRect(285, 34, 12, 14, TFT_GREEN);
  tft.fillRect(300, 34, 12, 14, TFT_BLUE);

  // 2. 功放与音频测试按键 (GPIO26 / GPIO4)
  font_draw_string(12, 58, "功放与音频 (IO4/26):", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  tft.fillRoundRect(210, 56, 102, 22, 4, 0x03E0);
  font_draw_string_center(261, 59, "测音频 1秒", TFT_WHITE, 0x03E0, 1);

  // 3. RGB LED 测试按键 (GPIO 22/17/16)
  font_draw_string(12, 84, "RGB状态灯 (IO22/17/16):", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  tft.fillRoundRect(210, 82, 102, 22, 4, COLOR_PANEL_BG);
  tft.drawRoundRect(210, 82, 102, 22, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(261, 85, "切换色彩", COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);

  // 4. TF 卡状态 (HSPI: 18/19/23/5)
  font_draw_string(12, 110, "TF卡总线 (HSPI):", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  if (sdcard_is_available()) {
    font_draw_string(150, 110, "[正常]", TFT_GREEN, COLOR_BG_DARK, 1);
  } else {
    font_draw_string(150, 110, "[未插卡]", TFT_YELLOW, COLOR_BG_DARK, 1);
  }

  // 触屏校准快捷入口按键
  tft.fillRoundRect(210, 108, 102, 22, 4, 0x10A2);
  tft.drawRoundRect(210, 108, 102, 22, 4, COLOR_ACCENT_CYAN);
  font_draw_string_center(261, 111, "校准触控", COLOR_ACCENT_CYAN, 0x10A2, 1);

  // 分隔线
  tft.drawLine(10, 134, 310, 134, COLOR_PANEL_BORDER);

  // 5. 动态采样区
  font_draw_string(12, 140, "实时动态传感器采样监测:", COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);
}

static void updateDynamicData() {
  TFT_eSPI& tft = display_get_tft();

  // 清除动态区域背景
  tft.fillRect(10, 160, 300, 74, COLOR_BG_DARK);

  // 实体按键 BOOT 状态
  bool bootPressed = button_is_pressed();
  font_draw_string(14, 162, "BOOT 按键 (IO 0):", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  font_draw_string(170, 162, bootPressed ? "[按下 PRESSED]" : "[松开 RELEASED]", bootPressed ? TFT_GREEN : COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);

  // 电池 ADC 与电压
  int rawAdc = battery_get_raw_adc();
  float volt = battery_get_voltage();
  font_draw_string(14, 184, "电池采样 (IO 34):", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  char bBuf[48];
  snprintf(bBuf, sizeof(bBuf), "ADC:%4d | %.2f V (%d%%)", rawAdc, volt, battery_get_percent());
  font_draw_string(150, 184, bBuf, COLOR_ACCENT_GREEN, COLOR_BG_DARK, 1);

  // 实时触摸坐标
  uint16_t rX = 0, rY = 0, rZ1 = 0;
  touch_get_raw(rX, rY, rZ1);
  font_draw_string(14, 206, "触摸采样:", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  char tBuf[64];
  if (s_liveTouchX >= 0) {
    snprintf(tBuf, sizeof(tBuf), "(%3d,%3d) | 原始:(%d,%d)", s_liveTouchX, s_liveTouchY, rX, rY);
    font_draw_string(110, 206, tBuf, TFT_GOLD, COLOR_BG_DARK, 1);
  } else {
    font_draw_string(110, 206, "点击屏幕测试坐标与触点", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  }
}

void hardware_test_ui_enter() {
  s_testRgbPhase = 0;
  s_liveTouchX = -1;
  s_liveTouchY = -1;
  renderBase();
  updateDynamicData();
}

void hardware_test_ui_update() {
  if (millis() - s_lastLiveRefresh >= 250) {
    s_lastLiveRefresh = millis();
    updateDynamicData();
  }
}

void hardware_test_ui_handle_touch(int16_t x, int16_t y) {
  s_liveTouchX = x;
  s_liveTouchY = y;
  TFT_eSPI& tft = display_get_tft();
  tft.fillCircle(x, y, 4, TFT_GREEN);
  tft.drawCircle(x, y, 8, TFT_YELLOW);
  updateDynamicData();

  // 1. 扩大返回按钮触控响应区域 (左上角 0~95, 0~55)
  if (x >= 0 && x <= 95 && y >= 0 && y <= 55) {
    rgb_set_state(RGB_OFF);
    audio_stop();
    ui_switch_to(UI_PAGE_SETTINGS);
    return;
  }

  // 2. 测试音频按键 (x: 200 ~ 318, y: 50 ~ 80)
  if (x >= 200 && x <= 318 && y >= 50 && y <= 80) {
    audio_test_beep(880, 800);
    return;
  }

  // 3. 切换 RGB 色彩按键 (x: 200 ~ 318, y: 80 ~ 106)
  if (x >= 200 && x <= 318 && y >= 80 && y <= 106) {
    s_testRgbPhase = (s_testRgbPhase + 1) % 5;
    if (s_testRgbPhase == 1) rgb_set_color(255, 0, 0);     // 红
    else if (s_testRgbPhase == 2) rgb_set_color(0, 255, 0); // 绿
    else if (s_testRgbPhase == 3) rgb_set_color(0, 0, 255); // 蓝
    else if (s_testRgbPhase == 4) rgb_set_color(180, 180, 180); // 白
    else rgb_set_color(0, 0, 0);                           // 灭
    return;
  }

  // 4. 校准触控按键 (x: 200 ~ 318, y: 106 ~ 132)
  if (x >= 200 && x <= 318 && y >= 106 && y <= 132) {
    ui_switch_to(UI_PAGE_TOUCH_CAL);
    return;
  }
}
