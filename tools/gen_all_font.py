import os
import time
from PIL import Image, ImageFont, ImageDraw

chars = set()

# 1. GB2312 characters (0xA1..0xF7)
for b1 in range(0xA1, 0xF8):
    for b2 in range(0xA1, 0xFF):
        try:
            b = bytes([b1, b2])
            c = b.decode('gb2312')
            chars.add(c)
        except:
            pass

# 2. UI text
ui_text = """
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
小说阅读书架书签章节进度跳转夜间护眼白昼字体颜色翻页排版目录上一页下一页书籍返回重扫重新扫描当前无编码格式
羊皮纸豆沙绿暗黑模式水墨白昼经典恢复主题切换拖动进度
"""
for c in ui_text:
    if c.strip() and ord(c) >= 128:
        chars.add(c)

sorted_chars = sorted(list(chars), key=lambda x: ord(x))
print(f"Total unique characters to generate: {len(sorted_chars)}")

font_path = "C:/Windows/Fonts/simsun.ttc"
font = ImageFont.truetype(font_path, 14)

t0 = time.time()
glyphs = []
for c in sorted_chars:
    u = ord(c)
    img = Image.new('1', (16, 16), 0)
    draw = ImageDraw.Draw(img)
    draw.text((1, 0), c, font=font, fill=1)
    
    b_arr = []
    for y in range(16):
        b0 = 0
        b1 = 0
        for x in range(8):
            if img.getpixel((x, y)): b0 |= (1 << (7 - x))
        for x in range(8, 16):
            if img.getpixel((x, y)): b1 |= (1 << (15 - x))
        b_arr.append(b0)
        b_arr.append(b1)
    glyphs.append((u, b_arr))

print(f"Bitmap generation took {time.time() - t0:.2f}s")

# Generate GB2312 to Unicode table (0xA1..0xF7 x 0xA1..0xFE)
gb_table = []
for q in range(0xA1, 0xF8):
    for w in range(0xA1, 0xFF):
        try:
            u = ord(bytes([q, w]).decode('gb2312'))
        except:
            u = 0
        gb_table.append(u)

# Write to src/ui/font_cn.h
with open("src/ui/font_cn.h", "w", encoding="utf-8") as f:
    f.write("""#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

void font_cn_init();
const uint8_t* font_find_glyph(uint16_t unicode);
uint16_t font_gb2312_to_unicode(uint8_t b1, uint8_t b2);
void font_draw_char(int16_t x, int16_t y, uint16_t unicode, uint16_t color, uint16_t bg, uint8_t scale = 1);
void font_draw_string(int16_t x, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale = 1);
void font_draw_string_center(int16_t cx, int16_t y, const char* utf8_str, uint16_t color, uint16_t bg, uint8_t scale = 1);
int16_t font_get_string_width(const char* utf8_str, uint8_t scale = 1);
""")

# Write to src/ui/font_cn.cpp
with open("src/ui/font_cn.cpp", "w", encoding="utf-8") as f:
    f.write("""#include "font_cn.h"
#include "hardware/display.h"
#include "config.h"

struct Glyph16 {
  uint16_t unicode;
  uint8_t data[32];
};

static const Glyph16 PROGMEM s_glyphs[] = {
""")
    for u, b_arr in glyphs:
        hex_str = ", ".join(f"0x{b:02X}" for b in b_arr)
        f.write(f"  {{ 0x{u:04X}, {{ {hex_str} }} }},\n")

    f.write("""};

static const int GLYPH_COUNT = sizeof(s_glyphs) / sizeof(Glyph16);

// GB2312 to Unicode lookup table (87 x 94 = 8178 entries)
static const uint16_t PROGMEM s_gb2312_to_unicode[] = {
""")
    # Write 16 entries per line
    for i in range(0, len(gb_table), 16):
        chunk = gb_table[i:i+16]
        f.write("  " + ", ".join(f"0x{val:04X}" for val in chunk) + ",\n")

    f.write("""};

void font_cn_init() {
  LOG_UI("全量中文点阵字库已就绪，收录字数: %d，GB2312映射表已载入", GLYPH_COUNT);
}

uint16_t font_gb2312_to_unicode(uint8_t b1, uint8_t b2) {
  if (b1 >= 0xA1 && b1 <= 0xF7 && b2 >= 0xA1 && b2 <= 0xFE) {
    int idx = (b1 - 0xA1) * 94 + (b2 - 0xA1);
    return pgm_read_word(&(s_gb2312_to_unicode[idx]));
  }
  return 0;
}

const uint8_t* font_find_glyph(uint16_t unicode) {
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
  const uint8_t* glyphData = font_find_glyph(unicode);
  if (!glyphData) {
    if (scale == 1) tft.drawRect(x, y, 16, 16, color);
    else tft.drawRect(x, y, 16 * scale, 16 * scale, color);
    return;
  }

  if (scale == 1) {
    tft.drawBitmap(x, y, glyphData, 16, 16, color, bg);
  } else {
    for (int r = 0; r < 16; r++) {
      uint8_t b0 = pgm_read_byte(glyphData + r * 2);
      uint8_t b1 = pgm_read_byte(glyphData + r * 2 + 1);
      uint16_t row = ((uint16_t)b0 << 8) | b1;
      for (int c = 0; c < 16; c++) {
        bool on = (row >> (15 - c)) & 1;
        uint16_t pixelColor = on ? color : bg;
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

print("Successfully generated font_cn.h and font_cn.cpp with GB2312 mapping!")
