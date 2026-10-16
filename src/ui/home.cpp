#include "home.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "storage/sdcard.h"
#include "storage/database.h"
#include "storage/scanner.h"
#include "player/player.h"
#include "reader/novel.h"

static PlayerState s_lastPlayerState = STATE_STOPPED;
static const TrackInfo* s_lastTrack = nullptr;
static unsigned long s_lastHomeUpdate = 0;

static void renderCard0() {
  TFT_eSPI& tft = display_get_tft();
  int16_t x = 14, y = 35, w = 140, h = 48;

  PlayerState st = player_get_state();
  const TrackInfo* curTrack = player_get_current_track();

  const char* title = "音乐播放";
  const char* sub = "点击进入播放器";
  uint16_t color = 0x05E0; // 翠绿

  if (st == STATE_PLAYING) {
    title = "正在播放";
    color = 0x07E0; // 高亮荧光绿
    if (curTrack && strlen(curTrack->title) > 0) {
      sub = curTrack->title;
    } else {
      sub = "正在播放中";
    }
  } else if (st == STATE_PAUSED) {
    title = "播放暂停";
    color = COLOR_ACCENT_ORANGE;
    if (curTrack && strlen(curTrack->title) > 0) {
      sub = curTrack->title;
    } else {
      sub = "点击继续播放";
    }
  }

  tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(x, y, w, h, 6, color);

  font_draw_string(x + 12, y + 7, title, color, COLOR_PANEL_BG, 1);
  font_draw_string(x + 12, y + 26, sub, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
}

void home_enter() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("摆烂播放器", false);

  // 卡片 0: 动态播放器卡片 (Row 0, Col 0: x=14, y=35, w=140, h=48)
  renderCard0();
  s_lastPlayerState = player_get_state();
  s_lastTrack = player_get_current_track();

  // 卡片 1: TXT 小说阅读 (Row 0, Col 1: x=166, y=35, w=140, h=48)
  {
    int16_t x = 166, y = 35, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, COLOR_ACCENT_CYAN);
    font_draw_string(x + 12, y + 7, "小说阅读", COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);

    char subBuf[32];
    snprintf(subBuf, sizeof(subBuf), "书架 (%d本)", novel_get_count());
    font_draw_string(x + 12, y + 26, subBuf, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 卡片 2: 目录文件 / 曲库 (Row 1, Col 0: x=14, y=88, w=140, h=48)
  {
    int16_t x = 14, y = 88, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0xFD20);
    font_draw_string(x + 12, y + 7, "音乐曲库", 0xFD20, COLOR_PANEL_BG, 1);

    char subBuf[32];
    snprintf(subBuf, sizeof(subBuf), "歌曲文件 (%d首)", db_get_track_count());
    font_draw_string(x + 12, y + 26, subBuf, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 卡片 3: 最近播放 (Row 1, Col 1: x=166, y=88, w=140, h=48)
  {
    int16_t x = 166, y = 88, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0x04DF);
    font_draw_string(x + 12, y + 7, "最近播放", 0x04DF, COLOR_PANEL_BG, 1);

    char subBuf[32];
    snprintf(subBuf, sizeof(subBuf), "历史记录 (%d首)", db_get_recent_count());
    font_draw_string(x + 12, y + 26, subBuf, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 卡片 4: 系统设置 (Row 2, Col 0: x=14, y=141, w=140, h=48)
  {
    int16_t x = 14, y = 141, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0x92BB);
    font_draw_string(x + 12, y + 7, "系统设置", 0x92BB, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "参数与硬件管理", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 卡片 5: 摸鱼大厅 / 经典游戏 (Row 2, Col 1: x=166, y=141, w=140, h=48)
  {
    int16_t x = 166, y = 141, w = 140, h = 48;
    tft.fillRoundRect(x, y, w, h, 6, COLOR_PANEL_BG);
    tft.drawRoundRect(x, y, w, h, 6, 0xFD20);
    font_draw_string(x + 12, y + 7, "摸鱼大厅", 0xFD20, COLOR_PANEL_BG, 1);
    font_draw_string(x + 12, y + 26, "2048 / 电子木鱼", COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }

  // 底部状态栏横幅 (x: 14, y: 194, w: 292, h: 36)
  tft.fillRoundRect(14, 194, 292, 36, 6, 0x10A2);
  tft.drawRoundRect(14, 194, 292, 36, 6, COLOR_PANEL_BORDER);

  if (sdcard_is_available()) {
    char buf[48];
    snprintf(buf, sizeof(buf), "TF卡已就绪 (%u MB | 点击重扫)", sdcard_get_size_mb());
    font_draw_string_center(160, 204, buf, TFT_GREEN, 0x10A2, 1);
  } else {
    font_draw_string_center(160, 204, "TF卡: 未检测到 (点击重新扫描)", TFT_YELLOW, 0x10A2, 1);
  }
}

void home_update() {
  if (millis() - s_lastHomeUpdate >= 500) {
    s_lastHomeUpdate = millis();
    PlayerState st = player_get_state();
    const TrackInfo* curTrack = player_get_current_track();

    if (st != s_lastPlayerState || curTrack != s_lastTrack) {
      s_lastPlayerState = st;
      s_lastTrack = curTrack;
      renderCard0();
    }
  }
}

void home_handle_touch(int16_t x, int16_t y) {
  if (y < 30) {
    // 顶部状态栏
    return;
  }

  // 底部 TF 卡状态横幅 (y >= 192): 点击直接重新检测并刷新曲库与小说
  if (y >= 192) {
    audio_test_beep(2000, 80);
    sdcard_reinit();
    scanner_scan_all();
    novel_scan_all();
    home_enter(); // 立即重新渲染首页以刷新容量、曲库数与书籍数
    return;
  }

  // 6 大卡片网格划分 (x分界 160; y分界 86, 139):
  if (x < 160) {
    if (y < 86) {
      ui_switch_to(UI_PAGE_PLAYER);       // 左上: 音乐播放
    } else if (y < 139) {
      ui_switch_to(UI_PAGE_MUSIC_LIST);   // 左中: 音乐曲库
    } else {
      ui_switch_to(UI_PAGE_SETTINGS);     // 左下: 系统设置
    }
  } else {
    if (y < 86) {
      ui_switch_to(UI_PAGE_NOVEL_LIST);   // 右上: 小说阅读 (TXT 书架)
    } else if (y < 139) {
      ui_switch_to(UI_PAGE_RECENT_LIST);  // 右中: 最近播放
    } else {
      ui_switch_to(UI_PAGE_GAME_MENU);    // 右下: 摸鱼大厅 (2048 / 木鱼)
    }
  }
}
