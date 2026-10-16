#include "novel_reader_ui.h"
#include "reader/novel.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "player/player.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "config.h"

static bool s_menuVisible = false;

static void drawMenuOverlay() {
  TFT_eSPI& tft = display_get_tft();

  // 顶部快捷控制条 (高度 34px)
  tft.fillRect(0, 0, 320, 34, 0x10A2);
  tft.drawLine(0, 34, 320, 34, COLOR_ACCENT_CYAN);

  // [返回书架] 按钮 (x: 6, y: 4, w: 76, h: 26)
  tft.fillRoundRect(6, 4, 76, 26, 4, 0x2124);
  tft.drawRoundRect(6, 4, 76, 26, 4, COLOR_ACCENT_CYAN);
  font_draw_string(10, 8, "< 书架", COLOR_ACCENT_CYAN, 0x2124, 1);

  // 书籍标题
  const NovelItem* book = novel_get_current_book();
  if (book) {
    char titleBuf[20];
    strncpy(titleBuf, book->title, sizeof(titleBuf) - 1);
    titleBuf[sizeof(titleBuf) - 1] = '\0';
    font_draw_string(88, 8, titleBuf, COLOR_TEXT_PRIMARY, 0x10A2, 1);
  }

  // [退出菜单] 按钮 (x: 238, y: 4, w: 76, h: 26)
  tft.fillRoundRect(238, 4, 76, 26, 4, 0x2124);
  tft.drawRoundRect(238, 4, 76, 26, 4, COLOR_ACCENT_GREEN);
  font_draw_string(254, 8, "完成", COLOR_ACCENT_GREEN, 0x2124, 1);

  // 浮动控制面板 (x: 14, y: 42, w: 292, h: 188)
  tft.fillRoundRect(14, 42, 292, 188, 8, 0x18C3);
  tft.drawRoundRect(14, 42, 292, 188, 8, COLOR_PANEL_BORDER);

  // --- 区域 1: 进度条与跳转 ---
  char pBuf[32];
  snprintf(pBuf, sizeof(pBuf), "阅读进度: %.1f%%", novel_get_progress_percent());
  font_draw_string(26, 50, pBuf, COLOR_TEXT_PRIMARY, 0x18C3, 1);

  // 进度条底槽 (x: 26, y: 72, w: 268, h: 14)
  tft.fillRoundRect(26, 72, 268, 14, 4, 0x0841);
  tft.drawRoundRect(26, 72, 268, 14, 4, COLOR_TEXT_MUTED);

  // 进度指示
  int curW = (int)((novel_get_progress_percent() / 100.0f) * 264.0f);
  curW = constrain(curW, 0, 264);
  if (curW > 0) {
    tft.fillRoundRect(28, 74, curW, 10, 2, COLOR_ACCENT_CYAN);
  }

  // 快速跳转按钮 (y: 92, h: 26): [-5%] [起始] [最新] [+5%]
  tft.fillRoundRect(26, 92, 58, 26, 4, 0x2945);
  tft.drawRoundRect(26, 92, 58, 26, 4, COLOR_PANEL_BORDER);
  font_draw_string(36, 96, "-5%", COLOR_TEXT_PRIMARY, 0x2945, 1);

  tft.fillRoundRect(96, 92, 58, 26, 4, 0x2945);
  tft.drawRoundRect(96, 92, 58, 26, 4, COLOR_PANEL_BORDER);
  font_draw_string(106, 96, "卷首", COLOR_TEXT_PRIMARY, 0x2945, 1);

  tft.fillRoundRect(166, 92, 58, 26, 4, 0x2945);
  tft.drawRoundRect(166, 92, 58, 26, 4, COLOR_PANEL_BORDER);
  font_draw_string(176, 96, "卷尾", COLOR_TEXT_PRIMARY, 0x2945, 1);

  tft.fillRoundRect(236, 92, 58, 26, 4, 0x2945);
  tft.drawRoundRect(236, 92, 58, 26, 4, COLOR_PANEL_BORDER);
  font_draw_string(246, 96, "+5%", COLOR_TEXT_PRIMARY, 0x2945, 1);

  // --- 区域 2: 主题换色 (y: 126) ---
  font_draw_string(26, 128, "阅读主题:", COLOR_TEXT_PRIMARY, 0x18C3, 1);

  NovelTheme curTheme = novel_get_theme();
  const char* themeNames[4] = { "暗黑", "羊皮", "豆沙", "深夜" };
  uint16_t themeBgs[4] = { 0x0000, 0xF7BE, 0xCE79, 0x0845 };
  uint16_t themeTexts[4] = { 0xFFFF, 0x2104, 0x0000, 0xCE79 };

  for (int i = 0; i < 4; i++) {
    int bx = 26 + i * 68;
    int by = 148;
    int bw = 60;
    int bh = 26;

    tft.fillRoundRect(bx, by, bw, bh, 4, themeBgs[i]);
    uint16_t borderCol = (curTheme == i) ? COLOR_ACCENT_GREEN : COLOR_PANEL_BORDER;
    tft.drawRoundRect(bx, by, bw, bh, 4, borderCol);
    if (curTheme == i) {
      tft.drawRoundRect(bx - 1, by - 1, bw + 2, bh + 2, 5, COLOR_ACCENT_GREEN);
    }
    font_draw_string_center(bx + bw / 2, by + 5, themeNames[i], themeTexts[i], themeBgs[i], 1);
  }

  // --- 区域 3: 伴读背景音乐 & 屏幕亮度 (y: 182, h: 36) ---
  // BGM 按钮 (x: 26, y: 182, w: 130, h: 36)
  PlayerState st = player_get_state();
  bool isPlaying = (st == STATE_PLAYING);
  tft.fillRoundRect(26, 182, 130, 36, 4, 0x2124);
  tft.drawRoundRect(26, 182, 130, 36, 4, isPlaying ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED);
  font_draw_string(34, 191, isPlaying ? "BGM: 播放中" : "BGM: 已暂停", isPlaying ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED, 0x2124, 1);

  // 亮度循环调节按钮 (x: 164, y: 182, w: 130, h: 36)
  char bBuf[24];
  snprintf(bBuf, sizeof(bBuf), "背光亮度: %d%%", display_get_brightness());
  tft.fillRoundRect(164, 182, 130, 36, 4, 0x2124);
  tft.drawRoundRect(164, 182, 130, 36, 4, COLOR_ACCENT_CYAN);
  font_draw_string(172, 191, bBuf, COLOR_ACCENT_CYAN, 0x2124, 1);
}

void novel_reader_ui_enter() {
  s_menuVisible = false;
  novel_render_current_page();
}

void novel_reader_ui_update() {
  // 阅读界面无需高频轮询，保持轻量高效
}

void novel_reader_ui_handle_touch(int16_t x, int16_t y) {
  if (s_menuVisible) {
    // 1. [返回书架] (x: 0..85, y: 0..36)
    if (x <= 85 && y <= 36) {
      audio_test_beep(1200, 40);
      s_menuVisible = false;
      novel_close();
      ui_switch_to(UI_PAGE_NOVEL_LIST);
      return;
    }

    // 2. [完成/退出菜单] (x: 230..320, y: 0..36)
    if (x >= 230 && y <= 36) {
      audio_test_beep(1500, 30);
      s_menuVisible = false;
      novel_render_current_page();
      return;
    }

    // 3. 点击/拖动进度条 (x: 20..300, y: 65..88)
    if (y >= 65 && y <= 88 && x >= 26 && x <= 294) {
      float percent = ((float)(x - 26) / 268.0f) * 100.0f;
      audio_test_beep(1400, 30);
      novel_seek_percent(percent);
      drawMenuOverlay();
      return;
    }

    // 4. 快速跳转按钮行 (y: 90..122)
    if (y >= 90 && y <= 122) {
      if (x >= 26 && x <= 84) {
        // -5%
        audio_test_beep(1200, 30);
        novel_seek_percent(novel_get_progress_percent() - 5.0f);
        drawMenuOverlay();
        return;
      } else if (x >= 96 && x <= 154) {
        // 卷首 (0%)
        audio_test_beep(1200, 40);
        novel_seek_percent(0.0f);
        drawMenuOverlay();
        return;
      } else if (x >= 166 && x <= 224) {
        // 卷尾 (99%)
        audio_test_beep(1200, 40);
        novel_seek_percent(99.0f);
        drawMenuOverlay();
        return;
      } else if (x >= 236 && x <= 294) {
        // +5%
        audio_test_beep(1200, 30);
        novel_seek_percent(novel_get_progress_percent() + 5.0f);
        drawMenuOverlay();
        return;
      }
    }

    // 5. 主题选择按钮 (y: 144..176)
    if (y >= 144 && y <= 176) {
      for (int i = 0; i < 4; i++) {
        int bx = 26 + i * 68;
        if (x >= bx && x <= bx + 60) {
          audio_test_beep(1600, 30);
          novel_set_theme((NovelTheme)i);
          drawMenuOverlay();
          return;
        }
      }
    }

    // 6. BGM 播放/暂停 (x: 26..156, y: 180..220)
    if (x >= 26 && x <= 156 && y >= 180 && y <= 220) {
      audio_test_beep(1800, 40);
      player_toggle();
      drawMenuOverlay();
      return;
    }

    // 7. 亮度调节 (x: 164..294, y: 180..220)
    if (x >= 164 && x <= 294 && y >= 180 && y <= 220) {
      audio_test_beep(2000, 30);
      uint8_t curB = display_get_brightness();
      uint8_t nextB = (curB >= 100) ? 20 : (curB + 20);
      display_set_brightness(nextB);
      drawMenuOverlay();
      return;
    }

    // 点击其他空白区域直接退出菜单
    s_menuVisible = false;
    novel_render_current_page();
    return;
  }

  // 未唤出菜单时的阅读触控交互：
  // 屏幕左侧 30% (x < 96): 上一页
  if (x < 96) {
    audio_test_beep(1000, 20);
    novel_prev_page();
    return;
  }

  // 屏幕右侧 30% (x > 224): 下一页
  if (x > 224) {
    audio_test_beep(1400, 20);
    novel_next_page();
    return;
  }

  // 屏幕中间 40% (96 <= x <= 224): 唤出功能菜单
  audio_test_beep(1800, 30);
  s_menuVisible = true;
  drawMenuOverlay();
}
