#pragma once
#include <Arduino.h>
#include "config.h"

void battery_init();
void battery_update();
float battery_get_voltage();
uint8_t battery_get_percent();
int battery_get_raw_adc();
bool battery_is_low();
bool battery_is_usb_powered();
