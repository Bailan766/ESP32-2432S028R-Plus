#include "battery.h"

static float s_batteryVoltage = 4.10f; // 初始默认估算值
static uint8_t s_batteryPercent = 90;
static int s_rawAdc = 0;
static unsigned long s_lastSampleTime = 0;

void battery_init() {
  LOG_BATTERY("初始化电池电压采样 (ADC1_CH6, GPIO%d)...", BATTERY_ADC_PIN);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  analogReadResolution(12); // 12-bit ADC (0 ~ 4095)
  battery_update();
  LOG_BATTERY("初次采样完成: ADC=%d, 电压=%.2f V, 估算电量=%d%% (分压比=%.1f)",
              s_rawAdc, s_batteryVoltage, s_batteryPercent, BATTERY_DIVIDER_RATIO);
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

  // 计算电压: ADC * 3.3V / 4095 * 分压比
  // 注意：BATTERY_DIVIDER_RATIO 默认 2.0，若板载电阻为 100k+100k 则正确；
  // 实测 USB 供电时经二极管常在 4.0~4.2V 之间。
  float v = (s_rawAdc * 3.3f / 4095.0f) * BATTERY_DIVIDER_RATIO;
  
  // 一阶低通滤波
  s_batteryVoltage = s_batteryVoltage * 0.7f + v * 0.3f;

  // 估算电量百分比 (3.30V ~ 4.20V 线性估算)
  // TODO: 后续阶段可导入锂电池放电曲线查找表以达到竞赛级精度
  float pct = (s_batteryVoltage - BATTERY_VOLT_MIN) / (BATTERY_VOLT_MAX - BATTERY_VOLT_MIN) * 100.0f;
  s_batteryPercent = (uint8_t)constrain((int)pct, 0, 100);
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
