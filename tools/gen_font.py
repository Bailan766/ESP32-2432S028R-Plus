import os
from PIL import Image, ImageFont, ImageDraw

# UI 中用到的所有中文字符集
text = """
摆烂播放器音乐曲库最近目录文件设置硬件自检
返回停止暂停就绪正在已未检测到挂载容量
音量亮度自动息屏秒分钟关闭开关
顺序单曲全部循环随机上一首下一首快退快进变奏模式
消噪功放电池电压触摸按键按下松开电量
系统运行状态测试切换色彩实时采样正常
演示超级马里奥俄罗斯方块菊次郎的夏天卡农抬棺电音神曲久石让
页码共暂无记录为空请插入或使用内置第卡
艺术家当前已选择未神曲提示开机就绪
全部播放列表刷新完成成功失败帕赫贝尔
校准十字中心点击轻触准确度画板笔滑动重置保存原始左上角右上角右下角左下角精密精准
"""

chars = sorted(list(set(c for c in text if '\u4e00' <= c <= '\u9fff')))
font = ImageFont.truetype('C:/Windows/Fonts/simsun.ttc', 14)

glyphs = []
for c in chars:
    unicode_val = ord(c)
    img = Image.new('1', (16, 16), 0)
    draw = ImageDraw.Draw(img)
    draw.text((1, 0), c, font=font, fill=1)
    
    bytes_arr = []
    for y in range(16):
        b0 = 0
        b1 = 0
        for x in range(8):
            if img.getpixel((x, y)):
                b0 |= (1 << (7 - x))
        for x in range(8, 16):
            if img.getpixel((x, y)):
                b1 |= (1 << (15 - x))
        bytes_arr.append(b0)
        bytes_arr.append(b1)
    glyphs.append((unicode_val, c, bytes_arr))

print(f"Total glyphs generated: {len(glyphs)}")

# 生成 src/ui/font_cn.h
with open('src/ui/font_cn.h', 'w', encoding='utf-8') as f:
    f.write("""#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

void font_cn_init();
void font_draw_char(int16_t x, int16_t y, uint16_t unicode, uint16_t color, uint16_t bg, uint8_t scale = 1);
void font_draw_string(int16_t x, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale = 1);
void font_draw_string_center(int16_t cx, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale = 1);
int16_t font_get_string_width(const char* utf8_str, uint8_t scale = 1);
""")

# 生成 src/ui/font_cn.cpp
with open('src/ui/font_cn.cpp', 'w', encoding='utf-8') as f:
    f.write("""#include "font_cn.h"
#include "hardware/display.h"
#include "config.h"

struct Glyph16 {
  uint16_t unicode;
  uint8_t data[32];
};

static const Glyph16 PROGMEM s_glyphs[] = {
""")
    for val, ch, b in glyphs:
        hex_data = ', '.join(f"0x{byte:02X}" for byte in b)
        f.write(f"  {{ 0x{val:04X}, {{ {hex_data} }} }}, // {ch}\n")
    
    f.write("""};

static const int GLYPH_COUNT = sizeof(s_glyphs) / sizeof(Glyph16);

void font_cn_init() {
}

static const uint8_t* find_glyph(uint16_t unicode) {
  int low = 0, high = GLYPH_COUNT - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    uint16_t midUni = pgm_read_word(&(s_glyphs[mid].unicode));
    if (midUni == unicode) {
      return s_glyphs[mid].data;
    }
    if (midUni < unicode) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  return nullptr;
}

void font_draw_char(int16_t x, int16_t y, uint16_t unicode, uint16_t color, uint16_t bg, uint8_t scale) {
  TFT_eSPI& tft = display_get_tft();
  const uint8_t* glyphData = find_glyph(unicode);
  if (!glyphData) {
    if (scale == 1) tft.drawRect(x, y, 16, 16, color);
    else tft.drawRect(x, y, 16 * scale, 16 * scale, color);
    return;
  }

  for (int r = 0; r < 16; r++) {
    uint8_t b0 = pgm_read_byte(glyphData + r * 2);
    uint8_t b1 = pgm_read_byte(glyphData + r * 2 + 1);
    uint16_t row = ((uint16_t)b0 << 8) | b1;

    for (int c = 0; c < 16; c++) {
      bool on = (row >> (15 - c)) & 1;
      uint16_t pixelColor = on ? color : bg;
      if (scale == 1) {
        tft.drawPixel(x + c, y + r, pixelColor);
      } else {
        tft.fillRect(x + c * scale, y + r * scale, scale, scale, pixelColor);
      }
    }
  }
}

void font_draw_string(int16_t x, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale) {
  TFT_eSPI& tft = display_get_tft();
  int16_t curX = x;
  const uint8_t* p = (const uint8_t*)utf8_str;

  while (*p) {
    if (*p < 0x80) {
      char asciiCh = (char)*p;
      tft.setTextColor(color, bg);
      tft.setTextSize(scale);
      tft.drawChar(asciiCh, curX, y, 2);
      curX += (scale == 1 ? 9 : 18);
      p++;
    } else if ((*p & 0xE0) == 0xE0) {
      if (*(p + 1) && *(p + 2)) {
        uint16_t unicode = (((uint16_t)(*p & 0x0F)) << 12) |
                           (((uint16_t)(*(p + 1) & 0x3F)) << 6) |
                           ((uint16_t)(*(p + 2) & 0x3F));
        font_draw_char(curX, y, unicode, color, bg, scale);
        curX += 16 * scale;
        p += 3;
      } else {
        p++;
      }
    } else if ((*p & 0xE0) == 0xC0) {
      p += 2;
    } else {
      p++;
    }
  }
}

int16_t font_get_string_width(const char* utf8_str, uint8_t scale) {
  int16_t w = 0;
  const uint8_t* p = (const uint8_t*)utf8_str;
  while (*p) {
    if (*p < 0x80) {
      w += (scale == 1 ? 9 : 18);
      p++;
    } else if ((*p & 0xE0) == 0xE0) {
      w += 16 * scale;
      p += 3;
    } else {
      p++;
    }
  }
  return w;
}

void font_draw_string_center(int16_t cx, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale) {
  int16_t w = font_get_string_width(utf8_str, scale);
  font_draw_string(cx - w / 2, y, utf8_str, color, bg, scale);
}
""")
