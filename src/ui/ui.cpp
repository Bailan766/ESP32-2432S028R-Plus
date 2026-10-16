#include "ui.h"
#include "font_cn.h"
#include "home.h"
#include "music_list.h"
#include "player_ui.h"
#include "clock_ui.h"
#include "settings.h"
#include "hardware_test_ui.h"
#include "touch_cal_ui.h"
#include "delta_pwd_ui.h"
#include "novel_list.h"
#include "novel_reader_ui.h"
#include "reader/novel.h"
#include "game_menu.h"
#include "game/game_2048.h"
#include "game/muyu.h"
#include "hardware/display.h"
#include "hardware/battery.h"
#include "hardware/audio.h"
#include "storage/sdcard.h"
#include "player/player.h"
#include "system/settings_manager.h"
#include "hardware/touch.h"
#include "../dog_image.h"

static UIPage s_currentPage = UI_PAGE_SPLASH;
static UIPage s_previousPage = UI_PAGE_SPLASH;
static unsigned long s_lastHeaderRefresh = 0;

void ui_draw_header(const char* title, bool showBackBtn) {
  TFT_eSPI& tft = display_get_tft();

  // 顶部黑底区域 (高度 30px)
  tft.fillRect(0, 0, 320, 30, COLOR_BG_DARK);
  tft.drawLine(0, 30, 320, 30, COLOR_PANEL_BORDER);

  // 返回按钮 (更醒目、更大尺寸)
  if (showBackBtn) {
    tft.fillRoundRect(4, 3, 68, 24, 4, 0x10A2);
    tft.drawRoundRect(4, 3, 68, 24, 4, COLOR_ACCENT_CYAN);
    font_draw_string(8, 7, "< 返回", COLOR_ACCENT_CYAN, 0x10A2, 1);
  }

  // 标题文字 (支持中文点阵渲染)
  font_draw_string(showBackBtn ? 78 : 10, 7, title, COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);

  // 电池电量数值与小图标 (右侧)
  char bBuf[16];
  snprintf(bBuf, sizeof(bBuf), "%.2fV", battery_get_voltage());
  tft.setTextColor(battery_is_low() ? TFT_RED : COLOR_ACCENT_GREEN, COLOR_BG_DARK);
  tft.drawString(bBuf, 226, 8, 1);

  int bx = 276, by = 8, bw = 30, bh = 14;
  tft.drawRect(bx, by, bw, bh, COLOR_TEXT_PRIMARY);
  tft.fillRect(bx + bw, by + 4, 3, 6, COLOR_TEXT_PRIMARY);
  int fillW = map(battery_get_percent(), 0, 100, 0, bw - 4);
  fillW = constrain(fillW, 0, bw - 4);
  uint16_t bCol = battery_is_low() ? TFT_RED : COLOR_ACCENT_GREEN;
  tft.fillRect(bx + 2, by + 2, fillW, bh - 4, bCol);
  tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, COLOR_BG_DARK);
}

void ui_init() {
  LOG_UI("初始化 UI 系统的字体库...");
  font_cn_init();
  novel_init();

  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);

  // 显示用户头像 (汪汪)
  tft.setSwapBytes(true);
  tft.pushImage(110, 15, dog_image_width, dog_image_height, dog_image_data);

  // 开机引导 / 开发者介绍
  font_draw_string_center(160, 125, "欢迎使用 摆烂播放器", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 155, "开发者: 摆烂233", COLOR_ACCENT_ORANGE, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 185, "邮箱: lb1493729@gmail.com", COLOR_ACCENT_GREEN, COLOR_BG_DARK, 1);
  
  font_draw_string_center(160, 220, "- 点击屏幕继续 -", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);

  // 等待触摸
  bool touched = false;
  int16_t tx, ty;
  while (!touched) {
    if (touch_read(tx, ty)) {
      touched = true;
    }
    delay(50);
  }

  // 防止点击事件立即穿透到下一个界面
  delay(200); 

  if (settings_get().bootCalibrate) {
    LOG_UI("检测到强制开机自动校准，直接跳转校准页面");
    ui_switch_to(UI_PAGE_TOUCH_CAL);
  } else {
    ui_switch_to(UI_PAGE_LOCKSCREEN);
  }
}

void ui_switch_to(UIPage page) {
  LOG_UI("UI 页面跳转: %d -> %d", s_currentPage, page);
  if (s_currentPage != page) {
    s_previousPage = s_currentPage;
  }
  s_currentPage = page;

  switch (s_currentPage) {
    case UI_PAGE_LOCKSCREEN:
      clock_ui_enter();
      break;
    case UI_PAGE_HOME:
      home_enter();
      break;
    case UI_PAGE_MUSIC_LIST:
      music_list_enter(false);
      break;
    case UI_PAGE_RECENT_LIST:
      music_list_enter(true);
      break;
    case UI_PAGE_PLAYER:
      player_ui_enter();
      break;
    case UI_PAGE_SETTINGS:
      settings_ui_enter();
      break;
    case UI_PAGE_HARDWARE_TEST:
      hardware_test_ui_enter();
      break;
    case UI_PAGE_TOUCH_CAL:
      touch_cal_ui_enter();
      break;
    case UI_PAGE_DELTA_PWD:
      delta_ui_enter();
      break;
    case UI_PAGE_NOVEL_LIST:
      novel_list_enter();
      break;
    case UI_PAGE_NOVEL_READER:
      novel_reader_ui_enter();
      break;
    case UI_PAGE_GAME_MENU:
      game_menu_enter();
      break;
    case UI_PAGE_GAME_2048:
      game_2048_enter();
      break;
    case UI_PAGE_MUYU:
      muyu_enter();
      break;
    default:
      break;
  }
}

UIPage ui_get_current_page() {
  return s_currentPage;
}

UIPage ui_get_previous_page() {
  return s_previousPage;
}

void ui_update() {
  switch (s_currentPage) {
    case UI_PAGE_LOCKSCREEN:
      clock_ui_update();
      break;
    case UI_PAGE_HOME:
      home_update();
      break;
    case UI_PAGE_MUSIC_LIST:
    case UI_PAGE_RECENT_LIST:
      music_list_update();
      break;
    case UI_PAGE_PLAYER:
      player_ui_update();
      break;
    case UI_PAGE_SETTINGS:
      settings_ui_update();
      break;
    case UI_PAGE_HARDWARE_TEST:
      hardware_test_ui_update();
      break;
    case UI_PAGE_TOUCH_CAL:
      touch_cal_ui_update();
      break;
    case UI_PAGE_DELTA_PWD:
      delta_ui_update();
      break;
    case UI_PAGE_NOVEL_LIST:
      novel_list_update();
      break;
    case UI_PAGE_NOVEL_READER:
      novel_reader_ui_update();
      break;
    case UI_PAGE_GAME_MENU:
      game_menu_update();
      break;
    case UI_PAGE_GAME_2048:
      game_2048_update();
      break;
    case UI_PAGE_MUYU:
      muyu_update();
      break;
    default:
      break;
  }

  // 每 2 秒微刷新一次头部电量指示
  if (millis() - s_lastHeaderRefresh >= 2000) {
    s_lastHeaderRefresh = millis();
    int bx = 276, by = 8, bw = 30, bh = 14;
    TFT_eSPI& tft = display_get_tft();
    int fillW = map(battery_get_percent(), 0, 100, 0, bw - 4);
    fillW = constrain(fillW, 0, bw - 4);
    uint16_t bCol = battery_is_low() ? TFT_RED : COLOR_ACCENT_GREEN;
    tft.fillRect(bx + 2, by + 2, fillW, bh - 4, bCol);
    tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, COLOR_BG_DARK);
  }
}

void ui_handle_touch(int16_t x, int16_t y) {
  switch (s_currentPage) {
    case UI_PAGE_LOCKSCREEN:
      clock_ui_handle_touch(x, y);
      break;
    case UI_PAGE_HOME:
      home_handle_touch(x, y);
      break;
    case UI_PAGE_MUSIC_LIST:
    case UI_PAGE_RECENT_LIST:
      music_list_handle_touch(x, y);
      break;
    case UI_PAGE_PLAYER:
      player_ui_handle_touch(x, y);
      break;
    case UI_PAGE_SETTINGS:
      settings_ui_handle_touch(x, y);
      break;
    case UI_PAGE_HARDWARE_TEST:
      hardware_test_ui_handle_touch(x, y);
      break;
    case UI_PAGE_TOUCH_CAL:
      touch_cal_ui_handle_touch(x, y);
      break;
    case UI_PAGE_DELTA_PWD:
      delta_ui_handle_touch(x, y);
      break;
    case UI_PAGE_NOVEL_LIST:
      novel_list_handle_touch(x, y);
      break;
    case UI_PAGE_NOVEL_READER:
      novel_reader_ui_handle_touch(x, y);
      break;
    case UI_PAGE_GAME_MENU:
      game_menu_handle_touch(x, y);
      break;
    case UI_PAGE_GAME_2048:
      game_2048_handle_touch(x, y);
      break;
    case UI_PAGE_MUYU:
      muyu_handle_touch(x, y);
      break;
    default:
      break;
  }
}

void ui_handle_button_click() {
  LOG_UI("实体按键 BOOT 单击");
  if (s_currentPage == UI_PAGE_PLAYER) {
    player_toggle();
    player_ui_enter();
  } else if (s_currentPage == UI_PAGE_HOME) {
    // 在主页单击直接进入触屏精准校准！
    LOG_UI("主页单击 BOOT -> 直达触屏校准");
    ui_switch_to(UI_PAGE_TOUCH_CAL);
  } else if (s_currentPage == UI_PAGE_TOUCH_CAL) {
    LOG_UI("校准页短按 BOOT -> 跳过校准");
    audio_test_beep(1000, 60);
    if (s_previousPage == UI_PAGE_SETTINGS) {
      ui_switch_to(UI_PAGE_SETTINGS);
    } else {
      ui_switch_to(UI_PAGE_LOCKSCREEN);
    }
  } else if (s_currentPage == UI_PAGE_LOCKSCREEN) {
    if (display_is_sleeping()) {
      display_wakeup();
    } else {
      display_sleep();
    }
  } else if (s_currentPage == UI_PAGE_GAME_2048 || s_currentPage == UI_PAGE_MUYU) {
    LOG_UI("游戏中短按 BOOT -> 返回摸鱼大厅");
    audio_test_beep(1000, 40);
    ui_switch_to(UI_PAGE_GAME_MENU);
  } else if (s_currentPage == UI_PAGE_GAME_MENU) {
    LOG_UI("摸鱼大厅短按 BOOT -> 返回主页");
    audio_test_beep(1000, 40);
    ui_switch_to(UI_PAGE_HOME);
  } else if (s_currentPage == UI_PAGE_NOVEL_READER) {
    LOG_UI("小说阅读中短按 BOOT -> 返回书架");
    audio_test_beep(1000, 40);
    novel_close();
    ui_switch_to(UI_PAGE_NOVEL_LIST);
  } else if (s_currentPage == UI_PAGE_NOVEL_LIST) {
    LOG_UI("书架短按 BOOT -> 返回主页");
    audio_test_beep(1000, 40);
    ui_switch_to(UI_PAGE_HOME);
  } else {
    // 在子页面短按 BOOT 键直接作为物理【返回】键！
    LOG_UI("物理按键触发返回主页");
    ui_switch_to(UI_PAGE_LOCKSCREEN);
  }
}

void ui_handle_button_long_press() {
  LOG_UI("实体按键 BOOT 长按 -> 重启设备");
  ESP.restart();
}
