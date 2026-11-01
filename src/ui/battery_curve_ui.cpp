#include "battery_curve_ui.h"
#include "ui.h"
#include "ui_theme.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/battery.h"
#include "hardware/audio.h"

static bool s_modeVoltage = true; // true = 电压(V), false = 百分比(%)
static unsigned long s_lastUpdate = 0;
static uint16_t s_lastHistoryCount = 0;

static const int16_t CHART_X = 36;
static const int16_t CHART_Y = 76;
static const int16_t CHART_W = 276;
static const int16_t CHART_H = 112;

static int16_t calcY(const BatteryPoint& p) {
  if (s_modeVoltage) {
    // 3300mV ~ 4200mV
    int32_t mv = constrain(p.mv, (uint16_t)3300, (uint16_t)4200);
    int16_t y = CHART_Y + CHART_H - 2 - (int16_t)((mv - 3300) * (CHART_H - 4) / 900);
    return constrain(y, (int16_t)(CHART_Y + 2), (int16_t)(CHART_Y + CHART_H - 2));
  } else {
    // 0% ~ 100%
    int32_t pct = constrain((int32_t)p.percent, 0, 100);
    int16_t y = CHART_Y + CHART_H - 2 - (int16_t)(pct * (CHART_H - 4) / 100);
    return constrain(y, (int16_t)(CHART_Y + 2), (int16_t)(CHART_Y + CHART_H - 2));
  }
}

static uint16_t getPointColor(const BatteryPoint& p) {
  if (p.isUsb) return UI_COL_CYAN;
  if (p.percent >= 25) return UI_COL_AMBER;
  return UI_COL_RED;
}

static uint16_t getAreaFillColor(const BatteryPoint& p) {
  if (p.isUsb) return 0x0188;
  if (p.percent >= 25) return 0x3940;
  return 0x2800;
}

static void renderDashboard() {
  TFT_eSPI& tft = display_get_tft();

  // 顶部信息面板 (y: 33 ~ 71, h = 38)
  tft.fillRoundRect(8, 33, 304, 38, 5, UI_COL_SURFACE);
  tft.drawRoundRect(8, 33, 304, 38, 5, UI_COL_BORDER_SUBTLE);

  float v = battery_get_voltage();
  uint8_t pct = battery_get_percent();
  bool usb = battery_is_usb_powered();
  float rate = battery_get_discharge_rate_per_hour();
  float remainingHours = battery_get_estimated_hours_remaining();

  // 第一行: 电压与电量 + 充放电状态
  char line1[48];
  snprintf(line1, sizeof(line1), "当前: %.2fV  %d%%", v, pct);
  font_draw_string(16, 37, line1, usb ? UI_COL_CYAN : UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

  if (usb) {
    font_draw_string(180, 37, "[ 供电/充电中 ]", UI_COL_CYAN, UI_COL_SURFACE, 1);
  } else {
    font_draw_string(180, 37, "[ 电池放电中 ]", UI_COL_AMBER, UI_COL_SURFACE, 1);
  }

  // 第二行: 预估续航与放电速率
  char line2[64];
  if (usb) {
    snprintf(line2, sizeof(line2), "USB持续供电中 | 采样点: %u/120", battery_history_get_count());
  } else {
    snprintf(line2, sizeof(line2), "预估续航:约%.1f小时 | 速率:%.1f%%/h", remainingHours, rate);
  }
  font_draw_string(16, 53, line2, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
}

static void renderChart() {
  TFT_eSPI& tft = display_get_tft();

  // 图表背景与边框
  tft.fillRect(CHART_X, CHART_Y, CHART_W, CHART_H, UI_COL_SURFACE);
  tft.drawRect(CHART_X, CHART_Y, CHART_W, CHART_H, UI_COL_BORDER_SUBTLE);

  // 清除 Y 轴刻度文字区 (x: 0 ~ 35, y: 72 ~ 190)
  tft.fillRect(0, 72, 35, 118, UI_COL_BG);

  // 绘制 Y 轴刻度标签与水平网格虚线
  tft.setTextSize(1);
  tft.setTextColor(UI_COL_TEXT_MUTED, UI_COL_BG);

  if (s_modeVoltage) {
    tft.drawString("4.2V", 4, CHART_Y + 2);
    tft.drawString("3.9V", 4, CHART_Y + CHART_H / 3 - 3);
    tft.drawString("3.6V", 4, CHART_Y + (CHART_H * 2) / 3 - 3);
    tft.drawString("3.3V", 4, CHART_Y + CHART_H - 10);
  } else {
    tft.drawString("100%", 4, CHART_Y + 2);
    tft.drawString("66%",  4, CHART_Y + CHART_H / 3 - 3);
    tft.drawString("33%",  4, CHART_Y + (CHART_H * 2) / 3 - 3);
    tft.drawString("0%",   4, CHART_Y + CHART_H - 10);
  }

  // 水平网格线
  int yMid1 = CHART_Y + CHART_H / 3;
  int yMid2 = CHART_Y + (CHART_H * 2) / 3;
  for (int x = CHART_X + 2; x < CHART_X + CHART_W - 2; x += 6) {
    tft.drawPixel(x, yMid1, UI_COL_BORDER_SUBTLE);
    tft.drawPixel(x, yMid2, UI_COL_BORDER_SUBTLE);
  }

  // 垂直网格线 (对应 -40m, -20m)
  int xT1 = CHART_X + CHART_W / 3;
  int xT2 = CHART_X + (CHART_W * 2) / 3;
  for (int y = CHART_Y + 2; y < CHART_Y + CHART_H - 2; y += 6) {
    tft.drawPixel(xT1, y, UI_COL_BORDER_SUBTLE);
    tft.drawPixel(xT2, y, UI_COL_BORDER_SUBTLE);
  }

  // X 轴时间刻度标签 (y: 190 ~ 200)
  tft.fillRect(CHART_X, 190, CHART_W, 12, UI_COL_BG);
  tft.drawString("-60m", CHART_X, 191);
  tft.drawString("-40m", xT1 - 10, 191);
  tft.drawString("-20m", xT2 - 10, 191);
  font_draw_string(CHART_X + CHART_W - 24, 190, "现在", UI_COL_TEXT_MUTED, UI_COL_BG, 1);

  // 绘制数据折线与阴影填充
  uint16_t count = battery_history_get_count();
  const BatteryPoint* pts = battery_history_get_points();
  if (count == 0 || pts == nullptr) return;

  int16_t prevX = -1;
  int16_t prevY = -1;
  float dx = (float)(CHART_W - 4) / (float)(BATTERY_HISTORY_MAX - 1);

  for (uint16_t i = 0; i < count; i++) {
    int16_t x;
    if (count < BATTERY_HISTORY_MAX) {
      x = CHART_X + 2 + (int16_t)(i * (CHART_W - 4) / (count > 1 ? (count - 1) : 1));
    } else {
      x = CHART_X + 2 + (int16_t)(i * dx);
    }
    int16_t y = calcY(pts[i]);

    if (i > 0 && prevX >= 0) {
      // 区域纵向填充
      uint16_t fillCol = getAreaFillColor(pts[i]);
      for (int16_t curX = prevX; curX <= x; curX++) {
        int16_t curY = prevY + (y - prevY) * (curX - prevX) / ((x - prevX) > 0 ? (x - prevX) : 1);
        tft.drawFastVLine(curX, curY + 1, (CHART_Y + CHART_H - 2) - curY, fillCol);
      }
      // 折线 (主线)
      uint16_t lineCol = getPointColor(pts[i]);
      tft.drawLine(prevX, prevY, x, y, lineCol);
      tft.drawLine(prevX, prevY + 1, x, y + 1, lineCol);
    }

    prevX = x;
    prevY = y;
  }

  // 最新数据点圆点
  if (count > 0 && prevX >= 0 && prevY >= 0) {
    uint16_t dotCol = getPointColor(pts[count - 1]);
    tft.fillCircle(prevX, prevY, 3, UI_COL_TEXT_PRIMARY);
    tft.drawCircle(prevX, prevY, 4, dotCol);

    char curValStr[16];
    if (s_modeVoltage) {
      snprintf(curValStr, sizeof(curValStr), "%.2fV", pts[count - 1].mv / 1000.0f);
    } else {
      snprintf(curValStr, sizeof(curValStr), "%d%%", pts[count - 1].percent);
    }
    int textX = prevX - 34;
    int textY = prevY - 14;
    if (textX < CHART_X + 4) textX = prevX + 6;
    if (textY < CHART_Y + 4) textY = prevY + 6;
    tft.setTextColor(UI_COL_TEXT_PRIMARY, UI_COL_SURFACE);
    tft.drawString(curValStr, textX, textY);
  }
}

static void renderButtons() {
  // 底部按钮栏 (y: 204 ~ 236, h = 32)
  // 1. 切换模式按钮 (x: 10, w: 94)
  const char* modeText = s_modeVoltage ? "模式: 电压V" : "模式: 百分比";
  ui_theme_draw_pill_btn(10, 204, 94, 32, modeText, UI_COL_CYAN, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);

  // 2. 重置曲线按钮 (x: 113, w: 94)
  ui_theme_draw_pill_btn(113, 204, 94, 32, "重置曲线", UI_COL_TEXT_MUTED, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);

  // 3. 硬件自检按钮 (x: 216, w: 94)
  ui_theme_draw_pill_btn(216, 204, 94, 32, "硬件自检", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);
}

void battery_curve_ui_enter() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("电池耗电曲线", true);
  renderDashboard();
  renderChart();
  renderButtons();

  s_lastUpdate = millis();
  s_lastHistoryCount = battery_history_get_count();
}

void battery_curve_ui_update() {
  if (millis() - s_lastUpdate >= 2000) {
    s_lastUpdate = millis();
    renderDashboard();

    uint16_t curCount = battery_history_get_count();
    if (curCount != s_lastHistoryCount) {
      s_lastHistoryCount = curCount;
      renderChart();
    }
  }
}

void battery_curve_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    UIPage prev = ui_get_previous_page();
    if (prev != UI_PAGE_SPLASH && prev != UI_PAGE_BATTERY_CURVE) {
      ui_switch_to(prev);
    } else {
      ui_switch_to(UI_PAGE_SETTINGS);
    }
    return;
  }

  // 2. 底部功能按钮 (y: 202 ~ 238)
  if (y >= 202 && y <= 238) {
    if (x >= 10 && x <= 104) {
      s_modeVoltage = !s_modeVoltage;
      audio_test_beep(1400, 30);
      renderButtons();
      renderChart();
      return;
    }

    if (x >= 113 && x <= 207) {
      battery_history_clear();
      s_lastHistoryCount = battery_history_get_count();
      audio_test_beep(800, 40);
      renderDashboard();
      renderChart();
      return;
    }

    if (x >= 216 && x <= 310) {
      audio_test_beep(1000, 40);
      ui_switch_to(UI_PAGE_HARDWARE_TEST);
      return;
    }
  }

  // 3. 点击图表区域直接切换 V / % 模式
  if (x >= CHART_X && x <= CHART_X + CHART_W && y >= CHART_Y && y <= CHART_Y + CHART_H) {
    s_modeVoltage = !s_modeVoltage;
    audio_test_beep(1200, 30);
    renderButtons();
    renderChart();
    return;
  }
}
