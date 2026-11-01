#include "storage/sdcard.h"
#include "storage/scanner.h"
#include "hardware/audio.h"
#include "music_list.h"
#include "ui.h"
#include "ui_theme.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "storage/database.h"
#include "player/player.h"

#define ITEMS_PER_PAGE 4

static bool s_isRecent = false;
static int s_currentPage = 0;

static void renderList() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  int totalItems = s_isRecent ? db_get_recent_count() : db_get_track_count();
  int totalPages = (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  if (totalPages == 0) totalPages = 1;
  s_currentPage = constrain(s_currentPage, 0, totalPages - 1);

  // 顶部包含曲目总数的标题
  char headerBuf[32];
  snprintf(headerBuf, sizeof(headerBuf), "%s (共%d首)", s_isRecent ? "最近播放" : "曲库列表", totalItems);
  ui_draw_header(headerBuf, true);

  if (!s_isRecent) {
    ui_theme_draw_pill_btn(154, 4, 58, 22, "重扫", UI_COL_AMBER, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);
  }

  int startIndex = s_currentPage * ITEMS_PER_PAGE;

  if (totalItems == 0) {
    if (!s_isRecent && !sdcard_is_available()) {
      font_draw_string_center(160, 80, "未检测到TF卡 (使用内置演示曲)", UI_COL_TEXT_MUTED, UI_COL_BG, 1);
      ui_theme_draw_pill_btn(80, 110, 160, 38, "点击手动挂载TF卡", UI_COL_AMBER, UI_COL_SURFACE, UI_COL_BORDER_LIGHT);
    } else {
      font_draw_string_center(160, 110, s_isRecent ? "暂无最近播放记录" : "曲库为空 (使用内置曲目)", UI_COL_TEXT_MUTED, UI_COL_BG, 1);
    }
  } else {
    for (int i = 0; i < ITEMS_PER_PAGE; i++) {
      int idx = startIndex + i;
      if (idx >= totalItems) break;

      const TrackInfo* track = s_isRecent ? db_get_recent_track(idx) : db_get_track(idx);
      if (!track) continue;

      int y = 33 + i * 39;
      bool isCur = (player_get_current_track() && player_get_current_track()->id == track->id);

      tft.fillRoundRect(10, y, 300, 36, 5, isCur ? UI_COL_SURFACE_HIGH : UI_COL_SURFACE);
      tft.drawRoundRect(10, y, 300, 36, 5, isCur ? UI_COL_BORDER_LIGHT : UI_COL_BORDER_SUBTLE);

      if (isCur) {
        // 当前播放高亮指示条
        tft.fillRect(12, y + 6, 3, 24, UI_COL_AMBER);
      }

      // 序号与曲目名 (支持中文渲染)
      char titleBuf[48];
      snprintf(titleBuf, sizeof(titleBuf), "%02d. %s", track->id + 1, track->title);
      font_draw_string(20, y + 4, titleBuf, isCur ? UI_COL_AMBER : UI_COL_TEXT_PRIMARY, isCur ? UI_COL_SURFACE_HIGH : UI_COL_SURFACE, 1);

      // 艺术家与时长
      char subBuf[48];
      snprintf(subBuf, sizeof(subBuf), "%s | %02d:%02d",
               track->artist, track->durationSec / 60, track->durationSec % 60);
      font_draw_string(20, y + 20, subBuf, UI_COL_TEXT_MUTED, isCur ? UI_COL_SURFACE_HIGH : UI_COL_SURFACE, 1);

      // 右侧播放箭头
      font_draw_string(266, y + 10, isCur ? "播放中" : "▶", isCur ? UI_COL_AMBER : UI_COL_TEXT_DIM, isCur ? UI_COL_SURFACE_HIGH : UI_COL_SURFACE, 1);
    }
  }

  // 底部翻页栏
  tft.drawLine(0, 192, 320, 192, UI_COL_BORDER_SUBTLE);

  // 上一页按钮
  ui_theme_draw_pill_btn(10, 196, 88, 38, "< 上一页", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);

  // 页码显示
  char pageBuf[16];
  snprintf(pageBuf, sizeof(pageBuf), "%d / %d", s_currentPage + 1, totalPages);
  font_draw_string_center(160, 206, pageBuf, UI_COL_AMBER, UI_COL_BG, 1);

  // 下一页按钮
  ui_theme_draw_pill_btn(222, 196, 88, 38, "下一页 >", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);
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
  // 1. 扩大返回按钮触控响应区域
  if (x >= 0 && x <= 95 && y >= 0 && y <= 32) {
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 1.5 顶部重扫按钮 (x: 150..220, y: 0..32)
  if (!s_isRecent && x >= 150 && x <= 220 && y <= 32) {
    audio_test_beep(2000, 60);
    sdcard_reinit(true);
    scanner_scan_all();
    s_currentPage = 0;
    renderList();
    return;
  }

  int totalItems = s_isRecent ? db_get_recent_count() : db_get_track_count();

  // 1.8 曲库为空时点击居中「点击手动挂载TF卡」按钮
  if (totalItems == 0 && !s_isRecent && !sdcard_is_available()) {
    if (x >= 70 && x <= 250 && y >= 105 && y <= 155) {
      audio_test_beep(2000, 80);
      sdcard_reinit(true);
      scanner_scan_all();
      renderList();
      return;
    }
  }

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
    int itemY = 33 + i * 39;
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
