// ============================================================
// TFT_eSPI 用户配置 - ESP32-2432S028R Plus (ST7789)
// ============================================================

// 选择驱动芯片
#define ST7789_DRIVER

// 屏幕尺寸
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// 颜色顺序 BGR
#define TFT_RGB_ORDER TFT_BGR

// ST7789 需要开启反色才能正确显示颜色
// 实测：不开启反色时绿色显示为品红色（0x07E0 取反 = 0xF81F）
#define TFT_INVERSION_ON

// ---------- 引脚定义 ----------
#define TFT_MISO  12
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1   // 无复位引脚
#define TFT_BL    21   // 背光控制

// 背光电平
#define TFT_BACKLIGHT_ON HIGH

// ---------- SPI 配置 ----------
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY   6000000
#define SPI_TOUCH_FREQUENCY  2500000

// 使用 VSPI (FSPI 是默认)
#define USE_HSPI_PORT
// VSPI = CS=15, SCK=14, MOSI=13, MISO=12 → 这正是我们的引脚
// ESP32 的 VSPI 默认引脚就是 SCK=18, MOSI=23, MISO=19
// 但我们用的是自定义引脚，所以需要设置
// 实际上 TFT_eSPI 会用上面定义的引脚

// 字体
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT
