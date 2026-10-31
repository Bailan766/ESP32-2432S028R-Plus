#pragma once
#include <Arduino.h>

void power_init();
void power_update();
void power_reset_inactivity_timer();
bool power_is_screen_sleeping();
void power_wake_screen();

// 当前 CPU 工作频率（MHz），用于诊断显示
uint8_t power_get_cpu_mhz();
