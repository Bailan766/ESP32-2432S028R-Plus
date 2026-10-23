#include "battery.h"

static float s_batteryVoltage = 3.85f; // 初始锂电池标称电压估算
static uint8_t s_batteryPercent = 70;
static int s_rawAdc = 0;
static unsigned long s_lastSampleTime = 0;

// 锂电池典型放电非线性分段线性插值 (基于 3.7V/4.2V 单节锂电池放电特性曲线)
static uint8_t calcLipoPercent(float v) {
  struct Point { float volt; uint8_t pct; };
  static const Point table[] = {
    { 4.18f, 100 },
    { 4.08f,  90 },
    { 3.98f,  80 },
    { 3.88f,  70 },
    { 3.80f,  60 },
    { 3.74f,  50 },
    { 3.68f,  40 },
    { 3.62f,  30 },
    { 3.56f,  20 },
    { 3.50f,  10 },
    { 3.42f,   5 },
    { 3.30f,   0 }
  };
  if (v >= table[0].volt) return 100;
  if (v <= table[11].volt) return 0;
  for (int i = 0; i < 11; i++) {
    if (v >= table[i + 1].volt) {
      float span = table[i].volt - table[i + 1].volt;
      float t = (v - table[i + 1].volt) / span;
      return (uint8_t)(table[i + 1].pct + t * (table[i].pct - table[i + 1].pct));
    }
  }
  return 0;
}

void battery_init() {
  LOG_BATTERY("初始化电池电压采样 (ADC1_CH6, GPIO%d, 分压比=%.1f)...", BATTERY_ADC_PIN, BATTERY_DIVIDER_RATIO);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  analogReadResolution(12); // 12-bit ADC (0 ~ 4095)
  
  // 初次连续多重采样稳定基准
  long sumMv = 0;
  long sumRaw = 0;
  for (int i = 0; i < 16; i++) {
    sumMv += analogReadMilliVolts(BATTERY_ADC_PIN);
    sumRaw += analogRead(BATTERY_ADC_PIN);
    delay(2);
  }
  s_rawAdc = sumRaw / 16;
  uint32_t avgMv = sumMv / 16;
  float pinVolt = avgMv / 1000.0f;
  s_batteryVoltage = pinVolt * BATTERY_DIVIDER_RATIO;
  s_batteryPercent = calcLipoPercent(s_batteryVoltage);
  
  LOG_BATTERY("电池监测初始化完成: ADC=%d, 引脚=%umV, 电池电压=%.2fV, 电量=%d%%",
              s_rawAdc, avgMv, s_batteryVoltage, s_batteryPercent);
}

void battery_update() {
  // 采样间隔 1000ms
  if (millis() - s_lastSampleTime < 1000 && s_lastSampleTime != 0) {
    return;
  }
  s_lastSampleTime = millis();

  // 8 次采样平滑去噪
  long sumMv = 0;
  long sumRaw = 0;
  for (int i = 0; i < 8; i++) {
    sumMv += analogReadMilliVolts(BATTERY_ADC_PIN);
    sumRaw += analogRead(BATTERY_ADC_PIN);
    delayMicroseconds(50);
  }
  s_rawAdc = sumRaw / 8;
  uint32_t avgMv = sumMv / 8;

  // 使用 ESP32 原厂 eFuse 校准转换得到引脚实际毫伏，再乘分压比
  float pinVolt = avgMv / 1000.0f;
  float v = pinVolt * BATTERY_DIVIDER_RATIO;

  // 采用一阶低通滤波平滑电平波动 (0.85 旧值 + 0.15 新值)
  s_batteryVoltage = s_batteryVoltage * 0.85f + v * 0.15f;
  s_batteryPercent = calcLipoPercent(s_batteryVoltage);
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

bool battery_is_low() {
  return s_batteryVoltage < (BATTERY_VOLT_MIN + 0.15f); // 低于 3.45V 告警
}

bool battery_is_usb_powered() {
  // 处于 USB 供电或充电状态时，单节锂电池端电压通常达到 4.15V 以上
  return s_batteryVoltage >= 4.15f;
}
