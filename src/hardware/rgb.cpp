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

void rgb_pulse_pitch(uint16_t freq) {
  if (s_rgbState != RGB_PLAYING) return;
  if (freq == 0) {
    rgb_set_color(0, 0, 0);
    return;
  }

  // 随音符频率变幻色彩
  if (freq < 350) {
    rgb_set_color(220, 40, 0);   // 低频暖橙红
  } else if (freq < 650) {
    rgb_set_color(0, 200, 80);   // 中频活力绿
  } else if (freq < 1000) {
    rgb_set_color(0, 180, 255);  // 高频科技蓝
  } else {
    rgb_set_color(220, 0, 220);  // 极高频炫彩紫
  }
}

void rgb_update() {
  if (s_rgbState == RGB_PAUSED) {
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
