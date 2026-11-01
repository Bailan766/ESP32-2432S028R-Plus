#include "battery.h"

static float s_batteryVoltage = 3.85f; // 初始锂电池标称电压估算
static uint8_t s_batteryPercent = 70;
static int s_rawAdc = 0;
static unsigned long s_lastSampleTime = 0;

// 耗电历史点环形缓冲区
static BatteryPoint s_history[BATTERY_HISTORY_MAX];
static uint16_t s_historyCount = 0;
static unsigned long s_lastHistorySample = 0;

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
  
  // 记录初始数据基准点
  BatteryPoint initialPoint;
  initialPoint.mv = (uint16_t)(s_batteryVoltage * 1000.0f);
  initialPoint.percent = s_batteryPercent;
  initialPoint.isUsb = battery_is_usb_powered() ? 1 : 0;
  initialPoint.timestamp = millis();
  
  s_history[0] = initialPoint;
  s_history[1] = initialPoint;
  s_historyCount = 2;
  s_lastHistorySample = millis();

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

  // 历史数据采样 (每 30 秒记录一次)
  if (millis() - s_lastHistorySample >= 30000 || s_lastHistorySample == 0) {
    s_lastHistorySample = millis();
    BatteryPoint p;
    p.mv = (uint16_t)(s_batteryVoltage * 1000.0f);
    p.percent = s_batteryPercent;
    p.isUsb = battery_is_usb_powered() ? 1 : 0;
    p.timestamp = millis();

    if (s_historyCount < BATTERY_HISTORY_MAX) {
      s_history[s_historyCount++] = p;
    } else {
      // 队列已满，平移淘汰最旧的点
      memmove(&s_history[0], &s_history[1], sizeof(BatteryPoint) * (BATTERY_HISTORY_MAX - 1));
      s_history[BATTERY_HISTORY_MAX - 1] = p;
    }
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

bool battery_is_low() {
  return s_batteryVoltage < (BATTERY_VOLT_MIN + 0.15f); // 低于 3.45V 告警
}

bool battery_is_usb_powered() {
  return (s_batteryVoltage >= 4.05f || s_batteryVoltage < 2.50f);
}

uint16_t battery_history_get_count() {
  return s_historyCount;
}

const BatteryPoint* battery_history_get_points() {
  return s_history;
}

void battery_history_clear() {
  s_historyCount = 0;
  s_lastHistorySample = millis();
  BatteryPoint initialPoint;
  initialPoint.mv = (uint16_t)(s_batteryVoltage * 1000.0f);
  initialPoint.percent = s_batteryPercent;
  initialPoint.isUsb = battery_is_usb_powered() ? 1 : 0;
  initialPoint.timestamp = millis();
  s_history[0] = initialPoint;
  s_historyCount = 1;
}

float battery_get_discharge_rate_per_hour() {
  if (s_historyCount < 2) return 0.0f;
  int startIdx = 0;
  uint32_t latestTime = s_history[s_historyCount - 1].timestamp;
  for (int i = s_historyCount - 2; i >= 0; i--) {
    if (latestTime - s_history[i].timestamp >= 60000) {
      startIdx = i;
      break;
    }
  }
  uint32_t dt_ms = latestTime - s_history[startIdx].timestamp;
  if (dt_ms < 10000) return 0.0f;

  float dp = (float)s_history[s_historyCount - 1].percent - (float)s_history[startIdx].percent;
  float hours = dt_ms / 3600000.0f;
  return dp / hours;
}

float battery_get_estimated_hours_remaining() {
  float rate = battery_get_discharge_rate_per_hour();
  if (rate >= -0.5f) {
    return (s_batteryPercent / 100.0f) * 6.5f;
  }
  float dischargeRate = -rate;
  float remaining = (float)s_batteryPercent / dischargeRate;
  if (remaining > 24.0f) remaining = 24.0f;
  if (remaining < 0.1f) remaining = 0.1f;
  return remaining;
}
