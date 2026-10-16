// ============================================================
// ESP32-2432S028R Plus 硬件引脚配置
// ============================================================
#pragma once

// ---------- LCD (ST7789, HSPI) ----------
#define TFT_SCK       14
#define TFT_MOSI      13
#define TFT_MISO      12
#define TFT_CS        15
#define TFT_DC         2
#define TFT_BL        21
#define TFT_RST       -1   // 无复位引脚

#define TFT_WIDTH    320
#define TFT_HEIGHT   240
#define TFT_ROTATION   1   // 横屏

// ---------- 触摸 (XPT2046, 软件SPI) ----------
#define TOUCH_MISO    39   // DOUT
#define TOUCH_SCK     25
#define TOUCH_MOSI    32   // DIN
#define TOUCH_CS      33
#define TOUCH_IRQ     36

// 触摸校准值 (根据实际屏幕校准调整)
#define TS_MINX      300
#define TS_MAXX     3900
#define TS_MINY      400
#define TS_MAXY     3800

// ---------- SD 卡 (VSPI) ----------
#define SD_SCK        18
#define SD_MISO       19
#define SD_MOSI       23
#define SD_CS          5

// ---------- GPS (UART2) ----------
#define GPS_RX         1   // GPIO1 (与USB TX共用，注意冲突)
#define GPS_BAUD    9600

// ---------- 音频 ----------
#define AUDIO_PWM     26
#define AUDIO_EN       4   // 高电平有效

// ---------- 电池检测 ----------
#define BATT_ADC      34
#define BATT_DIV_RATIO 2.0  // 分压比 (根据实际电路调整)

// ---------- RGB 灯 (低电平点亮) ----------
#define LED_R         22
#define LED_G         17
#define LED_B         16

// ---------- 按键 ----------
#define KEY_BOOT       0   // 低电平有效
