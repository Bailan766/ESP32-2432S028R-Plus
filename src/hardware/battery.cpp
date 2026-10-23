#include "battery.h"

// 默认 USB 供电模式 (CYD 开发板标准供电方式为 Type-C / 5V)
static bool s_isUsbPowered = true;
static float s_batteryVoltage = 5.00f;
static uint8_t s_batteryPercent = 100;
static int s_rawAdc = 0;
static int s_lightPercent = 50;
static unsigned long s_lastSampleTime = 0;

// 锂电池典型放电非线性映射函数 (精确到 5%)
static uint8_t calcLipoPercent(float v) {
  if (v >= 4.18f) return 100;
  if (v >= 4.08f) return 90;
  if (v >= 3.98f) return 80;
  if (v >= 3.88f) return 70;
  if (v >= 3.80f) return 60;
  if (v >= 3.74f) return 50;
  if (v >= 3.68f) return 40;
  if (v >= 3.62f) return 25;
  if (v >= 3.55f) return 15;
  if (v >= 3.45f) return 5;
  return 0;
}

void battery_init() {
  LOG_BATTERY("初始化电源与传感器采样 (ADC1_CH6, GPIO%d)...", BATTERY_ADC_PIN);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  analogReadResolution(12); // 12-bit ADC (0 ~ 4095)
  battery_update();
  LOG_BATTERY("电源监测初始化完成: 模式=[%s], ADC=%d, 照度=%d%%",
              s_isUsbPowered ? "USB 5V稳定供电" : "外部电池模式", s_rawAdc, s_lightPercent);
}

void battery_update() {
  // 采样间隔 1000ms，避免频繁采样消耗 CPU
  if (millis() - s_lastSampleTime < 1000 && s_lastSampleTime != 0) {
    return;
  }
  s_lastSampleTime = millis();

  // 4 次采样平滑去噪
  long sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += analogRead(BATTERY_ADC_PIN);
    delayMicroseconds(20);
  }
  s_rawAdc = sum / 4;

  // IO 34 在 CYD 板载上为光敏电阻 (CDS)
  // 光照强时电阻变小，分压升高 (0~4095) -> 转换为 0~100% 照度
  s_lightPercent = (int)constrain((s_rawAdc * 100L) / 4095, 0, 100);

  if (s_isUsbPowered) {
    s_batteryVoltage = 5.00f;
    s_batteryPercent = 100;
  } else {
    // 若用户外接了锂电池分压采样电路：
    // 使用 ESP32 原厂 eFuse 校准函数 analogReadMilliVolts
    uint32_t pinMv = analogReadMilliVolts(BATTERY_ADC_PIN);
    float v = (pinMv / 1000.0f) * BATTERY_DIVIDER_RATIO;
    s_batteryVoltage = s_batteryVoltage * 0.7f + v * 0.3f;
    s_batteryPercent = calcLipoPercent(s_batteryVoltage);
  }
}

float battery_get_voltage() {
  return s_batteryVoltage;
}

uint8_t battery_get_percent() {
  return s_batteryPercent;
}

int battery_get_raw_adc() {
  return s_rawAdc;
}

int battery_get_light_percent() {
  return s_lightPercent;
}

bool battery_is_low() {
  if (s_isUsbPowered) return false;
  return s_batteryVoltage < (BATTERY_VOLT_MIN + 0.15f);
}

bool battery_is_usb_powered() {
  return s_isUsbPowered;
}

void battery_set_usb_mode(bool isUsb) {
  s_isUsbPowered = isUsb;
  battery_update();
}
