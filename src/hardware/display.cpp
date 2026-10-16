#include "display.h"

static TFT_eSPI tft = TFT_eSPI();
static uint8_t s_currentBrightness = 80; // 默认亮度 80%
static uint8_t s_currentRotation = LCD_ROTATION;
static bool s_isSleeping = false;

TFT_eSPI& display_get_tft() {
  return tft;
}

void display_init() {
  LOG_DISPLAY("初始化 ST7789 LCD (320x240, 横屏模式)...");

  // 初始化 LCD 驱动
  tft.init();
  tft.setRotation(s_currentRotation);
  tft.invertDisplay(false); // 修复 ST7789 色彩反转问题
  tft.fillScreen(COLOR_BG_DARK);

  // 配置背光 PWM 控制 (GPIO21, LEDC 通道 4)
  ledcSetup(LCD_BL_LEDC_CH, LCD_BL_PWM_FREQ, 8);
  ledcAttachPin(LCD_BL_PIN, LCD_BL_LEDC_CH);
  display_set_brightness(s_currentBrightness);

  LOG_DISPLAY("LCD 初始化成功, 当前亮度: %d%%, 屏幕方向: %d", s_currentBrightness, s_currentRotation);
}

void display_set_rotation(uint8_t rotation) {
  s_currentRotation = rotation;
  tft.setRotation(s_currentRotation);
  LOG_DISPLAY("屏幕方向切换为: %d", s_currentRotation);
}

uint8_t display_get_rotation() {
  return s_currentRotation;
}

void display_set_brightness(uint8_t percent) {
  s_currentBrightness = constrain(percent, 0, 100);
  if (!s_isSleeping) {
    // 映射到 0~255 占空比
    uint32_t duty = (s_currentBrightness * 255) / 100;
    ledcWrite(LCD_BL_LEDC_CH, duty);
  }
}

uint8_t display_get_brightness() {
  return s_currentBrightness;
}

void display_sleep() {
  if (s_isSleeping) return;
  s_isSleeping = true;
  ledcWrite(LCD_BL_LEDC_CH, 0); // 关背光
  LOG_DISPLAY("屏幕进入息屏休眠状态 (背光关闭, 音频继续后台运行)");
}

void display_wakeup() {
  if (!s_isSleeping) return;
  s_isSleeping = false;
  display_set_brightness(s_currentBrightness);
  LOG_DISPLAY("屏幕被唤醒, 恢复背光: %d%%", s_currentBrightness);
}

bool display_is_sleeping() {
  return s_isSleeping;
}
