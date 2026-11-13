#include "hardware/rgb.h"
#include "bt_speaker_ui.h"
#include "bluetooth/bt_sink.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "ui/font_cn.h"
#include "ui/ui.h"
#include "config.h"

static BTSinkState s_lastState = (BTSinkState)-1;
static char s_lastTitle[64] = "";
static char s_lastArtist[64] = "";
static unsigned long s_lastAnimUpdate = 0;
static uint8_t s_barHeights[16] = {0};

static void renderStaticUI() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("蓝牙音箱模式", true);

  // 1. 中央大信息卡片 (x: 14, y: 36, w: 292, h: 130)
  tft.fillRoundRect(14, 36, 292, 130, 8, COLOR_PANEL_BG);
  tft.drawRoundRect(14, 36, 292, 130, 8, COLOR_ACCENT_CYAN);

  // 2. 底部控制按钮 (x: 14..306, y: 176..224)
  // [上一曲] (x: 14, y: 176, w: 64, h: 46)
  tft.fillRoundRect(14, 176, 64, 46, 6, 0x10A2);
  tft.drawRoundRect(14, 176, 64, 46, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(46, 189, "|<<", COLOR_TEXT_PRIMARY, 0x10A2, 1);

  // [播放/暂停] (x: 86, y: 176, w: 96, h: 46)
  tft.fillRoundRect(86, 176, 96, 46, 6, 0x05E0);
  tft.drawRoundRect(86, 176, 96, 46, 6, COLOR_ACCENT_GREEN);
  font_draw_string_center(134, 189, "播放/暂停", TFT_WHITE, 0x05E0, 1);

  // [下一曲] (x: 190, y: 176, w: 64, h: 46)
  tft.fillRoundRect(190, 176, 64, 46, 6, 0x10A2);
  tft.drawRoundRect(190, 176, 64, 46, 6, COLOR_PANEL_BORDER);
  font_draw_string_center(222, 189, ">>|", COLOR_TEXT_PRIMARY, 0x10A2, 1);

  // [重置] (x: 262, y: 176, w: 44, h: 46)
  tft.fillRoundRect(262, 176, 44, 46, 6, 0x2104);
  tft.drawRoundRect(262, 176, 44, 46, 6, COLOR_ACCENT_ORANGE);
  font_draw_string_center(284, 189, "重连", COLOR_ACCENT_ORANGE, 0x2104, 1);

  s_lastState = (BTSinkState)-1;
  s_lastTitle[0] = '\0';
  s_lastArtist[0] = '\0';
}

static void renderDynamicInfo() {
  TFT_eSPI& tft = display_get_tft();
  BTSinkState st = bt_sink_get_state();
  const char* curTitle = bt_sink_get_title();
  const char* curArtist = bt_sink_get_artist();

  // 1. 状态指示更新
  if (st != s_lastState) {
    s_lastState = st;
    tft.fillRect(24, 44, 270, 20, COLOR_PANEL_BG);

    uint16_t badgeColor = COLOR_ACCENT_ORANGE;
    const char* statusStr = "等待手机蓝牙连接...";
    if (st == BT_STATE_CONNECTING) {
      badgeColor = TFT_YELLOW;
      statusStr = "正在与手机握手配对...";
    } else if (st == BT_STATE_CONNECTED) {
      badgeColor = COLOR_ACCENT_CYAN;
      statusStr = "手机已连接 (就绪)";
    } else if (st == BT_STATE_PLAYING) {
      badgeColor = COLOR_ACCENT_GREEN;
      statusStr = "正在接收播放音频流";
    }

    tft.fillCircle(30, 52, 4, badgeColor);
    font_draw_string(40, 46, statusStr, badgeColor, COLOR_PANEL_BG, 1);
  }

  // 2. 设备名与歌曲元数据更新
  if (strcmp(curTitle, s_lastTitle) != 0 || strcmp(curArtist, s_lastArtist) != 0) {
    strncpy(s_lastTitle, curTitle, sizeof(s_lastTitle) - 1);
    strncpy(s_lastArtist, curArtist, sizeof(s_lastArtist) - 1);

    tft.fillRect(24, 68, 270, 44, COLOR_PANEL_BG);

    // 歌曲名
    font_draw_string_clipped(24, 68, 270, curTitle, COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);

    // 歌手与设备广播名
    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "%s | 蓝牙名: %s", curArtist, bt_sink_get_device_name());
    font_draw_string_clipped(24, 92, 270, infoBuf, COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);
  }
}

static void renderSpectrumBars() {
  TFT_eSPI& tft = display_get_tft();
  bool isStreaming = bt_sink_has_audio_stream();
  uint8_t audioLevel = isStreaming ? audio_stream_get_level() : 0;

  const int NUM_BARS = 16;
  const int BAR_W = 12;
  const int BAR_GAP = 5;
  const int START_X = 22;
  const int BASE_Y = 158;
  const int MAX_H = 34;

  for (int i = 0; i < NUM_BARS; i++) {
    uint8_t targetH = 3;
    if (isStreaming) {
      // 结合真实 PCM 包络能量与频段权重生成动态频谱柱
      int baseH = 4 + (audioLevel * (MAX_H - 6)) / 100;
      int jitter = (rand() % 41) - 15; // -15% ~ +25% 频段起伏
      int bandScale = (i < 4) ? 105 : ((i < 12) ? 95 : 80);
      int hCalc = (baseH * (bandScale + jitter)) / 100;
      targetH = (uint8_t)constrain(hCalc, 3, MAX_H);
    }

    // 平滑滤波
    s_barHeights[i] = (s_barHeights[i] * 3 + targetH) / 4;
    int h = s_barHeights[i];
    if (h < 2) h = 2;
    if (h > MAX_H) h = MAX_H;

    int bx = START_X + i * (BAR_W + BAR_GAP);
    int by = BASE_Y - h;

    // 清除上一次绘制的高处残影
    tft.fillRect(bx, BASE_Y - MAX_H, BAR_W, MAX_H - h, COLOR_PANEL_BG);

    // 渐变色彩柱
    uint16_t barCol = COLOR_ACCENT_CYAN;
    if (h > 26) barCol = 0xF800; // 红色
    else if (h > 18) barCol = 0xFD20; // 橙黄
    else if (h > 10) barCol = 0x07E0; // 翠绿

    tft.fillRect(bx, by, BAR_W, h, barCol);
  }

  if (bt_sink_get_state() == BT_STATE_PLAYING) {
    uint8_t energy = constrain(map(audioLevel, 0, 100, 20, 255), 0, 255);
    rgb_beat_pulse(energy, 300 + audioLevel * 12);
  }
}

void bt_speaker_ui_enter() {
  bt_sink_start();
  renderStaticUI();
  renderDynamicInfo();
}

void bt_speaker_ui_update() {
  renderDynamicInfo();

  if (millis() - s_lastAnimUpdate >= 60) {
    s_lastAnimUpdate = millis();
    renderSpectrumBars();
  }
}

void bt_speaker_ui_handle_touch(int16_t x, int16_t y) {
  // 1. 顶部返回按钮 (x: 0..76, y: 0..32)
  if (x <= 76 && y <= 32) {
    bt_speaker_ui_leave();
    audio_test_beep(1000, 30);
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  // 2. 底部控制按钮 (y: 170..230)
  if (y >= 170 && y <= 230) {
    if (x >= 14 && x <= 78) {
      // [上一曲]
      audio_test_beep(1500, 30);
      bt_sink_prev();
    } else if (x >= 86 && x <= 182) {
      // [播放/暂停]
      audio_test_beep(1800, 30);
      bt_sink_play_pause();
    } else if (x >= 190 && x <= 254) {
      // [下一曲]
      audio_test_beep(1500, 30);
      bt_sink_next();
    } else if (x >= 262 && x <= 306) {
      // [重置/重连]
      audio_test_beep(800, 50);
      bt_sink_stop();
      delay(80);
      bt_sink_start();
      renderStaticUI();
      renderDynamicInfo();
    }
  }
}

void bt_speaker_ui_leave() {
  bt_sink_stop();
}
