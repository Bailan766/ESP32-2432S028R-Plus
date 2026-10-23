#pragma once
#include <Arduino.h>
#include "config.h"

void battery_init();
void battery_update();
float battery_get_voltage();
uint8_t battery_get_percent();
int battery_get_raw_adc();
bool battery_is_low();

// 电源模式与光敏传感扩展支持 (适配 CYD ESP32-2432S028R-Plus 硬件特性)
bool battery_is_usb_powered();
void battery_set_usb_mode(bool isUsb);
int battery_get_light_percent(); // 获取 IO 34 板载光敏电阻照度百分比 (0~100%)
