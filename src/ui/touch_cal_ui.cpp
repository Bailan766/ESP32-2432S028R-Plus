#include "touch_cal_ui.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/touch.h"
#include "hardware/audio.h"
#include "hardware/buttons.h"

enum CalSubState {
  CAL_STATE_POINTS,
  CAL_STATE_CANVAS
};

static CalSubState s_calState = CAL_STATE_POINTS;
static int s_currentPoint = 0;

static const int TARGET_X[4] = { 30, 290, 290,  30 };
static const int TARGET_Y[4] = { 30,  30, 210, 210 };
static const char* TARGET_NAMES[4] = { "左上角", "右上角", "右下角", "左下角" };

static uint16_t s_sampleRawX[4];
static uint16_t s_sampleRawY[4];
static bool s_waitingRelease = false;
static unsigned long s_pointSampleTime = 0;
static unsigned long s_enterTime = 0;

static int16_t s_prevCanvasX = -1;
static int16_t s_prevCanvasY = -1;

// 校准页必须看得见：进入时临时拉满背光，退出时恢复原亮度。
// 否则用户把亮度调到很低后进校准页，几乎看不到红色十字，只能盲点。
static uint8_t s_prevBrightness = 80;
static bool s_brightnessBoosted = false;

static void boostBrightness() {
  if (s_brightnessBoosted) return;
  s_prevBrightness = display_get_brightness();
  display_set_brightness(100);
  s_brightnessBoosted = true;
}

static void restoreBrightness() {
  if (!s_brightnessBoosted) return;
  display_set_brightness(s_prevBrightness);
  s_brightnessBoosted = false;
}

static void drawCrosshair(int x, int y, uint16_t color) {
  TFT_eSPI& tft = display_get_tft();
  tft.drawCircle(x, y, 14, color);
  tft.drawCircle(x, y, 15, color);
  tft.drawCircle(x, y, 4, color);
  tft.fillCircle(x, y, 2, TFT_WHITE);
  tft.drawLine(x - 22, y, x + 22, y, color);
  tft.drawLine(x, y - 22, x, y + 22, color);
}

static void exitCalibration() {
  restoreBrightness();
  if (ui_get_previous_page() == UI_PAGE_SETTINGS) {
    ui_switch_to(UI_PAGE_SETTINGS);
  } else {
    ui_switch_to(UI_PAGE_HOME);
  }
}

static void renderTargetPoint(int idx) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  // 顶部说明
  font_draw_string_center(160, 10, "触屏精准校准 (4点法)", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);

  char stepBuf[64];
  snprintf(stepBuf, sizeof(stepBuf), "第 %d/4 点: 请点击 %s 十字中心", idx + 1, TARGET_NAMES[idx]);
  font_draw_string_center(160, 36, stepBuf, COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);

  font_draw_string_center(160, 105, "使用指尖或触控笔轻触红色十字", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 130, "短按实体 BOOT 键跳过校准", COLOR_ACCENT_ORANGE, COLOR_BG_DARK, 1);

  // 绘制当前标靶十字
  drawCrosshair(TARGET_X[idx], TARGET_Y[idx], TFT_RED);
}

static void renderCanvasUI() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  // 顶部标题
  tft.fillRect(0, 0, 320, 30, COLOR_PANEL_BG);
  tft.drawFastHLine(0, 30, 320, COLOR_PANEL_BORDER);
  font_draw_string(10, 8, "校准成功！请在屏幕上滑动测试", TFT_GREEN, COLOR_PANEL_BG, 1);

  // 右上角完成返回按钮 (x: 220 ~ 316, y: 3 ~ 27)
  tft.fillRoundRect(224, 4, 92, 23, 4, 0x03E0);
  font_draw_string_center(270, 7, "保存完成", TFT_WHITE, 0x03E0, 1);

  // 底部控制按钮 (y: 210 ~ 238)
  tft.fillRect(0, 210, 320, 30, COLOR_PANEL_BG);
  tft.drawFastHLine(0, 210, 320, COLOR_PANEL_BORDER);

  tft.fillRoundRect(8, 213, 86, 24, 4, 0x2124);
  font_draw_string_center(51, 217, "重新校准", COLOR_ACCENT_ORANGE, 0x2124, 1);

  tft.fillRoundRect(102, 213, 60, 24, 4, 0x2124);
  font_draw_string_center(132, 217, "清屏", COLOR_TEXT_PRIMARY, 0x2124, 1);

  font_draw_string(172, 217, "滑动画线实时跟随", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
}

void touch_cal_ui_enter() {
  s_calState = CAL_STATE_POINTS;
  s_currentPoint = 0;
  s_waitingRelease = false;
  s_pointSampleTime = 0;
  s_enterTime = millis();
  s_prevCanvasX = -1;
  s_prevCanvasY = -1;

  boostBrightness();   // 校准期间临时拉满背光，确保十字可见
  renderTargetPoint(0);
}

void touch_cal_ui_update() {
  TFT_eSPI& tft = display_get_tft();

  // 无论处于 4点采集阶段 还是 画板阶段，短按物理 BOOT 键直接跳过/退出校准
  if (millis() - s_enterTime > 300 && button_is_pressed()) {
    audio_test_beep(1000, 60);
    exitCalibration();
    return;
  }

  // 处于画板测试阶段时
  if (s_calState == CAL_STATE_CANVAS) {
    int16_t tx = 0, ty = 0;
    if (touch_read(tx, ty)) {
      // 1. 检查右上角 "保存完成" (x: 220 ~ 320, y: 0 ~ 35)
      if (tx >= 220 && ty <= 35) {
        touch_save_calibration();
        audio_test_beep(1200, 60);
        exitCalibration();
        return;
      }

      // 2. 检查底部 "重新校准" (x: 0 ~ 95, y: 205 ~ 240)
      if (tx <= 95 && ty >= 205) {
        audio_test_beep(600, 60);
        touch_cal_ui_enter();
        return;
      }

      // 3. 检查底部 "清屏" (x: 96 ~ 170, y: 205 ~ 240)
      if (tx >= 96 && tx <= 170 && ty >= 205) {
        tft.fillRect(0, 31, 320, 178, COLOR_BG_DARK);
        s_prevCanvasX = -1;
        s_prevCanvasY = -1;
        return;
      }

      // 4. 处于测试画板区域 (y: 31 ~ 205)
      if (ty >= 31 && ty <= 205) {
        if (s_prevCanvasX >= 0) {
          tft.drawLine(s_prevCanvasX, s_prevCanvasY, tx, ty, TFT_CYAN);
          tft.drawLine(s_prevCanvasX + 1, s_prevCanvasY, tx + 1, ty, TFT_CYAN);
        }
        tft.fillCircle(tx, ty, 2, TFT_GREEN);
        s_prevCanvasX = tx;
        s_prevCanvasY = ty;
      }
    } else {
      s_prevCanvasX = -1;
      s_prevCanvasY = -1;
    }
    return;
  }

  // 状态 1: 4点目标采集
  if (s_calState == CAL_STATE_POINTS) {
    // 刚采集完一个点，等待手指抬起
    if (s_waitingRelease) {
      // 手指抬起，或等待超过 350ms 强制放行（彻底防止外壳挤压边缘导致无法释放）
      if (!touch_is_touched() || (millis() - s_pointSampleTime > 350)) {
        s_waitingRelease = false;

        if (s_currentPoint < 4) {
          renderTargetPoint(s_currentPoint);
        } else {
          // 4点全部采集完成，执行仿射求解
          Serial.printf("[CAL] 4点采集完成，开始解算仿射参数...\n");
          bool ok = touch_calibrate_from_4points(TARGET_X, TARGET_Y, s_sampleRawX, s_sampleRawY);
          if (ok) {
            audio_test_beep(1760, 150);
            s_calState = CAL_STATE_CANVAS;
            s_enterTime = millis();
            renderCanvasUI();
          } else {
            // 求解失败重新开始
            Serial.printf("[CAL] 仿射解算失败，重新开始校准\n");
            audio_test_beep(400, 120);
            s_currentPoint = 0;
            renderTargetPoint(0);
          }
        }
      }
      return;
    }

    // 页面刚进入 300ms 内不采样本，避免页面切换瞬间残留动作误判
    if (millis() - s_enterTime > 300 && touch_is_touched()) {
      uint16_t rx = 0, ry = 0, rz = 0;
      if (touch_read_raw(rx, ry, rz)) {
        s_sampleRawX[s_currentPoint] = rx;
        s_sampleRawY[s_currentPoint] = ry;

        Serial.printf("[CAL] 采样点 %d/4: Raw=(%u, %u) 目标=(%d, %d)\n",
                      s_currentPoint + 1, rx, ry,
                      TARGET_X[s_currentPoint], TARGET_Y[s_currentPoint]);

        // 标靶变绿高亮反馈
        drawCrosshair(TARGET_X[s_currentPoint], TARGET_Y[s_currentPoint], TFT_GREEN);

        // 触点确认短鸣音 (880Hz, 50ms)
        audio_test_beep(880, 50);

        s_currentPoint++;
        s_waitingRelease = true;
        s_pointSampleTime = millis();
      }
    }
  }
}

void touch_cal_ui_handle_touch(int16_t x, int16_t y) {
  // 触摸主逻辑在 update 中通过非阻塞采样直接处理
}
