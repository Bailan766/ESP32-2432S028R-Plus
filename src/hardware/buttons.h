#pragma once
#include <Arduino.h>
#include "config.h"

enum ButtonEvent {
  BTN_EVENT_NONE,
  BTN_EVENT_CLICK,
  BTN_EVENT_LONG_PRESS
};

void button_init();
ButtonEvent button_update();
bool button_is_pressed();
