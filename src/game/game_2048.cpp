#include "game_2048.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "config.h"
#include <Preferences.h>

static uint16_t s_board[4][4] = {0};
static uint32_t s_score = 0;
static uint32_t s_bestScore = 0;
static bool s_gameOver = false;
static Preferences s_prefs;

// 记录触摸手势起始点
static int16_t s_touchStartX = -1;
static int16_t s_touchStartY = -1;

static void loadBestScore() {
  s_prefs.begin("g2048", true);
  s_bestScore = s_prefs.getUInt("best", 0);
  s_prefs.end();
}

static void saveBestScore() {
  if (s_score > s_bestScore) {
    s_bestScore = s_score;
    s_prefs.begin("g2048", false);
    s_prefs.putUInt("best", s_bestScore);
    s_prefs.end();
  }
}

static void spawnTile() {
  int emptySpots[16][2];
  int count = 0;
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (s_board[r][c] == 0) {
        emptySpots[count][0] = r;
        emptySpots[count][1] = c;
        count++;
      }
    }
  }
  if (count > 0) {
    int idx = random(count);
    int r = emptySpots[idx][0];
    int c = emptySpots[idx][1];
    s_board[r][c] = (random(10) == 0) ? 4 : 2;
  }
}

static void getTileColors(uint16_t val, uint16_t& bg, uint16_t& text) {
  switch (val) {
    case 0:    bg = 0x18C3; text = 0x0000; break;
    case 2:    bg = 0xE73C; text = 0x2104; break;
    case 4:    bg = 0xEF5D; text = 0x2104; break;
    case 8:    bg = 0xFBE0; text = 0xFFFF; break;
    case 16:   bg = 0xF9E0; text = 0xFFFF; break;
    case 32:   bg = 0xF8E0; text = 0xFFFF; break;
    case 64:   bg = 0xE8A0; text = 0xFFFF; break;
    case 128:  bg = 0xEF80; text = 0xFFFF; break;
    case 256:  bg = 0xFFE0; text = 0x0000; break;
    case 512:  bg = 0x07E0; text = 0x0000; break;
    case 1024: bg = 0x07FF; text = 0x0000; break;
    case 2048: bg = 0xFD20; text = 0x0000; break;
    default:   bg = 0x92BB; text = 0xFFFF; break;
  }
}

static void renderBoard() {
  TFT_eSPI& tft = display_get_tft();

  // 1. 棋盘底座 (x: 10, y: 20, w: 204, h: 204)
  tft.fillRoundRect(10, 20, 204, 204, 8, 0x2945);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int tx = 16 + c * 49;
      int ty = 26 + r * 49;
      int tw = 44;
      int th = 44;

      uint16_t val = s_board[r][c];
      uint16_t bg, fg;
      getTileColors(val, bg, fg);

      tft.fillRoundRect(tx, ty, tw, th, 4, bg);

      if (val > 0) {
        char numBuf[12];
        snprintf(numBuf, sizeof(numBuf), "%u", val);
        tft.setTextColor(fg, bg);
        tft.setTextDatum(MC_DATUM);
        if (val < 100) {
          tft.drawString(numBuf, tx + tw / 2, ty + th / 2, 4);
        } else if (val < 1000) {
          tft.drawString(numBuf, tx + tw / 2, ty + th / 2, 2);
        } else {
          tft.drawString(numBuf, tx + tw / 2, ty + th / 2, 1);
        }
      }
    }
  }

  // 2. 右侧信息面板 (x: 220..314)
  // 分数卡片 (x: 222, y: 20, w: 88, h: 42)
  tft.fillRoundRect(222, 20, 88, 42, 6, 0x10A2);
  tft.drawRoundRect(222, 20, 88, 42, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(266, 24, "当前得分", COLOR_TEXT_MUTED, 0x10A2, 1);
  char scoreBuf[16];
  snprintf(scoreBuf, sizeof(scoreBuf), "%u", s_score);
  tft.setTextColor(COLOR_ACCENT_GREEN, 0x10A2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(scoreBuf, 266, 50, 2);

  // 最高分卡片 (x: 222, y: 68, w: 88, h: 42)
  tft.fillRoundRect(222, 68, 88, 42, 6, 0x10A2);
  tft.drawRoundRect(222, 68, 88, 42, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(266, 72, "历史最高", COLOR_TEXT_MUTED, 0x10A2, 1);
  char bestBuf[16];
  snprintf(bestBuf, sizeof(bestBuf), "%u", s_bestScore);
  tft.setTextColor(COLOR_ACCENT_CYAN, 0x10A2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(bestBuf, 266, 98, 2);

  // 3. 触控方向虚拟按键十字盘 (y: 118..214)
  // 上按键 (x: 252, y: 118, w: 28, h: 26)
  tft.fillRoundRect(252, 118, 28, 26, 4, 0x2124);
  tft.drawRoundRect(252, 118, 28, 26, 4, COLOR_ACCENT_CYAN);
  tft.setTextColor(COLOR_ACCENT_CYAN, 0x2124);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("^", 266, 131, 2);

  // 左按键 (x: 222, y: 148, w: 28, h: 26)
  tft.fillRoundRect(222, 148, 28, 26, 4, 0x2124);
  tft.drawRoundRect(222, 148, 28, 26, 4, COLOR_ACCENT_CYAN);
  tft.drawString("<", 236, 161, 2);

  // 重置按键 (x: 252, y: 148, w: 28, h: 26)
  tft.fillRoundRect(252, 148, 28, 26, 4, 0x3104);
  tft.drawRoundRect(252, 148, 28, 26, 4, COLOR_ACCENT_ORANGE);
  font_draw_string_center(266, 154, "新", COLOR_ACCENT_ORANGE, 0x3104, 1);

  // 右按键 (x: 282, y: 148, w: 28, h: 26)
  tft.fillRoundRect(282, 148, 28, 26, 4, 0x2124);
  tft.drawRoundRect(282, 148, 28, 26, 4, COLOR_ACCENT_CYAN);
  tft.drawString(">", 296, 161, 2);

  // 下按键 (x: 252, y: 178, w: 28, h: 26)
  tft.fillRoundRect(252, 178, 28, 26, 4, 0x2124);
  tft.drawRoundRect(252, 178, 28, 26, 4, COLOR_ACCENT_CYAN);
  tft.drawString("v", 266, 191, 2);

  // 底部返回按键 (x: 222, y: 206, w: 88, h: 24)
  tft.fillRoundRect(222, 206, 88, 24, 4, 0x10A2);
  tft.drawRoundRect(222, 206, 88, 24, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(266, 210, "返回大厅", COLOR_TEXT_MUTED, 0x10A2, 1);
}

static void initNewGame() {
  memset(s_board, 0, sizeof(s_board));
  s_score = 0;
  s_gameOver = false;
  spawnTile();
  spawnTile();
}

static bool moveLeft() {
  bool moved = false;
  for (int r = 0; r < 4; r++) {
    uint16_t row[4] = {0};
    int target = 0;
    for (int c = 0; c < 4; c++) {
      if (s_board[r][c] != 0) {
        row[target++] = s_board[r][c];
      }
    }
    // 合并相同
    for (int i = 0; i < 3; i++) {
      if (row[i] != 0 && row[i] == row[i + 1]) {
        row[i] *= 2;
        s_score += row[i];
        row[i + 1] = 0;
        i++;
      }
    }
    // 再次左对齐
    uint16_t newRow[4] = {0};
    target = 0;
    for (int i = 0; i < 4; i++) {
      if (row[i] != 0) newRow[target++] = row[i];
    }
    for (int c = 0; c < 4; c++) {
      if (s_board[r][c] != newRow[c]) {
        s_board[r][c] = newRow[c];
        moved = true;
      }
    }
  }
  return moved;
}

static void rotateBoard() {
  uint16_t temp[4][4];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      temp[c][3 - r] = s_board[r][c];
    }
  }
  memcpy(s_board, temp, sizeof(s_board));
}

static bool moveBoard(int dir) {
  // dir: 0:左, 1:下, 2:右, 3:上
  bool moved = false;
  for (int i = 0; i < dir; i++) rotateBoard();
  moved = moveLeft();
  for (int i = 0; i < (4 - dir) % 4; i++) rotateBoard();

  if (moved) {
    audio_test_beep(1200, 30);
    saveBestScore();
    spawnTile();
    renderBoard();
  }
  return moved;
}

void game_2048_enter() {
  loadBestScore();
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);
  if (s_score == 0) {
    initNewGame();
  }
  renderBoard();
}

void game_2048_update() {
}

void game_2048_handle_touch(int16_t x, int16_t y) {
  // 1. 右下角虚拟控制按键：
  // 上 (x: 252..280, y: 118..144)
  if (x >= 252 && x <= 280 && y >= 118 && y <= 144) {
    moveBoard(3);
    return;
  }
  // 左 (x: 222..250, y: 148..174)
  if (x >= 222 && x <= 250 && y >= 148 && y <= 174) {
    moveBoard(0);
    return;
  }
  // 新游戏 (x: 252..280, y: 148..174)
  if (x >= 252 && x <= 280 && y >= 148 && y <= 174) {
    audio_test_beep(1800, 50);
    initNewGame();
    renderBoard();
    return;
  }
  // 右 (x: 282..310, y: 148..174)
  if (x >= 282 && x <= 310 && y >= 148 && y <= 174) {
    moveBoard(2);
    return;
  }
  // 下 (x: 252..280, y: 178..204)
  if (x >= 252 && x <= 280 && y >= 178 && y <= 204) {
    moveBoard(1);
    return;
  }
  // 返回大厅 (x: 222..310, y: 206..236)
  if (x >= 222 && x <= 310 && y >= 206 && y <= 236) {
    audio_test_beep(1000, 30);
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 棋盘区域滑动手势处理 (x: 10..214, y: 20..224)
  // 如果点击在棋盘内，直接根据相对象限判定移动方向
  if (x >= 10 && x <= 214 && y >= 20 && y <= 224) {
    int cx = 112; // 棋盘中心 X
    int cy = 122; // 棋盘中心 Y
    int dx = x - cx;
    int dy = y - cy;

    if (abs(dx) > abs(dy)) {
      if (dx > 0) moveBoard(2); // 右
      else moveBoard(0);        // 左
    } else {
      if (dy > 0) moveBoard(1); // 下
      else moveBoard(3);        // 上
    }
  }
}
