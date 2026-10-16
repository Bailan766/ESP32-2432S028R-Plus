#include "player_ui.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "player/player.h"

#define SPEC_BARS 16
static int s_specH[SPEC_BARS] = {0};
static unsigned long s_lastSpecTime = 0;
static unsigned long s_lastProgressTime = 0;

static void renderStaticElements() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("正在播放", true);

  // 1. 曲目信息卡片 (y: 34 ~ 80)
  tft.fillRoundRect(10, 34, 300, 46, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(10, 34, 300, 46, 6, COLOR_PANEL_BORDER);
  
  // 息屏按钮

  const TrackInfo* t = player_get_current_track();
  font_draw_string(20, 40, t ? t->title : "未选择曲目", COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);

  char sub[48];
  snprintf(sub, sizeof(sub), "艺术家: %s | %s",
           t ? t->artist : "---",
           player_state_to_string(player_get_state()));
  font_draw_string(20, 60, sub, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);

  // 2. 控制按键第一排 (y: 156 ~ 200)
  // PREV (|<)
  tft.fillRoundRect(10, 156, 58, 42, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(10, 156, 58, 42, 6, COLOR_PANEL_BORDER);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_ACCENT_CYAN);
  tft.drawString("|<", 39, 177, 4);

  // PLAY / PAUSE (主按键)
  bool playing = (player_get_state() == STATE_PLAYING);
  tft.fillRoundRect(76, 152, 80, 48, 8, playing ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_GREEN);
  tft.drawRoundRect(76, 152, 80, 48, 8, COLOR_TEXT_PRIMARY);
  font_draw_string_center(116, 168, playing ? "暂停" : "播放", TFT_BLACK, playing ? COLOR_ACCENT_ORANGE : COLOR_ACCENT_GREEN, 1);

  // NEXT (>|)
  tft.fillRoundRect(164, 156, 58, 42, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(164, 156, 58, 42, 6, COLOR_PANEL_BORDER);
  tft.setTextColor(COLOR_ACCENT_CYAN);
  tft.drawString(">|", 193, 177, 4);

  // MODE (循环模式)
  tft.fillRoundRect(230, 156, 80, 42, 6, COLOR_PANEL_BG);
  tft.drawRoundRect(230, 156, 80, 42, 6, COLOR_PANEL_BORDER);
  const char* mStr = "全部循环";
  if (player_get_mode() == MODE_REPEAT_ONE) mStr = "单曲循环";
  else if (player_get_mode() == MODE_SHUFFLE) mStr = "随机播放";
  else if (player_get_mode() == MODE_SEQUENTIAL) mStr = "顺序播放";
  font_draw_string_center(270, 169, mStr, COLOR_ACCENT_GREEN, COLOR_PANEL_BG, 1);

  // 3. 控制按键第二排 (快退, 音量, 快进)
  // -5s 快退
  tft.fillRoundRect(10, 206, 68, 28, 4, 0x10A2);
  tft.drawRoundRect(10, 206, 68, 28, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(44, 212, "快退 -5s", COLOR_TEXT_PRIMARY, 0x10A2, 1);

  // VOL 音量
  tft.fillRoundRect(86, 206, 136, 28, 4, 0x10A2);
  tft.drawRoundRect(86, 206, 136, 28, 4, COLOR_PANEL_BORDER);
  char volBuf[32];
  snprintf(volBuf, sizeof(volBuf), "音量: %d%%", audio_get_volume());
  font_draw_string_center(154, 212, volBuf, COLOR_ACCENT_CYAN, 0x10A2, 1);

  // +5s 快进
  tft.fillRoundRect(230, 206, 80, 28, 4, 0x10A2);
  tft.drawRoundRect(230, 206, 80, 28, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(270, 212, "快进 +5s", COLOR_TEXT_PRIMARY, 0x10A2, 1);

  tft.setTextDatum(TL_DATUM);
}

static void updateProgressBar() {
  TFT_eSPI& tft = display_get_tft();

  uint16_t elapsed = player_get_elapsed_seconds();
  uint16_t total = player_get_total_seconds();
  if (total == 0) total = 180;

  // 时间显示
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", elapsed / 60, elapsed % 60);
  tft.fillRect(10, 134, 45, 14, COLOR_BG_DARK);
  tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG_DARK);
  tft.drawString(timeBuf, 10, 134, 2);

  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", total / 60, total % 60);
  tft.fillRect(265, 134, 45, 14, COLOR_BG_DARK);
  tft.drawString(timeBuf, 270, 134, 2);

  // 进度条主体 (x: 58 ~ 260, 宽 202)
  int barX = 58, barY = 140, barW = 202, barH = 4;
  int prog = (int)((long)elapsed * barW / total);
  prog = constrain(prog, 0, barW);

  tft.fillRect(barX, barY, prog, barH, COLOR_ACCENT_CYAN);
  tft.fillRect(barX + prog, barY, barW - prog, barH, 0x3186);
  tft.fillCircle(barX + prog, barY + 2, 4, COLOR_TEXT_PRIMARY);
}

static void updateVisualizer() {
  if (millis() - s_lastSpecTime < 40) return; // ~25 FPS
  s_lastSpecTime = millis();

  TFT_eSPI& tft = display_get_tft();
  int startX = 14, startY = 128, maxH = 42, barW = 14, gap = 4;
  uint16_t freq = player_get_current_note_freq();
  bool playing = (player_get_state() == STATE_PLAYING);

  for (int i = 0; i < SPEC_BARS; i++) {
    int targetH = 0;
    if (playing && freq > 0) {
      int center = (freq / 65) % SPEC_BARS;
      int dist = abs(i - center);
      int pulse = random(max(4, maxH - dist * 7), maxH);
      targetH = constrain(pulse, 2, maxH);
    }

    if (s_specH[i] < targetH) {
      s_specH[i] = targetH;
    } else {
      s_specH[i] = max(0, s_specH[i] - 4);
    }

    int h = s_specH[i];
    int bx = startX + i * (barW + gap);
    int by = startY - h;

    if (maxH - h > 0) {
      tft.fillRect(bx, startY - maxH, barW, maxH - h, COLOR_BG_DARK);
    }
    if (h > 0) {
      uint16_t col = (h > 30) ? TFT_RED : ((h > 16) ? TFT_YELLOW : COLOR_ACCENT_GREEN);
      tft.fillRect(bx, by, barW, h, col);
    }
  }
}

void player_ui_enter() {
  renderStaticElements();
  updateProgressBar();
}

void player_ui_update() {
  updateVisualizer();

  if (millis() - s_lastProgressTime >= 1000) {
    s_lastProgressTime = millis();
    updateProgressBar();
  }
}

void player_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 扩大返回按键判定区 (左上角 0~95, 0~55)
  if (x >= 0 && x <= 95 && y >= 0 && y <= 55) {
    UIPage prev = ui_get_previous_page();
    if (prev == UI_PAGE_SPLASH || prev == UI_PAGE_PLAYER) prev = UI_PAGE_HOME;
    ui_switch_to(prev);
    return;
  }

  // 2. 进度条点击 Seeking (y: 130~150, x: 50~270)
  if (y >= 130 && y <= 150 && x >= 50 && x <= 270) {
    uint16_t total = player_get_total_seconds();
    if (total > 0) {
      int clickOffset = constrain(x - 58, 0, 202);
      uint16_t seekTarget = (uint16_t)((long)clickOffset * total / 202);
      player_seek(seekTarget);
      updateProgressBar();
    }
    return;
  }

  // 3. 第一排控制按键 (y: 150~202)
  if (y >= 150 && y <= 202) {
    if (x >= 8 && x <= 70) {
      // 上一首
      player_prev();
      renderStaticElements();
      updateProgressBar();
    } else if (x >= 74 && x <= 160) {
      // 播放/暂停
      player_toggle();
      renderStaticElements();
    } else if (x >= 162 && x <= 226) {
      // 下一首
      player_next();
      renderStaticElements();
      updateProgressBar();
    } else if (x >= 228 && x <= 312) {
      // 切换循环模式
      player_cycle_mode();
      renderStaticElements();
    }
    return;
  }

  // 4. 第二排控制按键 (y: 204~238)
  if (y >= 204 && y <= 238) {
    if (x >= 8 && x <= 80) {
      // 快退 -5s
      player_seek_offset(-5);
      updateProgressBar();
    } else if (x >= 84 && x <= 224) {
      // 循环调节音量: 30% -> 50% -> 70% -> 90% -> 100% -> 0%
      uint8_t curVol = audio_get_volume();
      uint8_t nextVol = (curVol >= 100) ? 30 : (curVol + 20);
      audio_set_volume(nextVol);
      renderStaticElements();
    } else if (x >= 226 && x <= 312) {
      // 快进 +5s
      player_seek_offset(+5);
      updateProgressBar();
    }
    return;
  }
}
