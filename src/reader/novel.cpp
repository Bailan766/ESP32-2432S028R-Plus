#include "novel.h"
#include "demo_novel.h"
#include "storage/sdcard.h"
#include "hardware/display.h"
#include "hardware/battery.h"
#include "ui/font_cn.h"
#include "config.h"
#include <Preferences.h>
#include <SD.h>

static NovelItem s_books[NOVEL_MAX_ITEMS];
static int s_bookCount = 0;
static int s_currentBookIdx = -1;
static bool s_isOpen = false;

static File s_file;
static uint32_t s_currentOffset = 0;
static uint32_t s_pageEndOffset = 0;
static uint32_t s_fileSize = 0;
static bool s_isGbk = false;

// 历史翻页栈 (记录前 64 页的起始偏移量，支持精准上一页回退)
static uint32_t s_history[NOVEL_HISTORY_DEPTH];
static int s_historyTop = 0;

// 主题配色表 (暗黑水墨、羊皮纸卷、豆沙护眼、深夜星河)
static const ThemeColors s_themes[] = {
  { 0x0000, 0xFFFF, 0x10A2, 0x8410, 0x07E0, "暗黑水墨" },
  { 0xF7BE, 0x2104, 0xEF5B, 0x630C, 0xBA00, "羊皮纸卷" },
  { 0xCE79, 0x0000, 0xBD75, 0x3286, 0x03E0, "豆沙护眼" },
  { 0x0845, 0xCE79, 0x1084, 0x738E, 0x05BF, "深夜星河" }
};
static NovelTheme s_currentTheme = THEME_DARK;

static Preferences s_prefs;

static void loadBookmarksForBook(NovelItem& item) {
  s_prefs.begin("novel_bm", true);
  // 用文件路径简单哈希或截取后8位作为键
  char key[16];
  uint32_t h = 5381;
  for (const char* p = item.path; *p; p++) {
    h = ((h << 5) + h) + *p;
  }
  snprintf(key, sizeof(key), "b_%08X", h);
  item.last_offset = s_prefs.getUInt(key, 0);
  s_prefs.end();
}

static void saveBookmarkForCurrent() {
  if (s_currentBookIdx < 0 || s_currentBookIdx >= s_bookCount) return;
  const NovelItem& item = s_books[s_currentBookIdx];
  s_prefs.begin("novel_bm", false);
  char key[16];
  uint32_t h = 5381;
  for (const char* p = item.path; *p; p++) {
    h = ((h << 5) + h) + *p;
  }
  snprintf(key, sizeof(key), "b_%08X", h);
  s_prefs.putUInt(key, s_currentOffset);
  s_prefs.end();
  s_books[s_currentBookIdx].last_offset = s_currentOffset;
}

static void scanDir(File dir, int depth) {
  if (depth > 2 || s_bookCount >= NOVEL_MAX_ITEMS) return;

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    String name = entry.name();
    if (name.startsWith(".") || name.startsWith("/.")) {
      entry.close();
      continue;
    }

    if (entry.isDirectory()) {
      scanDir(entry, depth + 1);
    } else {
      String lower = name;
      lower.toLowerCase();
      if (lower.endsWith(".txt")) {
        int slash = name.lastIndexOf('/');
        String base = (slash >= 0) ? name.substring(slash + 1) : name;
        int dot = base.lastIndexOf('.');
        String title = (dot >= 0) ? base.substring(0, dot) : base;

        NovelItem& item = s_books[s_bookCount];
        strncpy(item.title, title.c_str(), sizeof(item.title) - 1);
        item.title[sizeof(item.title) - 1] = '\0';
        strncpy(item.path, entry.path(), sizeof(item.path) - 1);
        item.path[sizeof(item.path) - 1] = '\0';
        item.file_size = entry.size();
        item.is_demo = false;
        loadBookmarksForBook(item);

        s_bookCount++;
        LOG_SD("发现小说 TXT: %s (%u 字节)", entry.path(), entry.size());
        if (s_bookCount >= NOVEL_MAX_ITEMS) {
          entry.close();
          break;
        }
      }
    }
    entry.close();
  }
}

void novel_init() {
  s_prefs.begin("novel_cfg", true);
  uint8_t th = s_prefs.getUChar("theme", 0);
  if (th <= 3) s_currentTheme = (NovelTheme)th;
  s_prefs.end();

  novel_scan_all();
}

void novel_scan_all() {
  s_bookCount = 0;
  s_currentBookIdx = -1;

  if (sdcard_is_available()) {
    File novelDir = SD.open("/novel");
    if (novelDir && novelDir.isDirectory()) {
      scanDir(novelDir, 0);
      novelDir.close();
    }
    File novelsDir = SD.open("/novels");
    if (novelsDir && novelsDir.isDirectory()) {
      scanDir(novelsDir, 0);
      novelsDir.close();
    }
    // 扫描根目录
    File rootDir = SD.open("/");
    if (rootDir) {
      scanDir(rootDir, 0);
      rootDir.close();
    }
  }

  // 如果没有扫描到任何小说，自动注入内置演示小说
  if (s_bookCount == 0) {
    NovelItem& item = s_books[s_bookCount];
    strncpy(item.title, DEMO_NOVEL_TITLE, sizeof(item.title) - 1);
    strcpy(item.path, "ROM://demo_novel");
    item.file_size = strlen_P(DEMO_NOVEL_TEXT);
    item.is_demo = true;
    loadBookmarksForBook(item);
    s_bookCount = 1;
    LOG_SD("已注入内置演示小说: 《%s》", DEMO_NOVEL_TITLE);
  }

  LOG_SD("小说书架就绪，共计 %d 本书籍", s_bookCount);
}

int novel_get_count() {
  return s_bookCount;
}

const NovelItem* novel_get_item(int index) {
  if (index >= 0 && index < s_bookCount) {
    return &s_books[index];
  }
  return nullptr;
}

int novel_get_current_index() {
  return s_currentBookIdx;
}

const NovelItem* novel_get_current_book() {
  return novel_get_item(s_currentBookIdx);
}

bool novel_is_open() {
  return s_isOpen;
}

// 自动探测文本编码是否为 GBK / GB2312
static bool detectIfGbk(const uint8_t* buf, int len) {
  if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
    return false; // UTF-8 带 BOM
  }
  int utf8Score = 0;
  int gbkScore = 0;
  for (int i = 0; i < len - 1; i++) {
    if ((buf[i] & 0xE0) == 0xE0 && (buf[i+1] & 0xC0) == 0x80) {
      if (i + 2 < len && (buf[i+2] & 0xC0) == 0x80) {
        utf8Score += 3;
        i += 2;
        continue;
      }
    }
    if (buf[i] >= 0xA1 && buf[i] <= 0xF7 && buf[i+1] >= 0xA1 && buf[i+1] <= 0xFE) {
      gbkScore += 2;
      i++;
    }
  }
  return (gbkScore > utf8Score);
}

bool novel_open(int index) {
  if (index < 0 || index >= s_bookCount) return false;

  novel_close();
  s_currentBookIdx = index;
  NovelItem& book = s_books[index];

  s_historyTop = 0;
  s_currentOffset = book.last_offset;
  s_fileSize = book.file_size;

  if (book.is_demo) {
    s_isGbk = false;
    s_isOpen = true;
    if (s_currentOffset >= s_fileSize) s_currentOffset = 0;
    return true;
  }

  if (!sdcard_is_available()) return false;

  s_file = SD.open(book.path, "r");
  if (!s_file) {
    LOG_SD("打开小说文件失败: %s", book.path);
    return false;
  }

  // 检测前 512 字节编码
  uint8_t probeBuf[512];
  int n = s_file.read(probeBuf, sizeof(probeBuf));
  s_isGbk = detectIfGbk(probeBuf, n);
  LOG_SD("小说编码探测: %s", s_isGbk ? "GBK/GB2312" : "UTF-8");

  // 如果是 UTF-8 带 BOM，偏移避开前 3 字节
  if (!s_isGbk && n >= 3 && probeBuf[0] == 0xEF && probeBuf[1] == 0xBB && probeBuf[2] == 0xBF) {
    if (s_currentOffset < 3) s_currentOffset = 3;
  }

  if (s_currentOffset >= s_fileSize) {
    s_currentOffset = (!s_isGbk && n >= 3 && probeBuf[0] == 0xEF) ? 3 : 0;
  }

  s_isOpen = true;
  return true;
}

void novel_close() {
  if (s_isOpen) {
    saveBookmarkForCurrent();
    if (s_file) {
      s_file.close();
    }
    s_isOpen = false;
  }
}

void novel_save_bookmark() {
  saveBookmarkForCurrent();
}

NovelTheme novel_get_theme() {
  return s_currentTheme;
}

void novel_set_theme(NovelTheme theme) {
  s_currentTheme = theme;
  s_prefs.begin("novel_cfg", false);
  s_prefs.putUChar("theme", (uint8_t)theme);
  s_prefs.end();
}

void novel_next_theme() {
  uint8_t th = ((uint8_t)s_currentTheme + 1) % 4;
  novel_set_theme((NovelTheme)th);
}

const ThemeColors& novel_get_theme_colors() {
  return s_themes[(int)s_currentTheme];
}

float novel_get_progress_percent() {
  if (s_fileSize == 0) return 0.0f;
  float p = ((float)s_currentOffset / (float)s_fileSize) * 100.0f;
  return constrain(p, 0.0f, 100.0f);
}

uint32_t novel_get_current_offset() {
  return s_currentOffset;
}

bool novel_next_page() {
  if (!s_isOpen) return false;
  if (s_pageEndOffset >= s_fileSize) return false; // 已到底

  // 保存当前页起点入栈
  if (s_historyTop < NOVEL_HISTORY_DEPTH - 1) {
    s_history[s_historyTop++] = s_currentOffset;
  } else {
    // 栈满左移一位
    for (int i = 0; i < NOVEL_HISTORY_DEPTH - 1; i++) {
      s_history[i] = s_history[i + 1];
    }
    s_history[NOVEL_HISTORY_DEPTH - 1] = s_currentOffset;
  }

  s_currentOffset = s_pageEndOffset;
  saveBookmarkForCurrent();
  novel_render_current_page();
  return true;
}

bool novel_prev_page() {
  if (!s_isOpen) return false;

  if (s_historyTop > 0) {
    s_currentOffset = s_history[--s_historyTop];
    saveBookmarkForCurrent();
    novel_render_current_page();
    return true;
  }

  // 栈为空但不在开头，尝试回退一段距离
  if (s_currentOffset > 0) {
    uint32_t step = (s_currentOffset > 380) ? 380 : s_currentOffset;
    s_currentOffset -= step;
    saveBookmarkForCurrent();
    novel_render_current_page();
    return true;
  }

  return false;
}

void novel_seek_percent(float percent) {
  if (!s_isOpen || s_fileSize == 0) return;
  percent = constrain(percent, 0.0f, 100.0f);
  uint32_t target = (uint32_t)(((float)s_fileSize * percent) / 100.0f);

  s_historyTop = 0; // 跳转后清空历史回退栈
  s_currentOffset = target;
  saveBookmarkForCurrent();
  novel_render_current_page();
}

// 页面排版与极速渲染核心算法
void novel_render_current_page() {
  if (!s_isOpen) return;

  TFT_eSPI& tft = display_get_tft();
  const ThemeColors& tc = novel_get_theme_colors();

  // 清空整屏为当前主题底色
  tft.fillScreen(tc.bg);

  // 1. 顶部状态栏 (高度 22px)
  tft.fillRect(0, 0, 320, 22, tc.status_bg);
  tft.drawLine(0, 22, 320, 22, tc.status_text);

  // 顶部左侧：书名 (截断显示)
  const NovelItem* curBook = novel_get_current_book();
  if (curBook) {
    char titleBuf[24];
    strncpy(titleBuf, curBook->title, sizeof(titleBuf) - 1);
    titleBuf[sizeof(titleBuf) - 1] = '\0';
    font_draw_string(8, 3, titleBuf, tc.text, tc.status_bg, 1);
  }

  // 顶部右侧：阅读进度百分比与电池电压
  char progBuf[32];
  snprintf(progBuf, sizeof(progBuf), "%.1f%%  %.2fV", novel_get_progress_percent(), battery_get_voltage());
  int16_t pw = font_get_string_width(progBuf, 1);
  font_draw_string(312 - pw, 3, progBuf, tc.accent, tc.status_bg, 1);

  // 2. 底部提示栏 (高度 18px)
  tft.fillRect(0, 222, 320, 18, tc.status_bg);
  tft.drawLine(0, 222, 320, 222, tc.status_text);
  font_draw_string(10, 224, "< 上页", tc.status_text, tc.status_bg, 1);
  font_draw_string_center(160, 224, "轻触中央唤出菜单", tc.status_text, tc.status_bg, 1);
  font_draw_string(270, 224, "下页 >", tc.status_text, tc.status_bg, 1);

  // 3. 正文排版参数
  const int16_t MARGIN_LEFT = 12;
  const int16_t MARGIN_RIGHT = 308; // 320 - 12
  const int16_t START_Y = 27;
  const int16_t LINE_HEIGHT = 19;
  const int MAX_LINES = 10;

  int16_t curX = MARGIN_LEFT;
  int16_t curY = START_Y;
  int currentLine = 0;
  bool isNewParagraph = true;

  // 准备分块读取缓冲区
  uint8_t readBuf[1024];
  int bufLen = 0;

  if (curBook->is_demo) {
    int totalLen = strlen_P(DEMO_NOVEL_TEXT);
    int readLen = totalLen - s_currentOffset;
    if (readLen > (int)sizeof(readBuf)) readLen = sizeof(readBuf);
    if (readLen > 0) {
      memcpy_P(readBuf, DEMO_NOVEL_TEXT + s_currentOffset, readLen);
      bufLen = readLen;
    }
  } else {
    if (s_file) {
      s_file.seek(s_currentOffset);
      bufLen = s_file.read(readBuf, sizeof(readBuf));
    }
  }

  if (bufLen <= 0) {
    font_draw_string_center(160, 110, "— 全书完 —", tc.status_text, tc.bg, 1);
    s_pageEndOffset = s_fileSize;
    return;
  }

  int ptr = 0;
  while (ptr < bufLen && currentLine < MAX_LINES) {
    uint8_t b = readBuf[ptr];

    // 处理回车换行
    if (b == '\r') {
      ptr++;
      continue;
    }
    if (b == '\n') {
      ptr++;
      curX = MARGIN_LEFT;
      curY += LINE_HEIGHT;
      currentLine++;
      isNewParagraph = true;
      continue;
    }

    // 新段落首行自动缩进 2 个全角汉字 (32 像素)
    if (isNewParagraph) {
      curX = MARGIN_LEFT + 32;
      isNewParagraph = false;
    }

    uint16_t unicode = 0;
    int charBytes = 1;
    int charWidth = 9; // 默认 ASCII 字符宽度

    if (s_isGbk) {
      // GBK / GB2312 解码
      if (b < 0x80) {
        unicode = b;
        charBytes = 1;
        charWidth = 9;
      } else if (ptr + 1 < bufLen) {
        unicode = font_gb2312_to_unicode(b, readBuf[ptr + 1]);
        charBytes = 2;
        charWidth = 16;
      } else {
        break; // 缓冲区边界截断
      }
    } else {
      // UTF-8 解码
      if (b < 0x80) {
        unicode = b;
        charBytes = 1;
        charWidth = 9;
      } else if ((b & 0xE0) == 0xE0) {
        if (ptr + 2 < bufLen) {
          unicode = (((uint16_t)(b & 0x0F)) << 12) |
                    (((uint16_t)(readBuf[ptr + 1] & 0x3F)) << 6) |
                    ((uint16_t)(readBuf[ptr + 2] & 0x3F));
          charBytes = 3;
          charWidth = 16;
        } else {
          break; // 边界截断
        }
      } else if ((b & 0xE0) == 0xC0) {
        if (ptr + 1 < bufLen) {
          unicode = (((uint16_t)(b & 0x1F)) << 6) | ((uint16_t)(readBuf[ptr + 1] & 0x3F));
          charBytes = 2;
          charWidth = 9;
        } else {
          break;
        }
      } else {
        ptr++;
        continue;
      }
    }

    // 检查行宽溢出与折行
    if (curX + charWidth > MARGIN_RIGHT) {
      curX = MARGIN_LEFT;
      curY += LINE_HEIGHT;
      currentLine++;
      if (currentLine >= MAX_LINES) {
        break; // 本页已排满
      }
    }

    // 绘制单个字符
    if (unicode < 0x80) {
      tft.setTextColor(tc.text, tc.bg);
      tft.setTextSize(1);
      tft.drawChar((char)unicode, curX, curY + 2, 2);
    } else {
      font_draw_char(curX, curY, unicode, tc.text, tc.bg, 1);
    }

    curX += charWidth;
    ptr += charBytes;
  }

  // 记录本页结束位置，作为下一次翻页的起始位置
  s_pageEndOffset = s_currentOffset + ptr;
}
