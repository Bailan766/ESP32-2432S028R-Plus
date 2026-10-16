#pragma once
#include <Arduino.h>
#include "config.h"

enum RGBState {
  RGB_OFF,
  RGB_BOOT,
  RGB_PLAYING,
  RGB_PAUSED,
  RGB_ERROR,
  RGB_LOW_BATTERY
};

void rgb_init();
void rgb_set_color(uint8_t r, uint8_t g, uint8_t b);
void rgb_set_state(RGBState state);
void rgb_pulse_pitch(uint16_t freq);
void rgb_update();
RGBState rgb_get_state();
