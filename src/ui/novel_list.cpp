#include "novel_list.h"
#include "reader/novel.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "storage/sdcard.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "config.h"

static int s_listPage = 0;
const int ITEMS_PER_PAGE = 3;

static void renderList() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("小说书架", true);

  // 顶部右上角添加 [重新扫描] 按钮 (x: 154, y: 3, w: 64, h: 24)
  tft.fillRoundRect(154, 3, 64, 24, 4, 0x10A2);
  tft.drawRoundRect(154, 3, 64, 24, 4, COLOR_ACCENT_GREEN);
  font_draw_string(160, 7, "重扫", COLOR_ACCENT_GREEN, 0x10A2, 1);

  int totalBooks = novel_get_count();
  int totalPages = (totalBooks + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  if (totalPages == 0) totalPages = 1;
  if (s_listPage >= totalPages) s_listPage = totalPages - 1;
  if (s_listPage < 0) s_listPage = 0;

  int startIdx = s_listPage * ITEMS_PER_PAGE;

  for (int i = 0; i < ITEMS_PER_PAGE; i++) {
    int bookIdx = startIdx + i;
    int16_t x = 14;
    int16_t y = 36 + i * 53;
    int16_t w = 292;
    int16_t h = 48;

    if (bookIdx < totalBooks) {
      const NovelItem* book = novel_get_item(bookIdx);
      tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
      tft.drawRoundRect(x, y, w, h, 6, COLOR_PANEL_BORDER);

      // 书名 (支持中文点阵)
      font_draw_string(x + 12, y + 8, book->title, COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);

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
      font_draw_string(x + 12, y + 28, infoBuf, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);

      // 历史阅读进度
      char progBuf[24];
      if (book->last_offset == 0) {
        strcpy(progBuf, "未读");
      } else {
        float p = ((float)book->last_offset / (float)book->file_size) * 100.0f;
        snprintf(progBuf, sizeof(progBuf), "已读 %.1f%%", constrain(p, 0.0f, 100.0f));
      }
      font_draw_string(x + 205, y + 28, progBuf, COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);
    } else {
      // 空位占位
      tft.fillRoundRect(x, y, w, h, 6, 0x0841);
      tft.drawRoundRect(x, y, w, h, 6, COLOR_PANEL_BORDER);
      font_draw_string_center(160, y + 16, "- 暂无更多书籍 -", COLOR_TEXT_MUTED, 0x0841, 1);
    }
  }

  // 底部翻页栏 (x: 14, y: 198, w: 292, h: 36)
  // [上页] (x: 14, y: 198, w: 84, h: 34)
  tft.fillRoundRect(14, 198, 84, 34, 6, (s_listPage > 0) ? 0x2124 : 0x10A2);
  tft.drawRoundRect(14, 198, 84, 34, 6, (s_listPage > 0) ? COLOR_ACCENT_CYAN : COLOR_PANEL_BORDER);
  font_draw_string_center(56, 207, "上页", (s_listPage > 0) ? COLOR_ACCENT_CYAN : COLOR_TEXT_MUTED, (s_listPage > 0) ? 0x2124 : 0x10A2, 1);

  // 页码显示
  char pageBuf[24];
  snprintf(pageBuf, sizeof(pageBuf), "第 %d / %d 页", s_listPage + 1, totalPages);
  font_draw_string_center(160, 207, pageBuf, COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);

  // [下页] (x: 222, y: 198, w: 84, h: 34)
  bool hasNext = (s_listPage < totalPages - 1);
  tft.fillRoundRect(222, 198, 84, 34, 6, hasNext ? 0x2124 : 0x10A2);
  tft.drawRoundRect(222, 198, 84, 34, 6, hasNext ? COLOR_ACCENT_CYAN : COLOR_PANEL_BORDER);
  font_draw_string_center(264, 207, "下页", hasNext ? COLOR_ACCENT_CYAN : COLOR_TEXT_MUTED, hasNext ? 0x2124 : 0x10A2, 1);
}

void novel_list_enter() {
  s_listPage = 0;
  renderList();
}

void novel_list_update() {
}

void novel_list_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮 (x: 0..76, y: 0..32)
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 顶部重新扫描按钮 (x: 150..220, y: 0..32)
  if (x >= 150 && x <= 220 && y <= 32) {
    audio_test_beep(2000, 60);
    sdcard_reinit();
    novel_scan_all();
    s_listPage = 0;
    renderList();
    return;
  }

  // 3. 点击书籍列表项 (y: 36..190)
  for (int i = 0; i < ITEMS_PER_PAGE; i++) {
    int itemY = 36 + i * 53;
    if (y >= itemY && y <= itemY + 48 && x >= 14 && x <= 306) {
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

  // 4. 底部翻页按钮 (y >= 195)
  if (y >= 195) {
    int totalPages = (novel_get_count() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (totalPages == 0) totalPages = 1;

    // 上页
    if (x >= 14 && x <= 100 && s_listPage > 0) {
      audio_test_beep(1200, 30);
      s_listPage--;
      renderList();
      return;
    }

    // 下页
    if (x >= 220 && x <= 306 && s_listPage < totalPages - 1) {
      audio_test_beep(1200, 30);
      s_listPage++;
      renderList();
      return;
    }
  }
}
