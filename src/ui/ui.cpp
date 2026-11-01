#include "web_manager_ui.h"
#include "web_radio_ui.h"
#include "system/web_file_manager.h"
#include "radio/web_radio.h"
#include "web_radio_ui.h"
#include "battery_curve_ui.h"
#include "ui.h"
#include "ui_theme.h"
#include "font_cn.h"
#include "home.h"
#include "music_list.h"
#include "player_ui.h"
#include "clock_ui.h"
#include "settings.h"
#include "hardware_test_ui.h"
#include "touch_cal_ui.h"
#include "novel_list.h"
#include "novel_reader_ui.h"
#include "reader/novel.h"
#include "game_menu.h"
#include "bt_speaker_ui.h"
#include "game/game_2048.h"
#include "game/muyu.h"
#include "hardware/display.h"
#include "hardware/battery.h"
#include "hardware/audio.h"
#include "storage/sdcard.h"
#include "storage/scanner.h"
#include "hardware/buttons.h"
#include "player/player.h"
#include "system/settings_manager.h"
#include "hardware/touch.h"
#include "../dog_image.h"

static UIPage s_currentPage = UI_PAGE_SPLASH;
static UIPage s_previousPage = UI_PAGE_SPLASH;
static unsigned long s_lastHeaderRefresh = 0;

static bool page_has_standard_header(UIPage p) {
  return p != UI_PAGE_LOCKSCREEN &&
         p != UI_PAGE_SPLASH &&
         p != UI_PAGE_TOUCH_CAL &&
         p != UI_PAGE_NOVEL_READER &&
         p != UI_PAGE_GAME_2048;
}

void ui_draw_header(const char* title, bool showBackBtn) {
  display_get_tft().setTextDatum(TL_DATUM);
  display_get_tft().setTextFont(1);
  display_get_tft().setTextSize(1);
  TFT_eSPI& tft = display_get_tft();

  // 顶部黑底区域 (高度 30px)
  tft.fillRect(0, 0, 320, 30, UI_COL_BG);
  tft.drawLine(0, 29, 320, 29, UI_COL_BORDER_SUBTLE);

  // 返回按钮 (更精致的药丸胶囊)
  if (showBackBtn) {
    tft.fillRoundRect(6, 4, 58, 22, 4, UI_COL_SURFACE_HIGH);
    tft.drawRoundRect(6, 4, 58, 22, 4, UI_COL_BORDER_LIGHT);
    font_draw_string(14, 7, "< 返回", UI_COL_TEXT_PRIMARY, UI_COL_SURFACE_HIGH, 1);
  }

  // 标题文字 (支持中文点阵渲染)
  font_draw_string_clipped(showBackBtn ? 70 : 12, 7, showBackBtn ? 140 : 198, title, UI_COL_TEXT_PRIMARY, UI_COL_BG, 1);

  // 电池电量数值与小图标 (右侧)
  char bBuf[24];
  uint8_t pct = battery_get_percent();
  float v = battery_get_voltage();
  bool usb = battery_is_usb_powered();
  snprintf(bBuf, sizeof(bBuf), "%d%% %.2fV", pct, v);
  tft.setTextColor(battery_is_low() ? UI_COL_RED : (usb ? UI_COL_AMBER : UI_COL_TEXT_MUTED), UI_COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextSize(1);
  int16_t bTextX = 274 - (int16_t)strlen(bBuf) * 6;
  tft.drawString(bBuf, bTextX, 11, 1);

  int bx = 278, by = 8, bw = 30, bh = 14;
  tft.drawRoundRect(bx, by, bw, bh, 3, UI_COL_BORDER_LIGHT);
  tft.fillRect(bx + bw, by + 4, 2, 6, UI_COL_BORDER_LIGHT);
  int fillW = map(pct, 0, 100, 0, bw - 4);
  fillW = constrain(fillW, 0, bw - 4);
  uint16_t bCol = battery_is_low() ? UI_COL_RED : (usb ? UI_COL_AMBER : UI_COL_GREEN);
  tft.fillRect(bx + 2, by + 2, fillW, bh - 4, bCol);
  tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, UI_COL_BG);
}

void ui_init() {
  LOG_UI("初始化 UI 系统的字体库...");
  font_cn_init();
  novel_init();

  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(UI_COL_BG);

  // 显示用户头像 (汪汪)
  tft.setSwapBytes(true);
  tft.pushImage(110, 15, dog_image_width, dog_image_height, dog_image_data);

  // 开机引导 / 开发者介绍 (极简工业排版)
  font_draw_string_center(160, 125, "摆烂播放器 · CYD-DAP", UI_COL_TEXT_PRIMARY, UI_COL_BG, 1);
  font_draw_string_center(160, 155, "开发者: 摆烂233", UI_COL_AMBER, UI_COL_BG, 1);
  font_draw_string_center(160, 185, "邮箱: lb1493729@gmail.com", UI_COL_TEXT_MUTED, UI_COL_BG, 1);
  
  font_draw_string_center(160, 218, "- 轻触屏幕继续 -", UI_COL_TEXT_MUTED, UI_COL_BG, 1);

  // 等待触摸
  bool touched = false;
  bool goCalibration = false;
  int16_t tx, ty;
  unsigned long splashStart = millis();

  while (!touched) {
    if (touch_read(tx, ty)) {
      touched = true;
      break;
    }

    // 物理按键兜底
    ButtonEvent evt = button_update();
    if (evt == BTN_EVENT_LONG_PRESS) {
      LOG_UI("开机画面: BOOT 长按 -> 进入触屏校准");
      goCalibration = true;
      break;
    }
    if (evt == BTN_EVENT_CLICK) {
      LOG_UI("开机画面: BOOT 短按 -> 跳过等待");
      break;
    }

    // 超时自动进入，避免设备卡死在开机画面
    if (millis() - splashStart > 2500) {
      LOG_UI("开机画面: 展示完毕，自动进入系统");
      break;
    }

    delay(50);
  }

  // 防止点击事件立即穿透到下一个界面
  delay(200); 

  if (goCalibration) {
    LOG_UI("检测到硬件 BOOT 键长按，进入触屏校准");
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
    case UI_PAGE_BLUETOOTH_SINK:
      bt_speaker_ui_enter();
      break;
    case UI_PAGE_WEB_MANAGER:
      web_manager_ui_enter();
      break;
    case UI_PAGE_WEB_RADIO:
      web_radio_ui_enter();
      break;
    case UI_PAGE_BATTERY_CURVE:
      battery_curve_ui_enter();
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
  // 息屏时跳过页面重绘：背光已关闭，重绘纯属浪费 CPU 与 SPI 带宽。
  if (display_is_sleeping() && s_currentPage != UI_PAGE_TOUCH_CAL) return;

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
    case UI_PAGE_BLUETOOTH_SINK:
      bt_speaker_ui_update();
      break;
    case UI_PAGE_WEB_MANAGER:
      web_manager_ui_update();
      break;
    case UI_PAGE_WEB_RADIO:
      web_radio_ui_update();
      break;
    case UI_PAGE_BATTERY_CURVE:
      battery_curve_ui_update();
      break;
    default:
      break;
  }

  // 每 2 秒微刷新一次通用头部电量指示
  if (millis() - s_lastHeaderRefresh >= 2000) {
    s_lastHeaderRefresh = millis();
    if (page_has_standard_header(s_currentPage) &&
        s_currentPage != UI_PAGE_SPLASH && 
        s_currentPage != UI_PAGE_TOUCH_CAL) {
      int bx = 278, by = 8, bw = 30, bh = 14;
      TFT_eSPI& tft = display_get_tft();
      uint8_t pct = battery_get_percent();
      bool usb = battery_is_usb_powered();
      int fillW = map(pct, 0, 100, 0, bw - 4);
      fillW = constrain(fillW, 0, bw - 4);
      uint16_t bCol = battery_is_low() ? UI_COL_RED : (usb ? UI_COL_AMBER : UI_COL_GREEN);
      tft.fillRect(bx + 2, by + 2, fillW, bh - 4, bCol);
      tft.fillRect(bx + 2 + fillW, by + 2, (bw - 4) - fillW, bh - 4, UI_COL_BG);
    }
  }
}

void ui_handle_touch(int16_t x, int16_t y) {
  // 顶栏右上角电池区域快捷点击 -> 直接打开耗电曲线
  if (y <= 30 && x >= 216 && page_has_standard_header(s_currentPage) &&
      s_currentPage != UI_PAGE_BLUETOOTH_SINK &&
      s_currentPage != UI_PAGE_WEB_RADIO &&
      s_currentPage != UI_PAGE_WEB_MANAGER &&
      s_currentPage != UI_PAGE_LOCKSCREEN && 
      s_currentPage != UI_PAGE_TOUCH_CAL && 
      s_currentPage != UI_PAGE_SPLASH &&
      s_currentPage != UI_PAGE_BATTERY_CURVE) {
    audio_test_beep(1400, 30);
    ui_switch_to(UI_PAGE_BATTERY_CURVE);
    return;
  }

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
    case UI_PAGE_BLUETOOTH_SINK:
      bt_speaker_ui_handle_touch(x, y);
      break;
    case UI_PAGE_WEB_MANAGER:
      web_manager_ui_handle_touch(x, y);
      break;
    case UI_PAGE_WEB_RADIO:
      web_radio_ui_handle_touch(x, y);
      break;
    case UI_PAGE_BATTERY_CURVE:
      battery_curve_ui_handle_touch(x, y);
      break;
    default:
      break;
  }
}

void ui_handle_button_click() {
  LOG_UI("实体按键 BOOT 单击");
  if (s_currentPage == UI_PAGE_PLAYER) {
    LOG_UI("播放器中短按 BOOT -> 返回主页");
    audio_test_beep(1000, 40);
    ui_switch_to(UI_PAGE_HOME);
  } else if (s_currentPage == UI_PAGE_HOME) {
    LOG_UI("主页单击 BOOT -> 锁屏");
    ui_switch_to(UI_PAGE_LOCKSCREEN);
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
  } else if (s_currentPage == UI_PAGE_BATTERY_CURVE) {
    LOG_UI("耗电曲线中短按 BOOT -> 返回");
    audio_test_beep(1000, 40);
    ui_switch_to(s_previousPage != UI_PAGE_SPLASH ? s_previousPage : UI_PAGE_SETTINGS);
  } else if (s_currentPage == UI_PAGE_BLUETOOTH_SINK) {
    bt_speaker_ui_leave();
    ui_switch_to(UI_PAGE_HOME);
  } else if (s_currentPage == UI_PAGE_WEB_RADIO) {
    web_radio_stop();
    ui_switch_to(UI_PAGE_GAME_MENU);
  } else if (s_currentPage == UI_PAGE_WEB_MANAGER) {
    web_file_manager_stop();
    ui_switch_to(UI_PAGE_GAME_MENU);
  } else {
    LOG_UI("物理按键触发返回主页");
    ui_switch_to(UI_PAGE_LOCKSCREEN);
  }
}

void ui_handle_button_long_press() {
  LOG_UI("实体按键 BOOT 长按 -> 进入触屏校准");
  audio_test_beep(1000, 80);
  ui_switch_to(UI_PAGE_TOUCH_CAL);
}
