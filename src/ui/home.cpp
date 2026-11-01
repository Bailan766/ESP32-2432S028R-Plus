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
  int16_t x = 10, y = 34, w = 300, h = 56;

  PlayerState st = player_get_state();
  const TrackInfo* curTrack = player_get_current_track();
  bool playing = (st == STATE_PLAYING);
  bool paused = (st == STATE_PAUSED);

  // 1. 卡片哑光底座与微弱分割轮廓
  tft.fillRoundRect(x, y, w, h, 6, UI_COL_SURFACE);
  tft.drawRoundRect(x, y, w, h, 6, playing ? UI_COL_BORDER_LIGHT : UI_COL_BORDER_SUBTLE);

  // 2. 左侧主状态垂直指示条 (3px)
  uint16_t accentCol = playing ? UI_COL_AMBER : (paused ? UI_COL_CYAN : UI_COL_BORDER_LIGHT);
  tft.fillRect(x + 2, y + 8, 3, h - 16, accentCol);

  // 3. 动态状态图标
  int iconX = x + 12, iconY = y + 14;
  if (playing) {
    tft.fillRoundRect(iconX, iconY, 20, 20, 4, UI_COL_AMBER_DIM);
    tft.drawRoundRect(iconX, iconY, 20, 20, 4, UI_COL_AMBER);
    tft.fillTriangle(iconX + 7, iconY + 5, iconX + 7, iconY + 15, iconX + 15, iconY + 10, UI_COL_AMBER);
  } else if (paused) {
    tft.fillRoundRect(iconX, iconY, 20, 20, 4, UI_COL_SURFACE_HIGH);
    tft.drawRoundRect(iconX, iconY, 20, 20, 4, UI_COL_CYAN);
    tft.fillRect(iconX + 6, iconY + 5, 3, 10, UI_COL_CYAN);
    tft.fillRect(iconX + 11, iconY + 5, 3, 10, UI_COL_CYAN);
  } else {
    tft.fillRoundRect(iconX, iconY, 20, 20, 4, UI_COL_SURFACE_HIGH);
    tft.drawRoundRect(iconX, iconY, 20, 20, 4, UI_COL_BORDER_LIGHT);
    tft.fillRect(iconX + 6, iconY + 6, 8, 8, UI_COL_TEXT_MUTED);
  }

  // 4. 曲目名称与艺术家/规格 (x: 40 ~ 216, 宽 176)
  char titleBuf[32];
  if (curTrack && strlen(curTrack->title) > 0) {
    snprintf(titleBuf, sizeof(titleBuf), "%s", curTrack->title);
  } else {
    snprintf(titleBuf, sizeof(titleBuf), "%s", "未选择曲目");
  }

  // 清除文字背景区域防重叠
  tft.fillRect(x + 38, y + 8, 168, 36, UI_COL_SURFACE);
  font_draw_string_clipped(x + 38, y + 8, 168, titleBuf, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

  char subBuf[48];
  if (curTrack && strlen(curTrack->artist) > 0) {
    snprintf(subBuf, sizeof(subBuf), "%s · DAC2", curTrack->artist);
  } else {
    snprintf(subBuf, sizeof(subBuf), "点击进入完整播放器");
  }
  font_draw_string_clipped(x + 38, y + 28, 168, subBuf, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);

  // 5. 右侧状态指示药丸胶囊 (x: 216, y: 44, w: 86, h: 26)
  int pillX = x + w - 88, pillY = y + 14, pillW = 80, pillH = 26;
  if (playing) {
    ui_theme_draw_pill_btn(pillX, pillY, pillW, pillH, "正在播放", UI_COL_AMBER, UI_COL_AMBER_DIM, UI_COL_AMBER);
  } else if (paused) {
    ui_theme_draw_pill_btn(pillX, pillY, pillW, pillH, "播放暂停", UI_COL_CYAN, UI_COL_SURFACE_HIGH, UI_COL_CYAN);
  } else {
    ui_theme_draw_pill_btn(pillX, pillY, pillW, pillH, "待机就绪", UI_COL_TEXT_MUTED, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);
  }

  // 6. 底沿微刻度时间轴进度条 (y: y + h - 3, h = 2)
  uint16_t elapsed = player_get_elapsed_seconds();
  uint16_t total = player_get_total_seconds();
  if (total == 0) total = 180;
  int progW = (int)((long)elapsed * (w - 8) / total);
  progW = constrain(progW, 0, w - 8);

  tft.fillRect(x + 4, y + h - 3, w - 8, 2, UI_COL_BORDER_SUBTLE);
  if (progW > 0 && playing) {
    tft.fillRect(x + 4, y + h - 3, progW, 2, UI_COL_AMBER);
  }
}

static void renderGridCard(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* title, const char* sub, uint16_t dotCol) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillRoundRect(x, y, w, h, 6, UI_COL_SURFACE);
  tft.drawRoundRect(x, y, w, h, 6, UI_COL_BORDER_SUBTLE);

  // 左侧微圆点指示器
  tft.fillCircle(x + 12, y + 14, 3, dotCol);

  // 标题与副标题
  font_draw_string_clipped(x + 22, y + 5, w - 26, title, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);
  font_draw_string_clipped(x + 22, y + 24, w - 26, sub, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
}

void home_enter() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("摆烂播放器", false);

  // 1. 上位 Hero 播放控制主舱 (x: 10, y: 34, w: 300, h: 56)
  renderHeroPlayerCard();
  s_lastPlayerState = player_get_state();
  s_lastTrack = player_get_current_track();
  s_lastElapsed = player_get_elapsed_seconds();

  // 2. 中位 2x2 极简功能舱 (y: 96 ~ 188)
  // Row 0, Col 0: 曲库音乐 (x: 10, y: 96, w: 146, h: 44)
  char musicSub[32];
  snprintf(musicSub, sizeof(musicSub), "本地曲目 (%d首)", db_get_track_count());
  renderGridCard(10, 96, 146, 44, "曲库音乐", musicSub, UI_COL_AMBER);

  // Row 0, Col 1: 文墨书架 (x: 164, y: 96, w: 146, h: 44)
  char novelSub[32];
  snprintf(novelSub, sizeof(novelSub), "TXT小说 (%d本)", novel_get_count());
  renderGridCard(164, 96, 146, 44, "文墨书架", novelSub, UI_COL_CYAN);

  // Row 1, Col 0: 极客大厅 (x: 10, y: 146, w: 146, h: 44)
  renderGridCard(10, 146, 146, 44, "极客大厅", "电台·传书·摸鱼", UI_COL_GREEN);

  // Row 1, Col 1: 系统控制 (x: 164, y: 146, w: 146, h: 44)
  renderGridCard(164, 146, 146, 44, "系统控制", "耗电·硬件·设置", UI_COL_TEXT_MUTED);

  // 3. 下位极客底座栏 (y: 196 ~ 234, h = 38, 3 个微胶囊按钮)
  // Left: TF 卡状态 (x: 10, w: 96)
  if (sdcard_is_available()) {
    char tfBuf[32];
    uint32_t mb = sdcard_get_size_mb();
    if (mb >= 1024) snprintf(tfBuf, sizeof(tfBuf), "TF:%uG就绪", (unsigned)(mb / 1024));
    else snprintf(tfBuf, sizeof(tfBuf), "TF:%uM就绪", (unsigned)mb);
    ui_theme_draw_pill_btn(10, 196, 96, 36, tfBuf, UI_COL_GREEN, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);
  } else {
    ui_theme_draw_pill_btn(10, 196, 96, 36, "TF:点击挂载", UI_COL_AMBER, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);
  }

  // Center: 蓝牙音箱快捷 (x: 112, w: 96)
  ui_theme_draw_pill_btn(112, 196, 96, 36, "蓝牙音箱", UI_COL_CYAN, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);

  // Right: 桌面时钟快捷 (x: 214, w: 96)
  ui_theme_draw_pill_btn(214, 196, 96, 36, "桌面时钟", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, UI_COL_BORDER_SUBTLE);
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
    audio_test_beep(1200, 30);
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
