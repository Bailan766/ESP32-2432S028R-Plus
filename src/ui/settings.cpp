#include "reader/novel.h"
#include "settings.h"
#include "ui.h"
#include "ui_theme.h"
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

  // M3 圆角分层弹窗卡片 (无生硬边框)
  ui_theme_draw_panel(16, 28, 288, 196, 16, UI_COL_SURFACE, UI_COL_SURFACE);

  font_draw_string_bold(82, 36, 180, "关于本机 · CYD-DAP", M3_PRIMARY, UI_COL_SURFACE, 1);

  font_draw_string(26, 60, "开发者: " PROJECT_AUTHOR, UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

  char verBuf[48];
  snprintf(verBuf, sizeof(verBuf), "固件: 摆烂播放器 %s %s", PROJECT_VERSION, PROJECT_EDITION);
  font_draw_string(26, 82, verBuf, M3_TERTIARY, UI_COL_SURFACE, 1);

  font_draw_string(26, 104, "硬件: ESP32-D0WD-V3 (双核 240MHz)", UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
  font_draw_string(26, 126, "屏幕: ST7789 2.8寸 (320x240)", UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);
  font_draw_string(26, 148, "触控: XPT2046 独立SPI仿射校准", UI_COL_TEXT_MUTED, UI_COL_SURFACE, 1);

  char heapBuf[48];
  snprintf(heapBuf, sizeof(heapBuf), "空闲内存: %u KB (M3 Theme)", ESP.getFreeHeap() / 1024);
  font_draw_string(26, 170, heapBuf, UI_COL_TEXT_DIM, UI_COL_SURFACE, 1);

  ui_theme_draw_badge(70, 192, 180, 26, "点击任意位置返回", M3_ON_PRIMARY, M3_PRIMARY);
}

static void render_slider(int y, int val, const char* valStr, uint16_t color) {
  TFT_eSPI& tft = display_get_tft();
  int trackX = 72, trackW = 188;

  // 清除滑块与数值区
  tft.fillRect(68, y + 6, 196, 20, UI_COL_SURFACE);
  ui_theme_draw_slider(trackX, y + 6, trackW, val, 100, color);

  // 数值文字
  tft.fillRect(266, y + 8, 40, 16, UI_COL_SURFACE);
  font_draw_string(268, y + 8, valStr, color, UI_COL_SURFACE, 1);
}

static void renderSettings() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  ui_draw_header("系统设置", true);

  SystemSettings& cfg = settings_get();

  // 1. 音量 (y: 33 ~ 65)
  ui_theme_draw_panel(10, 33, 300, 32, 12, UI_COL_SURFACE, UI_COL_SURFACE);
  font_draw_string_bold(20, 41, 48, "音量", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);
  char volBuf[16];
  snprintf(volBuf, sizeof(volBuf), "%d%%", cfg.volume);
  render_slider(33, cfg.volume, volBuf, M3_PRIMARY);

  // 2. 屏幕亮度 (y: 69 ~ 101)
  ui_theme_draw_panel(10, 69, 300, 32, 12, UI_COL_SURFACE, UI_COL_SURFACE);
  font_draw_string_bold(20, 77, 48, "亮度", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);
  char briBuf[16];
  snprintf(briBuf, sizeof(briBuf), "%d%%", cfg.brightness);
  render_slider(69, cfg.brightness, briBuf, M3_TERTIARY);

  // 3. 自动息屏时间 (y: 105 ~ 137)
  ui_theme_draw_panel(10, 105, 300, 32, 12, UI_COL_SURFACE, UI_COL_SURFACE);
  font_draw_string_bold(20, 113, 64, "电源", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE, 1);

  const char* sleepStr = "常亮";
  if (cfg.autoSleepSeconds == 30) sleepStr = "30秒";
  else if (cfg.autoSleepSeconds == 60) sleepStr = "1分钟";
  else if (cfg.autoSleepSeconds == 180) sleepStr = "3分钟";
  else if (cfg.autoSleepSeconds == 300) sleepStr = "5分钟";

  ui_theme_draw_badge(85, 108, 55, 26, "锁屏", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);
  ui_theme_draw_badge(145, 108, 55, 26, "息屏", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH);
  ui_theme_draw_badge(210, 108, 94, 26, sleepStr, M3_PRIMARY, M3_PRIMARY_CONTAINER);

  // 4. 第一排功能胶囊按钮 (y: 141 ~ 177, h: 36)
  const char* sdBtn = sdcard_is_available() ? "重扫TF卡" : "挂载TF卡";
  ui_theme_draw_badge(10, 141, 94, 36, sdBtn, M3_PRIMARY, M3_PRIMARY_CONTAINER);
  ui_theme_draw_badge(113, 141, 94, 36, "硬件自检", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE);

  const char* resTxt = cfg.resumeEnable ? "续播:开启" : "续播:关闭";
  ui_theme_draw_badge(216, 141, 94, 36, resTxt,
                      cfg.resumeEnable ? M3_SAGE : UI_COL_TEXT_MUTED,
                      cfg.resumeEnable ? M3_SAGE_CONTAINER : UI_COL_SURFACE);

  // 5. 第二排功能胶囊按钮 (y: 181 ~ 217, h: 36)
  ui_theme_draw_badge(10, 181, 70, 36, "耗电曲线", M3_TERTIARY, M3_TERTIARY_CONTAINER);
  ui_theme_draw_badge(85, 181, 70, 36, "屏幕旋转", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE);
  ui_theme_draw_badge(160, 181, 70, 36, "关于本机", M3_PRIMARY, M3_PRIMARY_CONTAINER);
  ui_theme_draw_badge(235, 181, 75, 36, "重启系统", UI_COL_RED, UI_COL_SURFACE);

  // 6. 底部胶囊徽章状态栏
  if (s_showAboutDialog) {
    renderAboutModal();
  } else {
    ui_theme_draw_badge(28, 220, 264, 18, "摆烂播放器 " PROJECT_VERSION " · Material You", UI_COL_TEXT_MUTED, UI_COL_SURFACE);
  }
}

void settings_ui_enter() {
  s_showAboutDialog = false;
  renderSettings();
}

void settings_ui_update() {
}

void settings_ui_handle_touch(int16_t x, int16_t y) {
  // 如果弹出了关于对话框，任意点击关闭对话框
  if (s_showAboutDialog) {
    audio_test_beep(1000, 30);
    s_showAboutDialog = false;
    renderSettings();
    return;
  }

  // 1. 顶部返回主菜单按钮 (x: 0 ~ 76, y: 0 ~ 32)
  if (x <= 76 && y <= 32) {
    audio_test_beep(1000, 30);
    ui_switch_to(UI_PAGE_HOME);
    return;
  }

  SystemSettings& cfg = settings_get();

  // 2. 音量调节行 (y: 33 ~ 65)
  if (y >= 33 && y <= 65) {
    int newVol = (x - 72) * 100 / 188;
    newVol = constrain(newVol, 0, 100);
    cfg.volume = (uint8_t)newVol;
    audio_set_volume(cfg.volume);
    settings_save();
    char volBuf[16];
    snprintf(volBuf, sizeof(volBuf), "%d%%", cfg.volume);
    render_slider(33, cfg.volume, volBuf, UI_COL_AMBER);
    audio_test_beep(800 + cfg.volume * 8, 30);
    return;
  }

  // 3. 亮度调节行 (y: 69 ~ 101)
  if (y >= 69 && y <= 101) {
    int newBri = (x - 72) * 100 / 188;
    newBri = constrain(newBri, 5, 100);
    cfg.brightness = (uint8_t)newBri;
    display_set_brightness(cfg.brightness);
    settings_save();
    char briBuf[16];
    snprintf(briBuf, sizeof(briBuf), "%d%%", cfg.brightness);
    render_slider(69, cfg.brightness, briBuf, UI_COL_CYAN);
    audio_test_beep(1200, 30);
    return;
  }

  // 4. 息屏控制行 (y: 105 ~ 137)
  if (y >= 105 && y <= 137) {
    // 立即锁屏 (x: 80 ~ 140)
    if (x >= 80 && x <= 140) {
      audio_test_beep(800, 40);
      ui_switch_to(UI_PAGE_LOCKSCREEN);
      return;
    }
    // 立即息屏 (x: 141 ~ 205)
    if (x >= 141 && x <= 205) {
      audio_test_beep(600, 40);
      display_sleep();
      return;
    }
    // 切换息屏时间 (x: 206 ~ 310)
    if (x >= 206 && x <= 310) {
      audio_test_beep(1200, 40);
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

  // 5. 第一排功能按钮 (y: 140 ~ 177, 3 个大按钮)
  if (y >= 140 && y <= 177) {
    // 挂载/重扫TF卡 (x: 0 ~ 106)
    if (x >= 0 && x <= 106) {
      audio_test_beep(2000, 100);
      sdcard_reinit(true);
      scanner_scan_all();
      novel_scan_all();
      renderSettings();
      return;
    }
    // 硬件自检 (x: 107 ~ 210)
    if (x >= 107 && x <= 210) {
      audio_test_beep(1000, 40);
      ui_switch_to(UI_PAGE_HARDWARE_TEST);
      return;
    }
    // 断点续播 (x: 211 ~ 320)
    if (x >= 211 && x <= 320) {
      cfg.resumeEnable = !cfg.resumeEnable;
      settings_save();
      audio_test_beep(1400, 60);
      renderSettings();
      return;
    }
  }

  // 6. 第二排功能按钮 (y: 180 ~ 218, 4 个按钮)
  if (y >= 180 && y <= 218) {
    // 耗电曲线 (x: 0 ~ 80)
    if (x >= 0 && x < 80) {
      audio_test_beep(1200, 30);
      ui_switch_to(UI_PAGE_BATTERY_CURVE);
      return;
    }
    // 屏幕旋转 (x: 80 ~ 155)
    if (x >= 80 && x < 155) {
      uint8_t newRot = (cfg.screenRotation == 3) ? 1 : 3;
      cfg.screenRotation = newRot;
      display_set_rotation(newRot);
      touch_set_rotation(newRot);
      touch_save_calibration();
      settings_save();
      renderSettings();
      return;
    }
    // 关于本机 (x: 155 ~ 230)
    if (x >= 155 && x < 230) {
      audio_test_beep(1200, 40);
      s_showAboutDialog = true;
      renderSettings();
      return;
    }
    // 重启系统 (x: 230 ~ 320)
    if (x >= 230 && x <= 320) {
      audio_test_beep(600, 100);
      ESP.restart();
      return;
    }
  }
}
