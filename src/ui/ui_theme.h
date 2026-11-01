#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// ==============================================================================
// CYD Plus Hi-Fi UI Design System
// 摆脱 AI 模板塑料感，采用 Braun / Teenage Engineering / 索尼高端砖机极简工业美学
// ==============================================================================

// ------------------------------------------------------------------------------
// 1. 哑光极简工业色彩代币 (RGB565)
// ------------------------------------------------------------------------------
#define UI_COL_BG              0x1082  // #121316 黑曜石沉浸黑底 (避免生硬的纯黑 0x0000)
#define UI_COL_SURFACE         0x18E4  // #1C1E24 第一层卡片哑光表面
#define UI_COL_SURFACE_HIGH    0x2146  // #262A32 悬浮按钮/交互块表面
#define UI_COL_SURFACE_PRESS   0x31A8  // #303642 激活按压状态高亮
#define UI_COL_BORDER_SUBTLE   0x2146  // #242830 极细微结构分割线
#define UI_COL_BORDER_LIGHT    0x31E9  // #373E4B 清晰轮廓边框

// 统一主强调色 (摒弃五颜六色霓虹乱杂感，以复古工业琥珀橙作为核心视觉灵魂)
#define UI_COL_AMBER           0xFBC0  // #FF7A00 标志性赛博琥珀橙 (Braun / Teenage Engineering)
#define UI_COL_AMBER_DIM       0x5960  // #5A2D00 沉稳暗琥珀底色
#define UI_COL_CYAN            0x069F  // #00D2FF 科技冷青 (仅用于数据指标与副强调)
#define UI_COL_CYAN_DIM        0x0188  // #003246 沉稳暗青
#define UI_COL_GREEN           0x2E6E  // #2ECC71 纯净荧光状态绿
#define UI_COL_GREEN_DIM       0x09E3  // #0F3C1E 状态绿衬底
#define UI_COL_RED             0xF249  // #F54B4B 珊瑚告警红

// 文字层级
#define UI_COL_TEXT_PRIMARY    0xF7BF  // #F5F7FA 纯净暖白 (主标题与数值)
#define UI_COL_TEXT_MUTED      0x8C94  // #8C92A0 烟岚灰 (副标题与说明)
#define UI_COL_TEXT_DIM        0x52AC  // #505560 微弱暗灰 (辅助刻度与分割)

// ------------------------------------------------------------------------------
// 2. 界面公共组件绘制函数
// ------------------------------------------------------------------------------
void ui_theme_draw_panel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t r = 5,
                         uint16_t bg = UI_COL_SURFACE, uint16_t border = UI_COL_BORDER_SUBTLE);

void ui_theme_draw_pill_btn(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
                            uint16_t textCol = UI_COL_TEXT_PRIMARY, uint16_t bg = UI_COL_SURFACE_HIGH,
                            uint16_t border = UI_COL_BORDER_LIGHT);

void ui_theme_draw_slider(int16_t x, int16_t y, int16_t w, int16_t val, int16_t maxVal,
                          uint16_t fillCol = UI_COL_AMBER);
