#include "game_menu.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "config.h"

static void renderGameMenu() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("摸鱼大厅", true);

  // 卡片 1: 2048 (x: 14, y: 40, w: 292, h: 68)
  tft.fillRoundRect(14, 40, 292, 68, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(14, 40, 292, 68, 6, 0xFD20);
  font_draw_string(30, 52, "2048 数字消除", 0xFD20, COLOR_PANEL_BG, 1);
  font_draw_string(30, 78, "经典益智滑屏合成，挑战 2048 巅峰", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);

  // 卡片 2: 电子木鱼 (x: 14, y: 120, w: 292, h: 68)
  tft.fillRoundRect(14, 120, 292, 68, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(14, 120, 292, 68, 6, COLOR_ACCENT_GREEN);
  font_draw_string(30, 132, "电子木鱼 (功德+1)", COLOR_ACCENT_GREEN, COLOR_PANEL_BG, 1);
  font_draw_string(30, 158, "沉浸拟真音效，静心修行，自动积攒功德", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);

  // 底部提示
  tft.fillRoundRect(14, 200, 292, 32, 6, 0x10A2);
  tft.drawRoundRect(14, 200, 292, 32, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(160, 208, "摆烂摸鱼，快乐至上", COLOR_ACCENT_CYAN, 0x10A2, 1);
}

void game_menu_enter() {
  renderGameMenu();
}

void game_menu_update() {
}

void game_menu_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 点击卡片 1: 2048 (y: 40..108)
  if (y >= 40 && y <= 108 && x >= 14 && x <= 306) {
    audio_test_beep(1500, 40);
    ui_switch_to(UI_PAGE_GAME_2048);
    return;
  }

  // 3. 点击卡片 2: 电子木鱼 (y: 120..188)
  if (y >= 120 && y <= 188 && x >= 14 && x <= 306) {
    audio_test_beep(1500, 40);
    ui_switch_to(UI_PAGE_MUYU);
    return;
  }
}
