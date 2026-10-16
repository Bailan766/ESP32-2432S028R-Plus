#include "muyu.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "config.h"
#include <Preferences.h>

static uint32_t s_meritCount = 0;
static bool s_autoHit = false;
static unsigned long s_lastAutoHit = 0;
static unsigned long s_lastHitTime = 0;
static Preferences s_prefs;

static void loadMerit() {
  s_prefs.begin("muyu", true);
  s_meritCount = s_prefs.getUInt("merit", 0);
  s_prefs.end();
}

static void saveMerit() {
  s_prefs.begin("muyu", false);
  s_prefs.putUInt("merit", s_meritCount);
  s_prefs.end();
}

static void drawMuyu(bool pressed) {
  TFT_eSPI& tft = display_get_tft();
  int cx = 160;
  int cy = 135;

  uint16_t woodColor = pressed ? 0x9A40 : 0xC3E0; // 木鱼木质褐色
  uint16_t shadowColor = 0x6200;
  uint16_t eyeColor = 0x0000;

  // 木鱼主体外形
  int w = pressed ? 116 : 124;
  int h = pressed ? 84 : 90;

  tft.fillRoundRect(cx - w / 2, cy - h / 2, w, h, 28, woodColor);
  tft.drawRoundRect(cx - w / 2, cy - h / 2, w, h, 28, shadowColor);
  tft.drawRoundRect(cx - w / 2 + 1, cy - h / 2 + 1, w - 2, h - 2, 27, shadowColor);

  // 木鱼鱼嘴/开孔弧形
  tft.fillRoundRect(cx - 30, cy + 12, 60, 16, 8, shadowColor);

  // 木鱼鱼眼
  tft.fillCircle(cx - 34, cy - 14, 7, eyeColor);
  tft.fillCircle(cx - 34, cy - 14, 3, 0xFFFF);

  // 头部装饰环
  tft.drawCircle(cx + 26, cy - 10, 12, shadowColor);
}

static void renderMuyuView() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("电子木鱼", true);

  // 顶部功德总计数 (y: 38)
  char buf[32];
  snprintf(buf, sizeof(buf), "当前功德: %u", s_meritCount);
  font_draw_string_center(160, 42, buf, COLOR_ACCENT_ORANGE, COLOR_BG_DARK, 1);

  // 绘制木鱼
  drawMuyu(false);

  // 底部控制按钮 (y: 198, h: 36)
  // [自动敲击: 开/关] (x: 14, y: 198, w: 140, h: 36)
  tft.fillRoundRect(14, 198, 140, 36, 6, 0x10A2);
  tft.drawRoundRect(14, 198, 140, 36, 6, s_autoHit ? COLOR_ACCENT_GREEN : COLOR_PANEL_BORDER);
  font_draw_string_center(84, 207, s_autoHit ? "自动敲击: 开启" : "自动敲击: 关闭", 
                          s_autoHit ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED, 0x10A2, 1);

  // [功德清零] (x: 166, y: 198, w: 140, h: 36)
  tft.fillRoundRect(166, 198, 140, 36, 6, 0x10A2);
  tft.drawRoundRect(166, 198, 140, 36, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(236, 207, "功德清零", COLOR_TEXT_MUTED, 0x10A2, 1);
}

static void hitMuyu() {
  s_meritCount++;
  saveMerit();

  // 播放清脆拟真木鱼顿挫音效 (320Hz 极短微脉冲)
  audio_test_beep(260, 35);

  TFT_eSPI& tft = display_get_tft();
  drawMuyu(true);

  // 浮现飘字：“功德 +1”
  int textY = 74;
  tft.fillRect(100, textY, 120, 20, COLOR_BG_DARK);
  font_draw_string_center(160, textY, "功德 +1", COLOR_ACCENT_GREEN, COLOR_BG_DARK, 1);

  // 刷新功德统计
  char buf[32];
  snprintf(buf, sizeof(buf), "当前功德: %u", s_meritCount);
  tft.fillRect(60, 42, 200, 20, COLOR_BG_DARK);
  font_draw_string_center(160, 42, buf, COLOR_ACCENT_ORANGE, COLOR_BG_DARK, 1);

  s_lastHitTime = millis();
}

void muyu_enter() {
  loadMerit();
  s_autoHit = false;
  renderMuyuView();
}

void muyu_update() {
  if (s_lastHitTime > 0 && millis() - s_lastHitTime >= 120) {
    s_lastHitTime = 0;
    drawMuyu(false);
  }

  // 自动敲击（每秒 1 次）
  if (s_autoHit) {
    if (millis() - s_lastAutoHit >= 1000) {
      s_lastAutoHit = millis();
      hitMuyu();
    }
  }
}

void muyu_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    s_autoHit = false;
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 底部功能按钮 (y >= 190)
  if (y >= 190) {
    if (x >= 14 && x <= 154) {
      // 切换自动敲击
      audio_test_beep(1400, 30);
      s_autoHit = !s_autoHit;
      renderMuyuView();
      return;
    } else if (x >= 166 && x <= 306) {
      // 清零
      audio_test_beep(1800, 40);
      s_meritCount = 0;
      saveMerit();
      renderMuyuView();
      return;
    }
  }

  // 3. 点击中央木鱼
  hitMuyu();
}
