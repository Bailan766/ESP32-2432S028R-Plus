#include "hardware/rgb.h"
#include "player_ui.h"
#include "ui.h"
#include "ui_theme.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "player/player.h"

#define SPEC_BARS 16
static int s_specH[SPEC_BARS] = {0};
static int s_peakH[SPEC_BARS] = {0};
static unsigned long s_lastPeakDecay[SPEC_BARS] = {0};
static unsigned long s_lastSpecTime = 0;
static unsigned long s_lastProgressTime = 0;

static void renderStaticElements() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("正在播放", true);

  // 1. 曲目信息卡片 (y: 34 ~ 80, h = 46)
  int16_t cardX = 10, cardY = 34, cardW = 300, cardH = 46;
  tft.fillRoundRect(cardX, cardY, cardW, cardH, 6, UI_COL_SURFACE);
  tft.drawRoundRect(cardX, cardY, cardW, cardH, 6, UI_COL_BORDER_SUBTLE);

  const TrackInfo* t = player_get_current_track();
  bool playing = (player_get_state() == STATE_PLAYING);

  // 左侧指示条
  tft.fillRect(cardX + 2, cardY + 8, 3, cardH - 16, playing ? UI_COL_AMBER : UI_COL_BORDER_LIGHT);

  // 曲目标题
  const char* songTitle = t && strlen(t->title) > 0 ? t->title : "未选择曲目";
  font_draw_string(cardX + 16, cardY + 6, songTitle, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

  // 格式与艺术家
  char sub[64];
  snprintf(sub, sizeof(sub), "MP3 44.1kHz DAC2 | %s",
           (t && strlen(t->artist) > 0) ? t->artist : "本地音频");
  font_draw_string(cardX + 16, cardY + 26, sub, UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);

  // 2. 第一排主控按键 (y: 154 ~ 198, h = 44)
  // 上一曲 |< (x: 10, w: 56)
  tft.fillRoundRect(10, 154, 56, 44, 6, UI_COL_SURFACE_HIGH);
  tft.drawRoundRect(10, 154, 56, 44, 6, UI_COL_BORDER_LIGHT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(UI_COL_TEXT_PRIMARY);
  tft.drawString("|<", 38, 176, 4);

  // 播放 / 暂停 (Hero 按键: x: 74, w: 84)
  tft.fillRoundRect(74, 152, 84, 48, 6, UI_COL_AMBER);
  tft.drawRoundRect(74, 152, 84, 48, 6, UI_COL_TEXT_PRIMARY);
  font_draw_string_center(116, 168, playing ? "暂停" : "播放", TFT_BLACK, UI_COL_AMBER, 1);

  // 下一曲 >| (x: 166, w: 56)
  tft.fillRoundRect(166, 154, 56, 44, 6, UI_COL_SURFACE_HIGH);
  tft.drawRoundRect(166, 154, 56, 44, 6, UI_COL_BORDER_LIGHT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(UI_COL_TEXT_PRIMARY);
  tft.drawString(">|", 194, 176, 4);

  // 循环模式切换 (x: 230, w: 80)
  tft.fillRoundRect(230, 154, 80, 44, 6, UI_COL_SURFACE);
  tft.drawRoundRect(230, 154, 80, 44, 6, UI_COL_BORDER_LIGHT);
  const char* mStr = "全部循环";
  if (player_get_mode() == MODE_REPEAT_ONE) mStr = "单曲循环";
  else if (player_get_mode() == MODE_SHUFFLE) mStr = "随机播放";
  else if (player_get_mode() == MODE_SEQUENTIAL) mStr = "顺序播放";
  font_draw_string_center(270, 168, mStr, UI_COL_AMBER, UI_COL_SURFACE, 1);

  // 3. 第二排辅助控制按键 (y: 204 ~ 236, h = 32)
  // -5s 快退 (x: 10, w: 68)
  ui_theme_draw_pill_btn(10, 204, 68, 32, "快退 -5s", UI_COL_TEXT_MUTED, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);

  // VOL 音量 (x: 86, w: 136)
  char volBuf[32];
  snprintf(volBuf, sizeof(volBuf), "音量: %d%%", audio_get_volume());
  ui_theme_draw_pill_btn(86, 204, 136, 32, volBuf, UI_COL_CYAN, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);

  // +5s 快进 (x: 230, w: 80)
  ui_theme_draw_pill_btn(230, 204, 80, 32, "快进 +5s", UI_COL_TEXT_MUTED, UI_COL_SURFACE_HIGH, UI_COL_BORDER_SUBTLE);

  tft.setTextDatum(TL_DATUM);
}

static void updateProgressBar() {
  TFT_eSPI& tft = display_get_tft();

  uint16_t elapsed = player_get_elapsed_seconds();
  uint16_t total = player_get_total_seconds();
  if (total == 0) total = 180;

  // 时间显示 (y: 132 ~ 146)
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", elapsed / 60, elapsed % 60);
  tft.fillRect(10, 132, 45, 14, UI_COL_BG);
  tft.setTextColor(UI_COL_TEXT_MUTED, UI_COL_BG);
  tft.drawString(timeBuf, 10, 132, 2);

  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", total / 60, total % 60);
  tft.fillRect(265, 132, 45, 14, UI_COL_BG);
  tft.setTextColor(UI_COL_TEXT_MUTED, UI_COL_BG);
  tft.drawString(timeBuf, 270, 132, 2);

  // 进度条主体 (x: 58 ~ 260, 宽 202)
  int barX = 58, barY = 138, barW = 202, barH = 4;
  int prog = (int)((long)elapsed * barW / total);
  prog = constrain(prog, 0, barW);

  // 凹槽底轨
  tft.fillRoundRect(barX, barY, barW, barH, 2, UI_COL_BORDER_SUBTLE);
  // 已播放进度高亮
  if (prog > 0) {
    tft.fillRoundRect(barX, barY, prog, barH, 2, UI_COL_AMBER);
  }
  // 滑动圆点指示
  tft.fillCircle(barX + prog, barY + 2, 4, UI_COL_TEXT_PRIMARY);
  tft.drawCircle(barX + prog, barY + 2, 5, UI_COL_AMBER);
}

// 复古 Hi-Fi LED 分段频谱分析仪 (带 Peak-Hold 峰值保持衰减)
static void updateVisualizer() {
  if (millis() - s_lastSpecTime < 40) return; // ~25 FPS 顺滑刷新
  s_lastSpecTime = millis();

  TFT_eSPI& tft = display_get_tft();
  int startX = 14, startY = 126, maxH = 40, barW = 14, gap = 4;
  uint16_t freq = player_get_current_note_freq();
  bool playing = (player_get_state() == STATE_PLAYING);

  for (int i = 0; i < SPEC_BARS; i++) {
    int targetH = 0;
    if (playing && freq > 0) {
      int center = (freq / 65) % SPEC_BARS;
      int dist = abs(i - center);
      int pulse = random(max(4, maxH - dist * 6), maxH);
      targetH = constrain(pulse, 2, maxH);
    }

    if (s_specH[i] < targetH) {
      s_specH[i] = targetH;
    } else {
      s_specH[i] = max(0, s_specH[i] - 3);
    }

    // 峰值保持与缓慢衰减算法
    if (s_specH[i] >= s_peakH[i]) {
      s_peakH[i] = s_specH[i];
      s_lastPeakDecay[i] = millis();
    } else if (millis() - s_lastPeakDecay[i] > 200) {
      if (s_peakH[i] > 0) s_peakH[i]--;
    }

    int h = s_specH[i];
    int bx = startX + i * (barW + gap);

    // 清除顶部未点亮区域
    if (maxH - h > 0) {
      tft.fillRect(bx, startY - maxH, barW, maxH - h, UI_COL_BG);
    }

    // 绘制 LED 分段块 (每块高 3px, 间隔 1px)
    int segH = 3, segGap = 1;
    int numSegs = h / (segH + segGap);
    for (int s = 0; s < numSegs; s++) {
      int segY = startY - (s + 1) * (segH + segGap);
      uint16_t segCol;
      if (s < 5) segCol = UI_COL_GREEN;
      else if (s < 8) segCol = UI_COL_AMBER;
      else segCol = UI_COL_RED;
      tft.fillRect(bx, segY, barW, segH, segCol);
    }

    // 绘制 Peak-Hold 悬浮单段微高亮
    if (s_peakH[i] > 2) {
      int peakY = startY - s_peakH[i];
      tft.drawFastHLine(bx, peakY, barW, UI_COL_TEXT_PRIMARY);
    }
  }

  if (playing) {
    int totalH = 0;
    for (int i = 0; i < SPEC_BARS; i++) totalH += s_specH[i];
    uint8_t energy = constrain(map(totalH, 0, SPEC_BARS * 28, 20, 255), 0, 255);
    rgb_beat_pulse(energy, freq);
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

  // 2. 进度条点击 Seeking (y: 125~150, x: 50~270)
  if (y >= 125 && y <= 150 && x >= 50 && x <= 270) {
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
