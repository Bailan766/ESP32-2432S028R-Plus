#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

void font_cn_init();
const uint8_t* font_find_glyph(uint16_t unicode);
uint16_t font_gb2312_to_unicode(uint8_t b1, uint8_t b2);
void font_draw_char(int16_t x, int16_t y, uint16_t unicode, uint16_t color, uint16_t bg, uint8_t scale = 1);
void font_draw_string(int16_t x, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale = 1);
void font_draw_string_center(int16_t cx, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale = 1);
int16_t font_get_string_width(const char* utf8_str, uint8_t scale = 1);
