#pragma once
#include <Arduino.h>
#include "config.h"

struct BatteryPoint {
  uint16_t mv;        // 毫伏 (3300 ~ 4200)
  uint8_t percent;    // 电量百分比 (0 ~ 100)
  uint8_t isUsb;      // 1=USB供电/充电中, 0=电池放电
  uint32_t timestamp; // 记录时刻 (millis)
};

#define BATTERY_HISTORY_MAX 120

void battery_init();
void battery_update();
float battery_get_voltage();
uint8_t battery_get_percent();
int battery_get_raw_adc();
bool battery_is_low();
bool battery_is_usb_powered();

// 耗电曲线历史数据接口
uint16_t battery_history_get_count();
const BatteryPoint* battery_history_get_points();
float battery_get_discharge_rate_per_hour(); // %/小时，负数代表放电，正数代表充电
float battery_get_estimated_hours_remaining();
void battery_history_clear();
