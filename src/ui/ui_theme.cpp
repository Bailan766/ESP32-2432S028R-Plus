#include "ui_theme.h"
#include "font_cn.h"
#include "hardware/display.h"

void ui_theme_draw_panel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t r,
                         uint16_t bg, uint16_t border) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillRoundRect(x, y, w, h, r, bg);
  if (border != bg) {
    tft.drawRoundRect(x, y, w, h, r, border);
  }
}

void ui_theme_draw_pill_btn(int16_t x, int16_t y, int16_t w, int16_t h, const char* label,
                            uint16_t textCol, uint16_t bg, uint16_t border) {
  TFT_eSPI& tft = display_get_tft();
  tft.fillRoundRect(x, y, w, h, 4, bg);
  tft.drawRoundRect(x, y, w, h, 4, border);
  int16_t textY = y + (h - 16) / 2;
  font_draw_string_center(x + w / 2, textY, label, textCol, bg, 1);
}

void ui_theme_draw_slider(int16_t x, int16_t y, int16_t w, int16_t val, int16_t maxVal,
                          uint16_t fillCol) {
  TFT_eSPI& tft = display_get_tft();
  val = constrain(val, 0, maxVal);
  int trackH = 4;
  int trackY = y + 8;

  // 轨道凹槽
  tft.fillRoundRect(x, trackY, w, trackH, 2, UI_COL_BORDER_SUBTLE);

  // 进度高亮段
  int fillW = (val * w) / (maxVal > 0 ? maxVal : 1);
  if (fillW > 0) {
    tft.fillRoundRect(x, trackY, fillW, trackH, 2, fillCol);
  }

  // 滑块圆角药丸手柄
  int thumbX = x + fillW;
  thumbX = constrain(thumbX, x, x + w);
  tft.fillCircle(thumbX, trackY + 2, 5, UI_COL_TEXT_PRIMARY);
  tft.drawCircle(thumbX, trackY + 2, 6, fillCol);
}
