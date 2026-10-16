#include "music_list.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "storage/database.h"
#include "player/player.h"

#define ITEMS_PER_PAGE 4

static bool s_isRecent = false;
static int s_currentPage = 0;

static void renderList() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  int totalItems = s_isRecent ? db_get_recent_count() : db_get_track_count();
  int totalPages = (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  if (totalPages == 0) totalPages = 1;
  s_currentPage = constrain(s_currentPage, 0, totalPages - 1);

  // 顶部包含曲目总数的标题
  char headerBuf[32];
  snprintf(headerBuf, sizeof(headerBuf), "%s (共%d首)", s_isRecent ? "最近播放" : "曲库列表", totalItems);
  ui_draw_header(headerBuf, true);

  int startIndex = s_currentPage * ITEMS_PER_PAGE;

  if (totalItems == 0) {
    font_draw_string_center(160, 110, s_isRecent ? "暂无最近播放记录" : "曲库为空 (使用内置曲目)", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  } else {
    for (int i = 0; i < ITEMS_PER_PAGE; i++) {
      int idx = startIndex + i;
      if (idx >= totalItems) break;

      const TrackInfo* track = s_isRecent ? db_get_recent_track(idx) : db_get_track(idx);
      if (!track) continue;

      int y = 32 + i * 40;
      bool isCur = (player_get_current_track() && player_get_current_track()->id == track->id);

      tft.fillRoundRect(10, y, 300, 36, 5, isCur ? 0x1A26 : COLOR_PANEL_BG);
      tft.drawRoundRect(10, y, 300, 36, 5, isCur ? COLOR_ACCENT_GREEN : COLOR_PANEL_BORDER);

      // 序号与曲目名 (支持中文渲染)
      char titleBuf[48];
      snprintf(titleBuf, sizeof(titleBuf), "%02d. %s", track->id + 1, track->title);
      font_draw_string(18, y + 3, titleBuf, isCur ? COLOR_ACCENT_GREEN : COLOR_TEXT_PRIMARY, isCur ? 0x1A26 : COLOR_PANEL_BG, 1);

      // 艺术家与时长
      char subBuf[48];
      snprintf(subBuf, sizeof(subBuf), "%s | %02d:%02d",
               track->artist, track->durationSec / 60, track->durationSec % 60);
      font_draw_string(18, y + 19, subBuf, COLOR_TEXT_MUTED, isCur ? 0x1A26 : COLOR_PANEL_BG, 1);

      // 右侧播放箭头
      font_draw_string(270, y + 10, isCur ? "播放" : ">", isCur ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED, isCur ? 0x1A26 : COLOR_PANEL_BG, 1);
    }
  }

  // 底部翻页栏
  tft.drawLine(0, 192, 320, 192, COLOR_PANEL_BORDER);

  // 上一页按钮
  tft.fillRoundRect(10, 196, 88, 38, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(10, 196, 88, 38, 5, COLOR_PANEL_BORDER);
  font_draw_string_center(54, 206, "< 上一页", COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);

  // 页码显示
  char pageBuf[16];
  snprintf(pageBuf, sizeof(pageBuf), "%d / %d", s_currentPage + 1, totalPages);
  font_draw_string_center(160, 206, pageBuf, COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);

  // 下一页按钮
  tft.fillRoundRect(222, 196, 88, 38, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(222, 196, 88, 38, 5, COLOR_PANEL_BORDER);
  font_draw_string_center(266, 206, "下一页 >", COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);
}

void music_list_enter(bool isRecentMode) {
  if (s_isRecent != isRecentMode || ui_get_previous_page() == UI_PAGE_HOME) {
    s_currentPage = 0;
  }
  s_isRecent = isRecentMode;
  renderList();
}

void music_list_update() {
}

void music_list_handle_touch(int16_t x, int16_t y) {
  // 1. 扩大返回按钮触控响应区域 (左上角 0~95, 0~32)
  if (x >= 0 && x <= 95 && y >= 0 && y <= 32) {
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  int totalItems = s_isRecent ? db_get_recent_count() : db_get_track_count();
  int totalPages = (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  if (totalPages == 0) totalPages = 1;

  // 2. 翻页按键 (y: 190 ~ 240)
  if (y >= 190 && y <= 240) {
    if (x >= 8 && x <= 105) {
      if (s_currentPage > 0) {
        s_currentPage--;
        renderList();
      }
      return;
    } else if (x >= 215 && x <= 312) {
      if (s_currentPage < totalPages - 1) {
        s_currentPage++;
        renderList();
      }
      return;
    }
  }

  // 3. 歌曲条目点击 (4条目)
  for (int i = 0; i < ITEMS_PER_PAGE; i++) {
    int itemY = 32 + i * 40;
    if (y >= itemY - 2 && y <= itemY + 38 && x >= 10 && x <= 310) {
      int itemIdx = s_currentPage * ITEMS_PER_PAGE + i;
      if (itemIdx < totalItems) {
        const TrackInfo* t = s_isRecent ? db_get_recent_track(itemIdx) : db_get_track(itemIdx);
        if (t) {
          player_play_track(t->id);
          ui_switch_to(UI_PAGE_PLAYER);
        }
      }
      return;
    }
  }
}
