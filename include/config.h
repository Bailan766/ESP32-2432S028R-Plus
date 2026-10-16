// ==============================================================================
// Bailan MP3 Player - 硬件配置与全局定义
// 目标平台: ESP32-2432S028R Plus 一体化开发板 (CYD Plus)
// 架构角色: 嵌入式底层硬件配置单点管理源 (Single Source of Truth)
// ==============================================================================
#pragma once
#include <Arduino.h>

// ------------------------------------------------------------------------------
// 项目与版本信息
// ------------------------------------------------------------------------------
#define PROJECT_NAME        "Bailan MP3 Player"
#define PROJECT_VERSION     "v1.0.0"
#define PROJECT_EDITION     "正式版"
#define PROJECT_AUTHOR      "摆烂233"
#define HARDWARE_REVISION   "ESP32-2432S028R-Plus"
#define FIRMWARE_BUILD_DATE __DATE__ " " __TIME__

// ------------------------------------------------------------------------------
// 调试日志系统 (统一日志等级与前缀)
// ------------------------------------------------------------------------------
#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_DEBUG   3
#define LOG_LEVEL_VERBOSE 4

#ifndef CURRENT_LOG_LEVEL
#define CURRENT_LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOG_TAG(tag, fmt, ...) do { \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO) { \
        Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

#define LOG_BOOT(fmt, ...)    LOG_TAG("BOOT", fmt, ##__VA_ARGS__)
#define LOG_DISPLAY(fmt, ...) LOG_TAG("DISPLAY", fmt, ##__VA_ARGS__)
#define LOG_TOUCH(fmt, ...)   LOG_TAG("TOUCH", fmt, ##__VA_ARGS__)
#define LOG_AUDIO(fmt, ...)   LOG_TAG("AUDIO", fmt, ##__VA_ARGS__)
#define LOG_AMP(fmt, ...)     LOG_TAG("AMP", fmt, ##__VA_ARGS__)
#define LOG_BATTERY(fmt, ...) LOG_TAG("BATTERY", fmt, ##__VA_ARGS__)
#define LOG_SD(fmt, ...)      LOG_TAG("SD", fmt, ##__VA_ARGS__)
#define LOG_PLAYER(fmt, ...)  LOG_TAG("PLAYER", fmt, ##__VA_ARGS__)
#define LOG_UI(fmt, ...)      LOG_TAG("UI", fmt, ##__VA_ARGS__)
#define LOG_SYS(fmt, ...)     LOG_TAG("SYS", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   do { \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR) { \
        Serial.printf("[ERROR] (%s:%d) " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } \
} while(0)

// ------------------------------------------------------------------------------
// 硬件引脚分配 (严格以实物标注为准，禁止自行臆测)
// ------------------------------------------------------------------------------

// ---------- 1. LCD 显示屏 (ST7789, 2.8寸, 320x240, VSPI) ----------
#define LCD_SCK_PIN       14
#define LCD_MOSI_PIN      13
#define LCD_MISO_PIN      12   // 未接或悬空，保留定义
#define LCD_CS_PIN        15
#define LCD_DC_PIN        2
#define LCD_RST_PIN       -1   // 硬件复位连接系统复位，无独立引脚
#define LCD_BL_PIN        21   // 背光控制 (支持 PWM 调光)
#define LCD_BL_LEDC_CH    4    // 背光使用的 LEDC 通道
#define LCD_BL_PWM_FREQ   5000 // 调光频率 5kHz，避免人耳啸叫
#define LCD_ROTATION      3    // 3=横屏 320x240 (正向正常握持，USB接口居左)

#define SCREEN_WIDTH      320
#define SCREEN_HEIGHT     240

// ---------- 2. 电阻式触摸屏 (XPT2046, 独立软件SPI) ----------
#define TOUCH_SCK_PIN     25
#define TOUCH_MOSI_PIN    32   // DIN
#define TOUCH_MISO_PIN    39   // DOUT (仅输入)
#define TOUCH_CS_PIN      33
#define TOUCH_IRQ_PIN     36   // 触摸中断 (仅输入)

// 触摸校准范围 (以 CYD Plus 实测 rawX 260~1400, rawY 300~1700 为准)
#define TS_RAW_MIN_X      260
#define TS_RAW_MAX_X      1400 // 关键修复：原 1700 导致顶部点击判定为 y>50，无法点中返回键
#define TS_RAW_MIN_Y      300
#define TS_RAW_MAX_Y      1700
#define TS_PRESSURE_MIN   35   // 过滤误触的最小压力阈值 (Z1)

// ---------- 3. TF / SD 卡 (HSPI 独立总线) ----------
#define SD_SCK_PIN        18
#define SD_MISO_PIN       19
#define SD_MOSI_PIN       23
#define SD_CS_PIN         5
#define SD_SPI_SPEED      4000000  // 4MHz 官方推荐稳定通信速率 (广泛兼容各品牌 MicroSD/TF 卡)

// ---------- 4. 音频与功放 ----------
#define AUDIO_PWM_PIN     26   // 板载标注“音频 PWM”，连接内部 DAC2 或 PWM 输入
#define AUDIO_PWM_CHANNEL 0    // 音频波形使用的 LEDC 通道
#define AUDIO_PWM_RES     8    // 8-bit 分辨率
#define AMP_ENABLE_PIN    4    // 功放使能 (高电平有效)

// ---------- 5. 电池电压检测 (ADC1) ----------
#define BATTERY_ADC_PIN   34   // ADC1_CHANNEL_6 (仅输入引脚)
// 注意：分压比待实物电路图最终确认，目前默认为 2.0 (如两个等值 100k 电阻串联)
#define BATTERY_DIVIDER_RATIO 2.0f 
#define BATTERY_VOLT_MIN      3.30f // 放电截止电压
#define BATTERY_VOLT_MAX      4.20f // 满电电压

// ---------- 6. 板载 RGB LED (低电平点亮) ----------
#define LED_R_PIN         22
#define LED_G_PIN         17
#define LED_B_PIN         16
#define LED_R_LEDC_CH     1
#define LED_G_LEDC_CH     2
#define LED_B_LEDC_CH     3

// ---------- 7. 实体按键 ----------
#define KEY_BOOT_PIN      0    // BOOT 键 (低电平按下，带内部上拉)

// ---------- 8. 预留接口 (Phase 1 暂不使用) ----------
#define GPS_RX_PIN        1    // GPS 模块 TX 接入此引脚，烧录固件时需拔掉防冲突
#define GPS_BAUD_RATE     9600

// ------------------------------------------------------------------------------
// 颜色定义 (RGB565 格式，适配 TFT_eSPI)
// ------------------------------------------------------------------------------
#define COLOR_BG_DARK     0x0842 // 极深灰黑
#define COLOR_PANEL_BG    0x18E3 // 深蓝灰卡片底色
#define COLOR_PANEL_BORDER 0x39E7 // 卡片高光边框
#define COLOR_ACCENT_CYAN 0x07FF // 科技青蓝
#define COLOR_ACCENT_GREEN 0x07E0 // 荧光绿
#define COLOR_ACCENT_ORANGE 0xFD20 // 亮橙色
#define COLOR_TEXT_PRIMARY 0xFFFF // 白色主文本
#define COLOR_TEXT_MUTED  0x8410 // 暗灰次级文本
#define COLOR_DANGER      0xF800 // 警告红
