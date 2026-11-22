#include "ui_theme.h"
#include "font_cn.h"
#include "hardware/display.h"
#include <math.h>

static uint16_t blend565(uint16_t c1, uint16_t c2, uint8_t t255) {
  uint16_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  uint16_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  uint16_t r = r1 + (((int)r2 - (int)r1) * t255) / 255;
  uint16_t g = g1 + (((int)g2 - (int)g1) * t255) / 255;
  uint16_t b = b1 + (((int)b2 - (int)b1) * t255) / 255;
  return (r << 11) | (g << 5) | b;
}

void ui_theme_draw_panel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t r,
                         uint16_t bg, uint16_t border) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillRoundRect(x, y, w, h, r, bg);
  if (border != bg && border != UI_COL_BORDER_SUBTLE) {
    tft.drawRoundRect(x, y, w, h, r, border);
  }
}

void ui_theme_draw_gradient_card(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t r,
                                 uint16_t colTop, uint16_t colBottom) {
  TFT_eSPI& tft = display_get_tft();
  if (h <= 0 || w <= 0) return;
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;

  for (int16_t row = 0; row < h; row++) {
    int16_t dx = 0;
    if (row < (int16_t)r) {
      int16_t dy = (int16_t)r - row;
      dx = (int16_t)r - (int16_t)(sqrtf((float)(r * r - dy * dy)) + 0.5f);
    } else if (row >= h - (int16_t)r) {
      int16_t dy = row - (h - 1 - (int16_t)r);
      dx = (int16_t)r - (int16_t)(sqrtf((float)(r * r - dy * dy)) + 0.5f);
    }
    uint8_t t = (h > 1) ? (uint8_t)((row * 255) / (h - 1)) : 0;
    uint16_t rowCol = blend565(colTop, colBottom, t);
    int16_t lineW = w - 2 * dx;
    if (lineW > 0) {
      tft.drawFastHLine(x + dx, y + row, lineW, rowCol);
    }
  }
}

void ui_theme_draw_pill_btn(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
                            uint16_t textCol, uint16_t bg, uint16_t border) {
  TFT_eSPI& tft = display_get_tft();
  uint16_t r = (h > 4) ? (uint16_t)(h / 2) : 2;
  if (r > 16) r = 16;
  tft.fillRoundRect(x, y, w, h, r, bg);
  if (border != bg && border != UI_COL_BORDER_SUBTLE && border != UI_COL_SURFACE_HIGH) {
    tft.drawRoundRect(x, y, w, h, r, border);
  }
  int16_t textY = y + (h - 16) / 2;
  font_draw_string_center(x + w / 2, textY, label, textCol, bg, 1);
}

void ui_theme_draw_badge(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
                         uint16_t textCol, uint16_t bgCol) {
  TFT_eSPI& tft = display_get_tft();
  uint16_t r = (uint16_t)(h / 2);
  tft.fillRoundRect(x, y, w, h, r, bgCol);
  int16_t textY = y + (h - 16) / 2;
  font_draw_string_center(x + w / 2, textY, label, textCol, bgCol, 1);
}

void ui_theme_draw_icon_badge(int16_t cx, int16_t cy, int16_t size, uint16_t bgCol,
                              uint16_t fgCol, M3IconType icon, bool scalloped) {
  TFT_eSPI& tft = display_get_tft();
  if (scalloped) {
    // M3 有机四叶花瓣造型衬底
    int16_t off = size / 5;
    int16_t cr = (size * 34) / 100;
    tft.fillCircle(cx - off, cy, cr, bgCol);
    tft.fillCircle(cx + off, cy, cr, bgCol);
    tft.fillCircle(cx, cy - off, cr, bgCol);
    tft.fillCircle(cx, cy + off, cr, bgCol);
    tft.fillRect(cx - off, cy - off, off * 2, off * 2, bgCol);
  } else {
    int16_t half = size / 2;
    tft.fillRoundRect(cx - half, cy - half, size, size, size / 3, bgCol);
  }

  // 矢量 M3 极简图标
  switch (icon) {
    case M3_ICON_NOTE:
      tft.fillCircle(cx - 4, cy + 4, 2, fgCol);
      tft.fillCircle(cx + 3, cy + 3, 2, fgCol);
      tft.drawFastVLine(cx - 2, cy - 5, 9, fgCol);
      tft.drawFastVLine(cx + 5, cy - 6, 9, fgCol);
      tft.drawLine(cx - 2, cy - 5, cx + 5, cy - 6, fgCol);
      tft.drawLine(cx - 2, cy - 4, cx + 5, cy - 5, fgCol);
      break;
    case M3_ICON_BOOK:
      tft.drawRoundRect(cx - 6, cy - 5, 12, 10, 2, fgCol);
      tft.drawFastVLine(cx, cy - 5, 10, fgCol);
      tft.drawFastHLine(cx - 4, cy - 2, 3, fgCol);
      tft.drawFastHLine(cx + 2, cy - 2, 3, fgCol);
      tft.drawFastHLine(cx - 4, cy + 1, 3, fgCol);
      tft.drawFastHLine(cx + 2, cy + 1, 3, fgCol);
      break;
    case M3_ICON_RADIO:
      tft.fillCircle(cx, cy + 2, 2, fgCol);
      tft.drawCircle(cx, cy + 2, 5, fgCol);
      tft.drawFastHLine(cx - 4, cy + 5, 9, bgCol);
      tft.drawFastVLine(cx, cy + 2, 4, fgCol);
      break;
    case M3_ICON_BT:
      tft.drawFastVLine(cx, cy - 6, 13, fgCol);
      tft.drawLine(cx, cy - 6, cx + 4, cy - 3, fgCol);
      tft.drawLine(cx + 4, cy - 3, cx - 3, cy + 3, fgCol);
      tft.drawLine(cx, cy + 6, cx + 4, cy + 3, fgCol);
      tft.drawLine(cx + 4, cy + 3, cx - 3, cy - 3, fgCol);
      break;
    case M3_ICON_CLOCK:
      tft.drawCircle(cx, cy, 6, fgCol);
      tft.drawFastVLine(cx, cy - 3, 4, fgCol);
      tft.drawFastHLine(cx, cy, 3, fgCol);
      break;
    case M3_ICON_SLIDERS:
      tft.drawFastHLine(cx - 6, cy - 3, 12, fgCol);
      tft.drawFastHLine(cx - 6, cy + 3, 12, fgCol);
      tft.fillCircle(cx + 2, cy - 3, 2, fgCol);
      tft.fillCircle(cx - 2, cy + 3, 2, fgCol);
      break;
    case M3_ICON_SD:
      tft.drawRoundRect(cx - 5, cy - 6, 10, 12, 2, fgCol);
      tft.drawFastVLine(cx - 2, cy - 4, 3, fgCol);
      tft.drawFastVLine(cx + 1, cy - 4, 3, fgCol);
      tft.drawFastVLine(cx + 3, cy - 4, 3, fgCol);
      break;
    case M3_ICON_GAME:
      tft.fillRoundRect(cx - 5, cy - 5, 4, 4, 1, fgCol);
      tft.fillRoundRect(cx + 1, cy - 5, 4, 4, 1, fgCol);
      tft.fillRoundRect(cx - 5, cy + 1, 4, 4, 1, fgCol);
      tft.drawRoundRect(cx + 1, cy + 1, 4, 4, 1, fgCol);
      break;
  }
}

void ui_theme_draw_slider(int16_t x, int16_t y, int16_t w, int16_t val, int16_t maxVal,
                          uint16_t fillCol) {
  TFT_eSPI& tft = display_get_tft();
  val = constrain(val, 0, maxVal);
  int trackH = 6;
  int trackY = y + 7;

  // M3 粗轨道凹槽
  tft.fillRoundRect(x, trackY, w, trackH, 3, UI_COL_SURFACE_PRESS);

  // M3 活跃填充段
  int fillW = (val * w) / (maxVal > 0 ? maxVal : 1);
  if (fillW > 2) {
    tft.fillRoundRect(x, trackY, fillW, trackH, 3, fillCol);
  }

  // M3 现代竖条手柄 (Vertical Handle)
  int thumbX = constrain(x + fillW, x + 2, x + w - 2);
  tft.fillRect(thumbX - 3, trackY - 4, 6, 14, UI_COL_BG);
  tft.fillRoundRect(thumbX - 2, trackY - 3, 4, 12, 2, fillCol);
}
