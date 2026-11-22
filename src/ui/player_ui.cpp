#include "hardware/rgb.h"
#include "player_ui.h"
#include "ui.h"
#include "ui_theme.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "player/player.h"
#include "system/settings_manager.h"

#define SPEC_BARS 16
static int s_specH[SPEC_BARS] = {0};
static int s_peakH[SPEC_BARS] = {0};
static unsigned long s_lastPeakDecay[SPEC_BARS] = {0};
static unsigned long s_lastSpecTime = 0;
static unsigned long s_lastProgressTime = 0;
static const TrackInfo* s_lastUiTrack = nullptr;
static PlayerState s_lastUiState = STATE_STOPPED;

static void renderStaticElements() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("正在播放", true);

  // 1. M3 柔和粉玫瑰渐变曲目信息卡片 (y: 34 ~ 80, h = 46, r = 12)
  int16_t cardX = 10, cardY = 34, cardW = 300, cardH = 46;
  ui_theme_draw_gradient_card(cardX, cardY, cardW, cardH, 12, M3_HERO_START, M3_HERO_END);

  const TrackInfo* t = player_get_current_track();
  bool playing = (player_get_state() == STATE_PLAYING);

  // 左侧深酒红圆角音符图标
  ui_theme_draw_icon_badge(cardX + 22, cardY + cardH / 2, 26, M3_ON_PRIMARY, M3_HERO_START, M3_ICON_NOTE, true);

  // 高对比度深色粗体标题 + 副标题
  const char* songTitle = t && strlen(t->title) > 0 ? t->title : "未选择曲目";
  font_draw_string_bold(cardX + 42, cardY + 6, cardW - 52, songTitle, M3_ON_PRIMARY, M3_ON_PRIMARY, 1);

  char sub[64];
  snprintf(sub, sizeof(sub), "I2S-DMA DAC2 · %s",
           (t && strlen(t->artist) > 0) ? t->artist : "本地音频");
  font_draw_string_clipped(cardX + 42, cardY + 25, cardW - 52, sub, M3_PRIMARY_CONTAINER, M3_PRIMARY_CONTAINER, 1);
  s_lastUiTrack = t;
  s_lastUiState = player_get_state();

  // 2. 第一排 M3 胶囊主控按键 (y: 152 ~ 200)
  // 上一曲 |< (x: 10, w: 56)
  ui_theme_draw_badge(10, 154, 56, 44, "|<", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);

  // Hero 播放 / 暂停 (M3 暖粉大胶囊: x: 74, w: 84, r: 22)
  ui_theme_draw_badge(74, 152, 84, 48, playing ? "暂停" : "播放", M3_ON_PRIMARY, M3_PRIMARY);

  // 下一曲 >| (x: 166, w: 56)
  ui_theme_draw_badge(166, 154, 56, 44, ">|", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);

  // 循环模式切换 (x: 230, w: 80)
  const char* mStr = "全部循环";
  if (player_get_mode() == MODE_REPEAT_ONE) mStr = "单曲循环";
  else if (player_get_mode() == MODE_SHUFFLE) mStr = "随机播放";
  else if (player_get_mode() == MODE_SEQUENTIAL) mStr = "顺序播放";
  ui_theme_draw_badge(230, 154, 80, 44, mStr, M3_PRIMARY, M3_PRIMARY_CONTAINER);

  // 3. 第二排辅助控制胶囊 (y: 204 ~ 236, h = 32)
  ui_theme_draw_badge(10, 204, 66, 32, "快退 -5s", UI_COL_TEXT_MUTED, UI_COL_SURFACE);

  char volBuf[20];
  snprintf(volBuf, sizeof(volBuf), "音量:%d%%", audio_get_volume());
  ui_theme_draw_badge(82, 204, 74, 32, volBuf, M3_TERTIARY, M3_TERTIARY_CONTAINER);

  char eqBuf[20];
  snprintf(eqBuf, sizeof(eqBuf), "%s", audio_get_eq_name(audio_get_eq()));
  ui_theme_draw_badge(162, 204, 62, 32, eqBuf, M3_SAGE, M3_SAGE_CONTAINER);

  ui_theme_draw_badge(230, 204, 80, 32, "快进 +5s", UI_COL_TEXT_MUTED, UI_COL_SURFACE);

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

  // M3 粗进度滑块主体 (x: 58 ~ 260, 宽 202)
  ui_theme_draw_slider(58, 130, 202, (int16_t)elapsed, (int16_t)total, M3_PRIMARY);
}

// 复古 Hi-Fi LED 分段频谱分析仪 (带 Peak-Hold 峰值保持衰减)
static void updateVisualizer() {
  if (millis() - s_lastSpecTime < 40) return; // ~25 FPS 顺滑刷新
  s_lastSpecTime = millis();

  TFT_eSPI& tft = display_get_tft();
  int startX = 14, startY = 126, maxH = 40, barW = 14, gap = 4;
  uint16_t freq = player_get_current_note_freq();
  uint8_t level = audio_stream_get_level();
  bool playing = (player_get_state() == STATE_PLAYING);

  for (int i = 0; i < SPEC_BARS; i++) {
    int targetH = 0;
    if (playing) {
      if (freq > 0) {
        int center = (freq / 65) % SPEC_BARS;
        int dist = abs(i - center);
        int pulse = random(max(4, maxH - dist * 6), maxH);
        targetH = constrain(pulse, 2, maxH);
      } else {
        int baseH = 4 + (level * (maxH - 6)) / 100;
        int bandScale = (i < 4) ? 105 : ((i < 12) ? 95 : 80);
        int jitter = random(-15, 25);
        targetH = constrain((baseH * (bandScale + jitter)) / 100, 2, maxH);
      }
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

    // 绘制 M3 暖玫瑰频谱分段块 (每块高 3px, 间隔 1px)
    int segH = 3, segGap = 1;
    int numSegs = h / (segH + segGap);
    for (int s = 0; s < numSegs; s++) {
      int segY = startY - (s + 1) * (segH + segGap);
      uint16_t segCol;
      if (s < 4) segCol = M3_PRIMARY_CONTAINER;
      else if (s < 7) segCol = M3_HERO_END;
      else segCol = M3_HERO_START;
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
    uint16_t beatFreq = (freq > 0) ? freq : (uint16_t)(300 + level * 12);
    rgb_beat_pulse(energy, beatFreq);
  }
}

void player_ui_enter() {
  renderStaticElements();
  updateProgressBar();
}

void player_ui_update() {
  if (player_get_current_track() != s_lastUiTrack || player_get_state() != s_lastUiState) {
    renderStaticElements();
    updateProgressBar();
  }
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
    if (x >= 8 && x <= 78) {
      // 快退 -5s
      player_seek_offset(-5);
      updateProgressBar();
    } else if (x >= 80 && x <= 158) {
      // 循环调节音量: 30% -> 50% -> 70% -> 90% -> 100% -> 30%
      uint8_t curVol = audio_get_volume();
      uint8_t nextVol = (curVol >= 100) ? 30 : (curVol + 20);
      if (nextVol > 100) nextVol = 100;
      audio_set_volume(nextVol);
      settings_get().volume = nextVol;
      settings_save();
      renderStaticElements();
    } else if (x >= 160 && x <= 224) {
      // 切换音效 EQ (原声 / 低音 / 人声)
      AudioEQMode eq = audio_cycle_eq();
      settings_get().eqMode = (uint8_t)eq;
      settings_save();
      renderStaticElements();
    } else if (x >= 226 && x <= 312) {
      // 快进 +5s
      player_seek_offset(+5);
      updateProgressBar();
    }
    return;
  }
}
