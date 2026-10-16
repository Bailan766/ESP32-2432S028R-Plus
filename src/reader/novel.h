#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

#define NOVEL_MAX_ITEMS 32
#define NOVEL_HISTORY_DEPTH 64

enum NovelTheme {
  THEME_DARK = 0,    // 暗黑水墨 (黑底荧光绿/白字)
  THEME_PAPER = 1,   // 羊皮纸卷 (护眼米黄底深褐字)
  THEME_GREEN = 2,   // 豆沙护眼 (浅豆沙绿底黑字)
  THEME_NIGHT = 3    // 深夜星河 (深藏青底淡青白字)
};

struct ThemeColors {
  uint16_t bg;
  uint16_t text;
  uint16_t status_bg;
  uint16_t status_text;
  uint16_t accent;
  const char* name;
};

struct NovelItem {
  char title[32];
  char path[64];
  uint32_t file_size;
  uint32_t last_offset;
  bool is_demo;
};

void novel_init();
void novel_scan_all();
int novel_get_count();
const NovelItem* novel_get_item(int index);
int novel_get_current_index();

// 阅读操作
bool novel_open(int index);
void novel_close();
bool novel_is_open();
const NovelItem* novel_get_current_book();

// 翻页与位置控制
bool novel_next_page();
bool novel_prev_page();
void novel_seek_percent(float percent);
float novel_get_progress_percent();
uint32_t novel_get_current_offset();

// 主题与色彩
NovelTheme novel_get_theme();
void novel_set_theme(NovelTheme theme);
void novel_next_theme();
const ThemeColors& novel_get_theme_colors();

// 页面内容渲染与排版
void novel_render_current_page();
void novel_save_bookmark();
