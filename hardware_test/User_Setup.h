// ============================================================
// TFT_eSPI 用户配置 - ESP32-2432S028R Plus (ST7789)
// 
// 使用方法：
// 1. 在 Arduino IDE 库管理器中安装 "TFT_eSPI" (Bodmer)
// 2. 找到库目录：文档\Arduino\libraries\TFT_eSPI\
// 3. 用这个文件替换里面的 User_Setup.h
// 4. 或者把这个文件改名为 User_Setup.h 放进去
// ============================================================

// ---- 驱动芯片 ----
#define ST7789_DRIVER

// ---- 屏幕尺寸 (竖屏方向的物理尺寸) ----
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---- 颜色顺序 BGR ----
#define TFT_RGB_ORDER TFT_BGR

// ---- 引脚定义 ----
#define TFT_MISO  12
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // 没有复位引脚
#define TFT_BL    21   // 背光控制

// 背光电平：HIGH=亮
#define TFT_BACKLIGHT_ON HIGH

// ---- SPI 频率 ----
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY   6000000
#define SPI_TOUCH_FREQUENCY  2500000

// ---- 字体 ----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT
