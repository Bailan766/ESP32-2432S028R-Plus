#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// ==============================================================================
// CYD Plus Material Design 3 (MD3) Dynamic Rose & Burgundy UI Design System
// 采用 M3 动态色调分层：柔和粉/玫瑰色渐变主卡 + 暖梅灰容器层级 + 大圆角无框 + 有机图标底衬 + 悬浮胶囊导航
// ==============================================================================

// ------------------------------------------------------------------------------
// 1. Material 3 Dark Rose / Burgundy 色调代币 (RGB565)
// ------------------------------------------------------------------------------
#define UI_COL_BG              0x1082  // #141012 M3 Surface (深暖黑梅底，低对比度沉浸)
#define UI_COL_SURFACE         0x20C3  // #221A1D M3 SurfaceContainer (卡片常规层)
#define UI_COL_SURFACE_HIGH    0x2904  // #2D2226 M3 SurfaceContainerHigh (悬浮底栏/交互层)
#define UI_COL_SURFACE_PRESS   0x3966  // #3B2D32 M3 SurfaceContainerHighest (按压/徽章底)
#define UI_COL_BORDER_SUBTLE   0x2904  // 色调融合边（无生硬细线）
#define UI_COL_BORDER_LIGHT    0x61A8  // #633545 深酒红轮廓高亮

// M3 动态主色调 (Warm Rose / Pink -> Burgundy Seed #FFB1C8)
#define M3_PRIMARY             0xFD99  // #FFB1C8 柔和暖粉主强调色
#define M3_ON_PRIMARY          0x3803  // #3E001D 高对比度深酒红文字（用于粉/玫瑰主卡上）
#define M3_PRIMARY_CONTAINER   0x61A8  // #633545 深酒红容器（底栏选中胶囊/图标底衬）
#define M3_SECONDARY_CONTAINER 0x4987  // #4D303A 暖梅灰次级容器
#define M3_HERO_START          0xFE1A  // #FFC2D4 柔和粉红渐变起点
#define M3_HERO_END            0xE455  // #E68AA8 柔和玫瑰粉渐变终点
#define M3_TERTIARY            0xF5F4  // #F2BFA4 暖杏蜜桃辅色
#define M3_TERTIARY_CONTAINER  0x51C5  // #573B2D 暖棕杏容器
#define M3_SAGE                0xA6B7  // #A7D7B8 莫兰迪鼠尾草绿
#define M3_SAGE_CONTAINER      0x2206  // #254030 深鼠尾草绿容器

// 兼容历史代币别名，保证全工程页面无缝统一切换为 MD3 色盘
#define UI_COL_AMBER           M3_PRIMARY
#define UI_COL_AMBER_DIM       M3_PRIMARY_CONTAINER
#define UI_COL_CYAN            M3_TERTIARY
#define UI_COL_CYAN_DIM        M3_TERTIARY_CONTAINER
#define UI_COL_GREEN           M3_SAGE
#define UI_COL_GREEN_DIM       M3_SAGE_CONTAINER
#define UI_COL_RED             0xFA6D  // #FFB4AB M3 柔珊瑚红

// M3 文字排版层级
#define UI_COL_TEXT_PRIMARY    0xF75D  // #F7EBEE 亮暖白 (粗体主标题)
#define UI_COL_TEXT_MUTED      0xCD97  // #C8B4BA 暖灰 (副标题/说明)
#define UI_COL_TEXT_DIM        0x7B4E  // #7E6B71 暗灰 (辅助标签)

enum M3IconType : uint8_t {
  M3_ICON_NOTE = 0,
  M3_ICON_BOOK,
  M3_ICON_RADIO,
  M3_ICON_BT,
  M3_ICON_CLOCK,
  M3_ICON_SLIDERS,
  M3_ICON_SD,
  M3_ICON_GAME
};

// ------------------------------------------------------------------------------
// 2. Material 3 公共组件绘制函数
// ------------------------------------------------------------------------------
void ui_theme_draw_panel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t r = 12,
                         uint16_t bg = UI_COL_SURFACE, uint16_t border = UI_COL_SURFACE);

void ui_theme_draw_gradient_card(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t r,
                                 uint16_t colTop, uint16_t colBottom);

void ui_theme_draw_pill_btn(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
                            uint16_t textCol = UI_COL_TEXT_PRIMARY, uint16_t bg = UI_COL_SURFACE_HIGH,
                            uint16_t border = UI_COL_SURFACE_HIGH);

void ui_theme_draw_badge(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
                         uint16_t textCol, uint16_t bgCol);

void ui_theme_draw_icon_badge(int16_t cx, int16_t cy, int16_t size, uint16_t bgCol,
                              uint16_t fgCol, M3IconType icon, bool scalloped = false);

void ui_theme_draw_slider(int16_t x, int16_t y, int16_t w, int16_t val, int16_t maxVal,
                          uint16_t fillCol = M3_PRIMARY);
