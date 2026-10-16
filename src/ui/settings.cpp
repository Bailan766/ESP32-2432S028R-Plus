#include "settings.h"
#include "ui.h"
#include "font_cn.h"
#include "hardware/display.h"
#include "hardware/audio.h"
#include "hardware/touch.h"
#include "storage/sdcard.h"
#include "storage/scanner.h"
#include "system/settings_manager.h"
#include "player/player.h"

static bool s_showAboutDialog = false;

static void renderAboutModal() {
  TFT_eSPI& tft = display_get_tft();

  // 半透明深色遮罩与对话框卡片
  tft.fillRoundRect(16, 28, 288, 196, 8, 0x08A3);
  tft.drawRoundRect(16, 28, 288, 196, 8, COLOR_ACCENT_CYAN);

  font_draw_string_center(160, 36, "关于本机 (CYD Plus)", COLOR_ACCENT_CYAN, 0x08A3, 1);
  tft.drawLine(24, 54, 296, 54, COLOR_PANEL_BORDER);

  font_draw_string(26, 60, "开发者: " PROJECT_AUTHOR, COLOR_ACCENT_ORANGE, 0x08A3, 1);

  char verBuf[48];
  snprintf(verBuf, sizeof(verBuf), "固件: 摆烂播放器 %s %s", PROJECT_VERSION, PROJECT_EDITION);
  font_draw_string(26, 82, verBuf, COLOR_ACCENT_GREEN, 0x08A3, 1);

  font_draw_string(26, 104, "硬件: ESP32-D0WD-V3 (双核 240MHz)", COLOR_TEXT_PRIMARY, 0x08A3, 1);
  font_draw_string(26, 126, "屏幕: ST7789 2.8寸 (320x240)", COLOR_TEXT_PRIMARY, 0x08A3, 1);
  font_draw_string(26, 148, "触控: XPT2046 软件SPI仿射校准", COLOR_TEXT_PRIMARY, 0x08A3, 1);

  char heapBuf[48];
  snprintf(heapBuf, sizeof(heapBuf), "空闲内存: %u KB (运行正常)", ESP.getFreeHeap() / 1024);
  font_draw_string(26, 170, heapBuf, COLOR_TEXT_MUTED, 0x08A3, 1);

  tft.fillRoundRect(70, 192, 180, 26, 4, 0x10A2);
  tft.drawRoundRect(70, 192, 180, 26, 4, COLOR_ACCENT_CYAN);
  font_draw_string_center(160, 197, "[点击任意位置返回设置]", COLOR_ACCENT_CYAN, 0x10A2, 1);
}


static void render_slider(int y, int val, const char* valStr, uint16_t color) {
  TFT_eSPI& tft = display_get_tft();
  int cx = 70 + (val * 190 / 100);
  
  // Clear track and handle area
  tft.fillRect(64, y + 8, 202, 16, COLOR_PANEL_BG);
  
  // Track
  tft.fillRoundRect(70, y + 13, 190, 6, 3, 0x10A2);
  // Fill
  if (val > 0) {
    tft.fillRoundRect(70, y + 13, val * 190 / 100, 6, 3, color);
  }
  // Handle
  tft.fillRoundRect(cx - 6, y + 8, 12, 16, 4, TFT_WHITE);
  
  // Clear and draw text
  tft.fillRect(265, y + 8, 45, 16, COLOR_PANEL_BG);
  font_draw_string(270, y + 8, valStr, color, COLOR_PANEL_BG, 1);
}

static void renderSettings() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  ui_draw_header("系统设置", true);

  SystemSettings& cfg = settings_get();

  // 1. 音量 (y: 33 ~ 65)
  tft.fillRoundRect(10, 33, 300, 32, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(10, 33, 300, 32, 5, COLOR_PANEL_BORDER);
  font_draw_string(20, 41, "音量", COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);
  char volBuf[16];
  snprintf(volBuf, sizeof(volBuf), "%d%%", cfg.volume);
  render_slider(33, cfg.volume, volBuf, COLOR_ACCENT_CYAN);
  // 2. 屏幕亮度 (y: 69 ~ 101)
  tft.fillRoundRect(10, 69, 300, 32, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(10, 69, 300, 32, 5, COLOR_PANEL_BORDER);
  font_draw_string(20, 77, "亮度", COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);
  char briBuf[16];
  snprintf(briBuf, sizeof(briBuf), "%d%%", cfg.brightness);
  render_slider(69, cfg.brightness, briBuf, COLOR_ACCENT_GREEN);
  // 3. 自动息屏时间 (y: 105 ~ 137)
  tft.fillRoundRect(10, 105, 300, 32, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(10, 105, 300, 32, 5, COLOR_PANEL_BORDER);
  font_draw_string(20, 113, "自动息屏", COLOR_TEXT_PRIMARY, COLOR_PANEL_BG, 1);

  const char* sleepStr = "关闭";
  if (cfg.autoSleepSeconds == 30) sleepStr = "30秒";
  else if (cfg.autoSleepSeconds == 60) sleepStr = "1分钟";
  else if (cfg.autoSleepSeconds == 180) sleepStr = "3分钟";
  else if (cfg.autoSleepSeconds == 300) sleepStr = "5分钟";

    tft.fillRoundRect(85, 108, 55, 26, 4, 0x10A2);
  tft.drawRoundRect(85, 108, 55, 26, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(112, 113, "锁屏", TFT_WHITE, 0x10A2, 1);

  tft.fillRoundRect(145, 108, 55, 26, 4, 0x10A2);
  tft.drawRoundRect(145, 108, 55, 26, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(172, 113, "息屏", TFT_WHITE, 0x10A2, 1);

  tft.fillRoundRect(210, 108, 94, 26, 4, 0x10A2);
  tft.drawRoundRect(210, 108, 94, 26, 4, COLOR_PANEL_BORDER);
  font_draw_string_center(257, 113, sleepStr, COLOR_ACCENT_ORANGE, 0x10A2, 1);

  // 4. 第一排功能按钮 (y: 141 ~ 177, h: 36)
  // 触屏校准 (Cyan)
  tft.fillRoundRect(8, 141, 96, 36, 5, 0x10A2);
  tft.drawRoundRect(8, 141, 96, 36, 5, COLOR_ACCENT_CYAN);
  font_draw_string_center(56, 151, "触屏校准", COLOR_ACCENT_CYAN, 0x10A2, 1);

  // 重扫TF卡 (Orange)
  tft.fillRoundRect(110, 141, 96, 36, 5, 0x10A2);
  tft.drawRoundRect(110, 141, 96, 36, 5, COLOR_ACCENT_ORANGE);
  font_draw_string_center(158, 151, "重扫TF卡", COLOR_ACCENT_ORANGE, 0x10A2, 1);

  // 开机校准开关 (Green if ON, Muted if OFF)
  uint16_t calCol = cfg.bootCalibrate ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED;
  uint16_t calBg = cfg.bootCalibrate ? 0x0A42 : COLOR_PANEL_BG;
  tft.fillRoundRect(212, 141, 100, 36, 5, calBg);
  tft.drawRoundRect(212, 141, 100, 36, 5, cfg.bootCalibrate ? COLOR_ACCENT_GREEN : COLOR_PANEL_BORDER);
  font_draw_string_center(262, 151, cfg.bootCalibrate ? "开机校准:开" : "开机校准:关", calCol, calBg, 1);

  // 5. 第二排功能按钮 (y: 181 ~ 217, h: 36, 5 buttons)
  // 硬件自检
  tft.fillRoundRect(5, 181, 58, 36, 5, 0x03E0);
  tft.drawRoundRect(5, 181, 58, 36, 5, COLOR_TEXT_PRIMARY);
  font_draw_string_center(34, 191, "自检", TFT_WHITE, 0x03E0, 1);

  // 屏幕旋转
  tft.fillRoundRect(68, 181, 58, 36, 5, 0x10A2);
  tft.drawRoundRect(68, 181, 58, 36, 5, 0x92BB);
  font_draw_string_center(97, 191, "旋转", COLOR_TEXT_PRIMARY, 0x10A2, 1);

  // 断点续播
  tft.fillRoundRect(131, 181, 58, 36, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(131, 181, 58, 36, 5, COLOR_PANEL_BORDER);
  font_draw_string_center(160, 191, cfg.resumeEnable ? "续:开" : "续:关", cfg.resumeEnable ? COLOR_ACCENT_GREEN : COLOR_TEXT_MUTED, COLOR_PANEL_BG, 1);

  // 关于
  tft.fillRoundRect(194, 181, 58, 36, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(194, 181, 58, 36, 5, COLOR_ACCENT_CYAN);
  font_draw_string_center(223, 191, "关于", COLOR_ACCENT_CYAN, COLOR_PANEL_BG, 1);

  // 重启
  tft.fillRoundRect(257, 181, 58, 36, 5, TFT_RED);
  tft.drawRoundRect(257, 181, 58, 36, 5, COLOR_TEXT_PRIMARY);
  font_draw_string_center(286, 191, "重启", TFT_WHITE, TFT_RED, 1);

  // 6. 底部关于信息
  if (s_showAboutDialog) {
    renderAboutModal();
  } else {
    font_draw_string_center(160, 224, "摆烂播放器 " PROJECT_VERSION " " PROJECT_EDITION " | CYD Plus", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  }
}

void settings_ui_enter() {
  s_showAboutDialog = false;
  renderSettings();
}

void settings_ui_update() {
}

void settings_ui_handle_touch(int16_t x, int16_t y) {
  // 如果当前弹出了关于本机对话框，点击任意处关闭对话框
  if (s_showAboutDialog) {
    s_showAboutDialog = false;
    renderSettings();
    return;
  }

  // 1. 扩大返回按钮响应区域 (左上角 0~95, 0~32)
  if (x >= 0 && x <= 95 && y >= 0 && y <= 32) {
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  SystemSettings& cfg = settings_get();

  // 2. 音量 滑动 (y: 33 ~ 65)
  if (y >= 33 && y <= 65 && x >= 60 && x <= 270) {
    int16_t curX, curY;
    while (touch_read(curX, curY)) {
      int newVol = (curX - 70) * 100 / 190;
      if (newVol < 0) newVol = 0;
      if (newVol > 100) newVol = 100;
      if (newVol != cfg.volume) {
        cfg.volume = newVol;
        audio_set_volume(cfg.volume);
        char volBuf[16];
        snprintf(volBuf, sizeof(volBuf), "%d%%", cfg.volume);
        render_slider(33, cfg.volume, volBuf, COLOR_ACCENT_CYAN);
      }
      delay(20);
    }
    settings_save();
    return;
  }

  // 3. 亮度 - / + (y: 69 ~ 101)
  // 3. 屏幕亮度 滑动 (y: 69 ~ 101)
  if (y >= 69 && y <= 101 && x >= 60 && x <= 270) {
    int16_t curX, curY;
    while (touch_read(curX, curY)) {
      int newBri = (curX - 70) * 100 / 190;
      if (newBri < 5) newBri = 5;
      if (newBri > 100) newBri = 100;
      if (newBri != cfg.brightness) {
        cfg.brightness = newBri;
        display_set_brightness(cfg.brightness);
        char briBuf[16];
        snprintf(briBuf, sizeof(briBuf), "%d%%", cfg.brightness);
        render_slider(69, cfg.brightness, briBuf, COLOR_ACCENT_GREEN);
      }
      delay(20);
    }
    settings_save();
    return;
  }
  // 4. 自动息屏切换 (y: 105 ~ 137, x: 205 ~ 310)
  if (y >= 105 && y <= 137) {
    if (x >= 85 && x <= 140) {
      ui_switch_to(UI_PAGE_LOCKSCREEN);
      return;
    } else if (x >= 145 && x <= 200) {
      display_sleep();
      return;
    } else if (x >= 205 && x <= 310) {
    if (cfg.autoSleepSeconds == 0) cfg.autoSleepSeconds = 30;
    else if (cfg.autoSleepSeconds == 30) cfg.autoSleepSeconds = 60;
    else if (cfg.autoSleepSeconds == 60) cfg.autoSleepSeconds = 180;
    else if (cfg.autoSleepSeconds == 180) cfg.autoSleepSeconds = 300;
    else cfg.autoSleepSeconds = 0;
    settings_save();
    renderSettings();
    return;
  }
  }

  // 5. 第一排功能按钮 (y: 140 ~ 177)
  if (y >= 140 && y <= 177) {
    // 触屏校准 (x: 0 ~ 106)
    if (x >= 0 && x <= 106) {
      ui_switch_to(UI_PAGE_TOUCH_CAL);
      return;
    }
    // 重扫TF卡 (x: 107 ~ 208)
    if (x >= 107 && x <= 208) {
      audio_test_beep(2000, 100);
      sdcard_reinit();
      scanner_scan_all();
      renderSettings();
      return;
    }
    // 开机校准开关 (x: 209 ~ 320)
    if (x >= 209 && x <= 320) {
      cfg.bootCalibrate = !cfg.bootCalibrate;
      settings_save();
      audio_test_beep(1400, 60);
      renderSettings();
      return;
    }
  }

  // 6. 第二排功能按钮 (y: 180 ~ 218)
  if (y >= 180 && y <= 218) {
    if (x >= 0 && x <= 63) {
      ui_switch_to(UI_PAGE_HARDWARE_TEST);
      return;
    }
    if (x >= 64 && x <= 126) {
      uint8_t newRot = (cfg.screenRotation == 3) ? 1 : 3;
      cfg.screenRotation = newRot;
      display_set_rotation(newRot);
      touch_set_rotation(newRot);
      touch_save_calibration();
      settings_save();
      renderSettings();
      return;
    }
    if (x >= 127 && x <= 189) {
      cfg.resumeEnable = !cfg.resumeEnable;
      settings_save();
      renderSettings();
      return;
    }
    if (x >= 190 && x <= 252) {
      s_showAboutDialog = true;
      renderSettings();
      return;
    }
    if (x >= 253 && x <= 320) {
      ESP.restart();
      return;
    }
  }
}
