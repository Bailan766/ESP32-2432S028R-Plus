#include "home.h"
#include "ui.h"
#include "ui_theme.h"
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
static uint16_t s_lastElapsed = 0;

static void renderHeroPlayerCard() {
  TFT_eSPI& tft = display_get_tft();
  int16_t x = 10, y = 33, w = 300, h = 58;

  PlayerState st = player_get_state();
  const TrackInfo* curTrack = player_get_current_track();
  bool playing = (st == STATE_PLAYING);
  bool paused = (st == STATE_PAUSED);

  // 1. M3 柔和粉红 -> 玫瑰色渐变主卡片 (大圆角 14px，无生硬边框)
  ui_theme_draw_gradient_card(x, y, w, h, 14, M3_HERO_START, M3_HERO_END);

  // 2. 左侧高对比度深酒红状态圆盘
  int cx = x + 22, cy = y + 26;
  tft.fillCircle(cx, cy, 13, M3_ON_PRIMARY);
  if (playing) {
    tft.fillRect(cx - 4, cy - 5, 3, 10, M3_HERO_START);
    tft.fillRect(cx + 1, cy - 5, 3, 10, M3_HERO_START);
  } else {
    tft.fillTriangle(cx - 3, cy - 5, cx - 3, cy + 5, cx + 5, cy, M3_HERO_START);
  }

  // 3. 高对比度深色粗体主标题 + 常规副标题 (透明背景叠加于渐变上)
  char titleBuf[32];
  if (curTrack && strlen(curTrack->title) > 0) {
    snprintf(titleBuf, sizeof(titleBuf), "%s", curTrack->title);
  } else {
    snprintf(titleBuf, sizeof(titleBuf), "%s", "点击立即播放");
  }
  font_draw_string_bold(x + 42, y + 8, 166, titleBuf, M3_ON_PRIMARY, M3_ON_PRIMARY, 1);

  char subBuf[48];
  if (curTrack && strlen(curTrack->artist) > 0) {
    snprintf(subBuf, sizeof(subBuf), "%s · %s", curTrack->artist, audio_get_eq_name(audio_get_eq()));
  } else {
    snprintf(subBuf, sizeof(subBuf), "Hi-Fi I2S-DAC · %s", audio_get_eq_name(audio_get_eq()));
  }
  font_draw_string_clipped(x + 42, y + 28, 166, subBuf, M3_PRIMARY_CONTAINER, M3_PRIMARY_CONTAINER, 1);

  // 4. 右侧 M3 胶囊状态徽章
  int pillX = x + w - 82, pillY = y + 14, pillW = 72, pillH = 24;
  if (playing) {
    ui_theme_draw_badge(pillX, pillY, pillW, pillH, "播放中", M3_HERO_START, M3_ON_PRIMARY);
  } else if (paused) {
    ui_theme_draw_badge(pillX, pillY, pillW, pillH, "已暂停", M3_HERO_START, M3_PRIMARY_CONTAINER);
  } else {
    ui_theme_draw_badge(pillX, pillY, pillW, pillH, "默认", M3_HERO_START, M3_PRIMARY_CONTAINER);
  }

  // 5. 底沿 M3 圆角进度条
  uint16_t elapsed = player_get_elapsed_seconds();
  uint16_t total = player_get_total_seconds();
  if (total == 0) total = 180;
  int trackW = w - 28;
  int progW = constrain((int)((long)elapsed * trackW / total), 0, trackW);

  tft.fillRoundRect(x + 14, y + h - 7, trackW, 3, 1, M3_PRIMARY_CONTAINER);
  if (progW > 2 && (playing || paused)) {
    tft.fillRoundRect(x + 14, y + h - 7, progW, 3, 1, M3_ON_PRIMARY);
  }
}

static void renderGridCard(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* title, const char* sub,
                           uint16_t iconBg, uint16_t iconFg,
                           M3IconType icon, bool scalloped) {
  // M3 暖梅灰容器层级，大圆角 12px，无生硬边框
  ui_theme_draw_panel(x, y, w, h, 12, UI_COL_SURFACE, UI_COL_SURFACE);

  // 左侧 M3 有机/圆角柔和色块图标底衬
  ui_theme_draw_icon_badge(x + 21, y + h / 2, 26, iconBg, iconFg, icon, scalloped);

  // 右侧双层排版: 粗体主标题 + 常规暖灰副标题
  font_draw_string_bold(x + 40, y + 5, w - 44, title, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);
  font_draw_string_clipped(x + 40, y + 24, w - 44, sub, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
}

void home_enter() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("摆烂播放器", false);

  // 1. 上位 M3 粉玫瑰渐变主卡片 (x: 10, y: 33, w: 300, h: 58)
  renderHeroPlayerCard();
  s_lastPlayerState = player_get_state();
  s_lastTrack = player_get_current_track();
  s_lastElapsed = player_get_elapsed_seconds();

  // 2. 中位 2x2 M3 暖灰容器功能块 (y: 96 ~ 190, 圆角 12px)
  char musicSub[32];
  snprintf(musicSub, sizeof(musicSub), "本地·%d首", db_get_track_count());
  renderGridCard(10, 96, 146, 44, "曲库音乐", musicSub, M3_PRIMARY_CONTAINER, M3_PRIMARY, M3_ICON_NOTE, true);

  char novelSub[32];
  snprintf(novelSub, sizeof(novelSub), "TXT·%d本", novel_get_count());
  renderGridCard(164, 96, 146, 44, "文墨书架", novelSub, M3_TERTIARY_CONTAINER, M3_TERTIARY, M3_ICON_BOOK, false);

  renderGridCard(10, 145, 146, 44, "极客大厅", "电台·传书·玩物", M3_SECONDARY_CONTAINER, M3_PRIMARY, M3_ICON_RADIO, false);
  renderGridCard(164, 145, 146, 44, "系统控制", "音效·电量·设置", UI_COL_SURFACE_PRESS, UI_COL_TEXT_PRIMARY, M3_ICON_SLIDERS, true);

  // 3. 下位 M3 悬浮胶囊底栏 (x: 10, y: 195, w: 300, h: 38, 全半圆胶囊 r=19)
  tft.fillRoundRect(10, 195, 300, 38, 19, UI_COL_SURFACE_HIGH);

  // Left Tab: TF 卡状态
  if (sdcard_is_available()) {
    char tfBuf[24];
    uint32_t mb = sdcard_get_size_mb();
    if (mb >= 1024) snprintf(tfBuf, sizeof(tfBuf), "TF:%uG", (unsigned)(mb / 1024));
    else snprintf(tfBuf, sizeof(tfBuf), "TF:%uM", (unsigned)mb);
    ui_theme_draw_badge(15, 200, 88, 28, tfBuf, M3_SAGE, M3_SAGE_CONTAINER);
  } else {
    ui_theme_draw_badge(15, 200, 88, 28, "挂载TF", M3_TERTIARY, UI_COL_SURFACE_PRESS);
  }

  // Center Active Tab: 蓝牙音箱 (M3 暖粉/深酒红选中胶囊)
  ui_theme_draw_badge(110, 200, 100, 28, "蓝牙音箱", M3_PRIMARY, M3_PRIMARY_CONTAINER);

  // Right Tab: 桌面时钟
  ui_theme_draw_badge(217, 200, 88, 28, "桌面时钟", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_PRESS);
}

void home_update() {
  if (millis() - s_lastHomeUpdate >= 500) {
    s_lastHomeUpdate = millis();
    PlayerState st = player_get_state();
    const TrackInfo* curTrack = player_get_current_track();
    uint16_t el = player_get_elapsed_seconds();

    if (st != s_lastPlayerState || curTrack != s_lastTrack || (st == STATE_PLAYING && abs((int)el - (int)s_lastElapsed) >= 1)) {
      s_lastPlayerState = st;
      s_lastTrack = curTrack;
      s_lastElapsed = el;
      renderHeroPlayerCard();
    }
  }
}

void home_handle_touch(int16_t x, int16_t y) {
  if (y < 30) {
    return;
  }

  // 1. 下位底座栏 (y >= 194)
  if (y >= 194) {
    if (x < 110) {
      // 点击左侧 [TF 卡手动挂载 / 重扫]
      audio_test_beep(2000, 80);
      ui_theme_draw_pill_btn(10, 196, 96, 36, "挂载中...", UI_COL_AMBER, UI_COL_SURFACE_HIGH, UI_COL_AMBER);
      bool ok = sdcard_reinit(true);
      if (ok) {
        scanner_scan_all();
        novel_scan_all();
        audio_test_beep(2500, 100);
      } else {
        audio_test_beep(800, 200);
      }
      home_enter();
      return;
    } else if (x < 210) {
      // 点击中间 [蓝牙音箱]
      audio_test_beep(1800, 40);
      ui_switch_to(UI_PAGE_BLUETOOTH_SINK);
      return;
    } else {
      // 点击右侧 [桌面时钟]
      audio_test_beep(1400, 40);
      ui_switch_to(UI_PAGE_LOCKSCREEN);
      return;
    }
  }

  // 2. 上位 Hero 播放控制舱 (y: 32 ~ 92)
  if (y >= 32 && y <= 92) {
    if (player_get_state() != STATE_PLAYING) {
      player_play();
    } else {
      audio_test_beep(1200, 30);
    }
    ui_switch_to(UI_PAGE_PLAYER);
    return;
  }

  // 3. 中位 2x2 功能舱 (y: 93 ~ 193)
  if (y < 144) {
    // Row 0 (y: 93 ~ 143)
    if (x < 160) {
      audio_test_beep(1000, 30);
      ui_switch_to(UI_PAGE_MUSIC_LIST); // 左上: 曲库音乐
    } else {
      audio_test_beep(1000, 30);
      ui_switch_to(UI_PAGE_NOVEL_LIST); // 右上: 文墨书架
    }
  } else {
    // Row 1 (y: 144 ~ 193)
    if (x < 160) {
      audio_test_beep(1000, 30);
      ui_switch_to(UI_PAGE_GAME_MENU);  // 左下: 极客大厅 (电台/传书/游戏/翻页钟)
    } else {
      audio_test_beep(1000, 30);
      ui_switch_to(UI_PAGE_SETTINGS);   // 右下: 系统控制 (设置/耗电曲线/自检)
    }
  }
}
