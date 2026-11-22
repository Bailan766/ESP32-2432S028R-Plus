#include "game_menu.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "ui/ui_theme.h"
#include "config.h"

static void renderGameCard(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* title, const char* sub,
                           uint16_t iconBg, uint16_t iconFg,
                           M3IconType icon, bool scalloped) {
  ui_theme_draw_panel(x, y, w, h, 12, UI_COL_SURFACE, UI_COL_SURFACE);
  ui_theme_draw_icon_badge(x + 21, y + h / 2, 26, iconBg, iconFg, icon, scalloped);

  font_draw_string_bold(x + 40, y + 6, w - 44, title, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);
  font_draw_string_clipped(x + 40, y + 25, w - 44, sub, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
}

static void renderGameMenu() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("极客功能大厅", true);

  // 1. 卡片 0: 网络收音机
  renderGameCard(12, 36, 144, 48, "网络收音", "在线电台", M3_PRIMARY_CONTAINER, M3_PRIMARY, M3_ICON_RADIO, true);

  // 2. 卡片 1: 空中传歌/书
  renderGameCard(164, 36, 144, 48, "空中传书", "遥控与传歌", M3_TERTIARY_CONTAINER, M3_TERTIARY, M3_ICON_SD, false);

  // 3. 卡片 2: 蓝牙音箱
  renderGameCard(12, 89, 144, 48, "蓝牙音箱", "无线推流", M3_SECONDARY_CONTAINER, M3_PRIMARY, M3_ICON_BT, false);

  // 4. 卡片 3: 电子木鱼
  renderGameCard(164, 89, 144, 48, "电子木鱼", "功德自动攒", M3_TERTIARY_CONTAINER, M3_TERTIARY, M3_ICON_NOTE, true);

  // 5. 卡片 4: 2048 经典
  renderGameCard(12, 142, 144, 48, "2048游戏", "数字滑屏", M3_SAGE_CONTAINER, M3_SAGE, M3_ICON_GAME, false);

  // 6. 卡片 5: 复古翻页钟
  renderGameCard(164, 142, 144, 48, "桌面时钟", "翻页摆件", M3_PRIMARY_CONTAINER, M3_PRIMARY, M3_ICON_CLOCK, true);

  // M3 悬浮胶囊信息栏 (y: 196, h: 34, r: 17)
  ui_theme_draw_badge(12, 196, 296, 34, "Material You · 极客摆件工作台", UI_COL_TEXT_MUTED, UI_COL_SURFACE_HIGH);
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
