#include "game_menu.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "ui/ui_theme.h"
#include "config.h"

static void renderGameCard(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* title, const char* sub, uint16_t dotCol) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillRoundRect(x, y, w, h, 6, UI_COL_SURFACE);
  tft.drawRoundRect(x, y, w, h, 6, UI_COL_BORDER_SUBTLE);

  // 左侧微圆点指示器
  tft.fillCircle(x + 12, y + 14, 3, dotCol);

  font_draw_string(x + 22, y + 7, title, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);
  font_draw_string(x + 22, y + 26, sub, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
}

static void renderGameMenu() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("极客功能大厅", true);

  // 1. 卡片 0: 网络收音机 (Row 0, Col 0: x=12, y=36, w=144, h=48)
  renderGameCard(12, 36, 144, 48, "网络收音机", "在线流媒体电台", UI_COL_AMBER);

  // 2. 卡片 1: 空中传歌/书 (Row 0, Col 1: x=164, y=36, w=144, h=48)
  renderGameCard(164, 36, 144, 48, "空中传歌/书", "局域网拖拽直传", UI_COL_CYAN);

  // 3. 卡片 2: 蓝牙音箱 (Row 1, Col 0: x=12, y=89, w=144, h=48)
  renderGameCard(12, 89, 144, 48, "蓝牙音箱", "手机无线推歌", UI_COL_CYAN);

  // 4. 卡片 3: 电子木鱼 (Row 1, Col 1: x=164, y=89, w=144, h=48)
  renderGameCard(164, 89, 144, 48, "电子木鱼", "功德+1 自动积攒", UI_COL_AMBER);

  // 5. 卡片 4: 2048 经典 (Row 2, Col 0: x=12, y=142, w=144, h=48)
  renderGameCard(12, 142, 144, 48, "2048 经典", "滑屏数字消除", UI_COL_GREEN);

  // 6. 卡片 5: 复古翻页钟 (Row 2, Col 1: x=164, y=142, w=144, h=48)
  renderGameCard(164, 142, 144, 48, "复古翻页钟", "拟物机械桌面摆件", UI_COL_TEXT_PRIMARY);

  // 底部信息条 (y: 196, h: 34)
  tft.fillRoundRect(12, 196, 296, 34, 6, UI_COL_SURFACE);
  tft.drawRoundRect(12, 196, 296, 34, 6, UI_COL_BORDER_SUBTLE);
  font_draw_string_center(160, 204, "极客玩物 · 摆烂摸鱼，快乐至上", UI_COL_TEXT_DIM, UI_COL_SURFACE, 1);
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
    if (x >= 12 && x <= 156) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_WEB_RADIO);
      return;
    } else if (x >= 164 && x <= 308) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_WEB_MANAGER);
      return;
    }
  }

  // 3. 第二行 (y: 89..139)
  if (y >= 89 && y <= 139) {
    if (x >= 12 && x <= 156) {
      audio_test_beep(1800, 40);
      ui_switch_to(UI_PAGE_BLUETOOTH_SINK);
      return;
    } else if (x >= 164 && x <= 308) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_MUYU);
      return;
    }
  }

  // 4. 第三行 (y: 142..192)
  if (y >= 142 && y <= 192) {
    if (x >= 12 && x <= 156) {
      audio_test_beep(1500, 40);
      ui_switch_to(UI_PAGE_GAME_2048);
      return;
    } else if (x >= 164 && x <= 308) {
      audio_test_beep(1600, 40);
      ui_switch_to(UI_PAGE_LOCKSCREEN);
      return;
    }
  }
}
