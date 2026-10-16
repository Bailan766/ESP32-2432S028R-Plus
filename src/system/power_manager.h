#pragma once
#include <Arduino.h>

void power_init();
void power_update();
void power_reset_inactivity_timer();
bool power_is_screen_sleeping();
void power_wake_screen();
