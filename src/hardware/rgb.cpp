#include "rgb.h"

static RGBState s_rgbState = RGB_OFF;
static uint8_t s_r = 0, s_g = 0, s_b = 0;

void rgb_init() {
  LOG_SYS("初始化板载 RGB LED (R=%d, G=%d, B=%d, 低电平点亮)...",
          LED_R_PIN, LED_G_PIN, LED_B_PIN);
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);

  // 默认置高电平熄灭
  digitalWrite(LED_R_PIN, HIGH);
  digitalWrite(LED_G_PIN, HIGH);
  digitalWrite(LED_B_PIN, HIGH);

  // 配置 LEDC PWM 通道
  ledcSetup(LED_R_LEDC_CH, 5000, 8); ledcAttachPin(LED_R_PIN, LED_R_LEDC_CH);
  ledcSetup(LED_G_LEDC_CH, 5000, 8); ledcAttachPin(LED_G_PIN, LED_G_LEDC_CH);
  ledcSetup(LED_B_LEDC_CH, 5000, 8); ledcAttachPin(LED_B_PIN, LED_B_LEDC_CH);

  rgb_set_color(0, 0, 0);
  s_rgbState = RGB_OFF;
}

void rgb_set_color(uint8_t r, uint8_t g, uint8_t b) {
  s_r = r; s_g = g; s_b = b;
  // 低电平点亮，PWM 取反 (255 - val)
  ledcWrite(LED_R_LEDC_CH, 255 - r);
  ledcWrite(LED_G_LEDC_CH, 255 - g);
  ledcWrite(LED_B_LEDC_CH, 255 - b);
}

void rgb_set_state(RGBState state) {
  s_rgbState = state;
  switch (state) {
    case RGB_OFF:
      rgb_set_color(0, 0, 0);
      break;
    case RGB_BOOT:
      rgb_set_color(0, 180, 220); // 开机青蓝
      break;
    case RGB_ERROR:
      rgb_set_color(255, 0, 0);   // 错误红色
      break;
    default:
      break;
  }
}

static uint8_t s_targetR = 0, s_targetG = 0, s_targetB = 0;
static unsigned long s_lastRgbDecay = 0;

void rgb_beat_pulse(uint8_t energy, uint16_t freq) {
  if (s_rgbState != RGB_PLAYING && s_rgbState != RGB_BOOT) return;
  if (energy < 15) return;

  uint8_t baseR = 0, baseG = 0, baseB = 0;
  if (freq == 0) {
    static uint16_t hue = 0;
    hue = (hue + 15) % 360;
    if (hue < 120) {
      baseR = 255 - hue * 2;
      baseG = hue * 2;
      baseB = 40;
    } else if (hue < 240) {
      baseR = 20;
      baseG = 255 - (hue - 120) * 2;
      baseB = (hue - 120) * 2;
    } else {
      baseR = (hue - 240) * 2;
      baseG = 20;
      baseB = 255 - (hue - 240) * 2;
    }
  } else if (freq < 250) {
    baseR = 255; baseG = 40; baseB = 0;    // 低频暖橙红 (重低音轰炸)
  } else if (freq < 600) {
    baseR = 240; baseG = 180; baseB = 10;  // 中低频金黄 (人声/节奏)
  } else if (freq < 1200) {
    baseR = 10; baseG = 240; baseB = 180;  // 中高频翡翠青蓝
  } else {
    baseR = 200; baseG = 10; baseB = 255;  // 极高频炫彩紫电
  }

  uint8_t r = (uint8_t)(((uint16_t)baseR * energy) / 255);
  uint8_t g = (uint8_t)(((uint16_t)baseG * energy) / 255);
  uint8_t b = (uint8_t)(((uint16_t)baseB * energy) / 255);

  if (r > s_targetR) s_targetR = r;
  if (g > s_targetG) s_targetG = g;
  if (b > s_targetB) s_targetB = b;
  rgb_set_color(s_targetR, s_targetG, s_targetB);
}

void rgb_pulse_pitch(uint16_t freq) {
  rgb_beat_pulse(200, freq);
}

void rgb_update() {
  if (s_rgbState == RGB_PLAYING) {
    if (millis() - s_lastRgbDecay >= 25) {
      s_lastRgbDecay = millis();
      s_targetR = (uint8_t)((s_targetR * 82) / 100);
      s_targetG = (uint8_t)((s_targetG * 82) / 100);
      s_targetB = (uint8_t)((s_targetB * 82) / 100);
      rgb_set_color(s_targetR, s_targetG, s_targetB);
    }
  } else if (s_rgbState == RGB_PAUSED) {
    // 暂停状态：慢速呼吸蓝灯
    float breath = (sin(millis() / 450.0f) + 1.0f) * 0.5f; // 0.0 ~ 1.0
    uint8_t b = (uint8_t)(breath * 130.0f);
    uint8_t g = (uint8_t)(breath * 40.0f);
    rgb_set_color(0, g, b);
  } else if (s_rgbState == RGB_LOW_BATTERY) {
    // 低电量状态：红灯慢闪
    bool flash = ((millis() / 500) % 2) == 0;
    rgb_set_color(flash ? 200 : 0, flash ? 30 : 0, 0);
  }
}

RGBState rgb_get_state() {
  return s_rgbState;
}
