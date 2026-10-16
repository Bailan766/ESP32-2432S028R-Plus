#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

enum UIPage {
  UI_PAGE_SPLASH,
  UI_PAGE_LOCKSCREEN,
  UI_PAGE_HOME,
  UI_PAGE_MUSIC_LIST,
  UI_PAGE_RECENT_LIST,
  UI_PAGE_PLAYER,
  UI_PAGE_SETTINGS,
  UI_PAGE_HARDWARE_TEST,
  UI_PAGE_TOUCH_CAL,
  UI_PAGE_DELTA_PWD,
  UI_PAGE_NOVEL_LIST,
  UI_PAGE_NOVEL_READER
};

void ui_init();
void ui_switch_to(UIPage page);
UIPage ui_get_current_page();
UIPage ui_get_previous_page();
void ui_update();
void ui_handle_touch(int16_t x, int16_t y);
void ui_handle_button_click();
void ui_handle_button_long_press();

// 公用头部栏绘制 (标题, 返回按钮, 电池电量)
void ui_draw_header(const char* title, bool showBackBtn);
