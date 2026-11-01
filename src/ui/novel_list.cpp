#include "novel_list.h"
#include "reader/novel.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "storage/sdcard.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "ui/ui_theme.h"
#include "config.h"

static int s_listPage = 0;
const int ITEMS_PER_PAGE = 3;

static void renderList() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("文墨书架", true);

  // 顶部右上角添加 [重扫] 按钮
  ui_theme_draw_pill_btn(154, 4, 58, 22, "重扫", UI_COL_AMBER, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);

  int totalBooks = novel_get_count();
  int totalPages = (totalBooks + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  if (totalPages == 0) totalPages = 1;
  if (s_listPage >= totalPages) s_listPage = totalPages - 1;
  if (s_listPage < 0) s_listPage = 0;

  int startIdx = s_listPage * ITEMS_PER_PAGE;

  for (int i = 0; i < ITEMS_PER_PAGE; i++) {
    int bookIdx = startIdx + i;
    int16_t x = 12;
    int16_t y = 36 + i * 51;
    int16_t w = 296;
    int16_t h = 46;

    if (bookIdx < totalBooks) {
      const NovelItem* book = novel_get_item(bookIdx);
      tft.fillRoundRect(x, y, w, h, 6, UI_COL_SURFACE);
      tft.drawRoundRect(x, y, w, h, 6, UI_COL_BORDER_SUBTLE);

      // 左侧指示条
      tft.fillRect(x + 2, y + 8, 3, h - 16, book->last_offset > 0 ? UI_COL_AMBER : UI_COL_BORDER_LIGHT);

      // 书名 (支持中文点阵)
      font_draw_string_clipped(x + 14, y + 6, w - 24, book->title, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

      // 来源与容量
      char infoBuf[32];
      if (book->is_demo) {
        snprintf(infoBuf, sizeof(infoBuf), "内置演示 (%.1f KB)", (float)book->file_size / 1024.0f);
      } else {
        if (book->file_size > 1024 * 1024) {
          snprintf(infoBuf, sizeof(infoBuf), "TF卡 (%.1f MB)", (float)book->file_size / (1024.0f * 1024.0f));
        } else {
          snprintf(infoBuf, sizeof(infoBuf), "TF卡 (%u KB)", book->file_size / 1024);
        }
      }
      font_draw_string_clipped(x + 14, y + 26, 190, infoBuf, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);

      // 历史阅读进度
      char progBuf[24];
      if (book->last_offset == 0) {
        strcpy(progBuf, "未读");
      } else {
        float p = ((float)book->last_offset / (float)book->file_size) * 100.0f;
        snprintf(progBuf, sizeof(progBuf), "已读 %.1f%%", constrain(p, 0.0f, 100.0f));
      }
      font_draw_string(x + 210, y + 26, progBuf, book->last_offset > 0 ? UI_COL_AMBER : UI_COL_TEXT_DIM, UI_COL_SURFACE, 1);
    } else {
      // 空位占位
      tft.fillRoundRect(x, y, w, h, 6, UI_COL_SURFACE);
      tft.drawRoundRect(x, y, w, h, 6, UI_COL_BORDER_SUBTLE);
      font_draw_string_center(160, y + 16, "- 暂无更多书籍 -", UI_COL_TEXT_DIM, UI_COL_SURFACE, 1);
    }
  }

  // 底部翻页栏
  tft.drawLine(0, 192, 320, 192, UI_COL_BORDER_SUBTLE);

  // [上页]
  ui_theme_draw_pill_btn(12, 196, 84, 36, "< 上页", (s_listPage > 0) ? UI_COL_TEXT_PRIMARY : UI_COL_TEXT_DIM, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);

  // 页码显示
  char pageBuf[24];
  snprintf(pageBuf, sizeof(pageBuf), "%d / %d", s_listPage + 1, totalPages);
  font_draw_string_center(160, 206, pageBuf, UI_COL_AMBER, UI_COL_BG, 1);

  // [下页]
  bool hasNext = (s_listPage < totalPages - 1);
  ui_theme_draw_pill_btn(224, 196, 84, 36, "下页 >", hasNext ? UI_COL_TEXT_PRIMARY : UI_COL_TEXT_DIM, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);
}

void novel_list_enter() {
  s_listPage = 0;
  renderList();
}

void novel_list_update() {
}

void novel_list_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 顶部重新扫描按钮
  if (x >= 150 && x <= 220 && y <= 32) {
    audio_test_beep(2000, 60);
    sdcard_reinit(true);
    novel_scan_all();
    s_listPage = 0;
    renderList();
    return;
  }

  // 3. 点击书籍列表项
  for (int i = 0; i < ITEMS_PER_PAGE; i++) {
    int itemY = 36 + i * 51;
    if (y >= itemY && y <= itemY + 46 && x >= 12 && x <= 308) {
      int bookIdx = s_listPage * ITEMS_PER_PAGE + i;
      if (bookIdx < novel_get_count()) {
        audio_test_beep(1500, 40);
        if (novel_open(bookIdx)) {
          ui_switch_to(UI_PAGE_NOVEL_READER);
        }
        return;
      }
    }
  }

  // 4. 底部翻页按钮
  if (y >= 194) {
    int totalPages = (novel_get_count() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (totalPages == 0) totalPages = 1;

    // 上页
    if (x >= 10 && x <= 100 && s_listPage > 0) {
      audio_test_beep(1200, 30);
      s_listPage--;
      renderList();
      return;
    }

    // 下页
    if (x >= 220 && x <= 310 && s_listPage < totalPages - 1) {
      audio_test_beep(1200, 30);
      s_listPage++;
      renderList();
      return;
    }
  }
}
