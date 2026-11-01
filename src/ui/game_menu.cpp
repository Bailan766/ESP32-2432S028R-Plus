#include "game_menu.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "config.h"

static void renderGameMenu() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("摸鱼与功能大厅", true);

  // 1. 卡片 0: 网络收音机 (Row 0, Col 0: x=14, y=36, w=140, h=48)
  {
    int16_t x = 14, y = 36, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, COLOR_ACCENT_GREEN);
    font_draw_string(x + 12, y + 7, "网络收音机", COLOR_ACCENT_GREEN, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "在线流媒体电台", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 2. 卡片 1: 空中传歌/书 (Row 0, Col 1: x=166, y=36, w=140, h=48)
  {
    int16_t x = 166, y = 36, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, COLOR_ACCENT_CYAN);
    font_draw_string(x + 12, y + 7, "空中传歌/书", COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "网页拖拽免拔卡", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 3. 卡片 2: 蓝牙音箱 (Row 1, Col 0: x=14, y=89, w=140, h=48)
  {
    int16_t x = 14, y = 89, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0x07FF);
    font_draw_string(x + 12, y + 7, "蓝牙音箱", 0x07FF, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "手机无线推歌", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 4. 卡片 3: 电子木鱼 (Row 1, Col 1: x=166, y=89, w=140, h=48)
  {
    int16_t x = 166, y = 89, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0xFD20);
    font_draw_string(x + 12, y + 7, "电子木鱼", 0xFD20, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "功德+1 自动积攒", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 5. 卡片 4: 2048 数字消除 (Row 2, Col 0: x=14, y=142, w=140, h=48)
  {
    int16_t x = 14, y = 142, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0xFD20);
    font_draw_string(x + 12, y + 7, "2048 经典", 0xFD20, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "滑屏数字消除", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 6. 卡片 5: 复古翻页钟 (Row 2, Col 1: x=166, y=142, w=140, h=48)
  {
    int16_t x = 166, y = 142, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0x92BB);
    font_draw_string(x + 12, y + 7, "复古翻页钟", 0x92BB, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "拟物机械桌面摆件", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 底部信息条 (y: 196, h: 34)
  tft.fillRoundRect(14, 196, 292, 34, 6, 0x10A2);
  tft.drawRoundRect(14, 196, 292, 34, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(160, 204, "摆烂摸鱼，快乐至上 · 极客玩物", 0x92BB, 0x10A2, 1);
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

  // 2. 第一行 (y: 36..86)
  if (y >= 36 && y <= 86) {
    if (x >= 14 && x <= 154) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_WEB_RADIO);
      return;
    } else if (x >= 166 && x <= 306) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_WEB_MANAGER);
      return;
    }
  }

  // 3. 第二行 (y: 89..139)
  if (y >= 89 && y <= 139) {
    if (x >= 14 && x <= 154) {
      audio_test_beep(1800, 40);
      ui_switch_to(UI_PAGE_BLUETOOTH_SINK);
      return;
    } else if (x >= 166 && x <= 306) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_MUYU);
      return;
    }
  }

  // 4. 第三行 (y: 142..192)
  if (y >= 142 && y <= 192) {
    if (x >= 14 && x <= 154) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_GAME_2048);
      return;
    } else if (x >= 166 && x <= 306) {
      audio_test_beep(1600, 40);
      ui_switch_to(UI_PAGE_LOCKSCREEN);
      return;
    }
  }
}
