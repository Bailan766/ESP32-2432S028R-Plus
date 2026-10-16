#include "power_manager.h"
#include "settings_manager.h"
#include "hardware/display.h"
#include "player/player.h"

static unsigned long s_lastActivityTime = 0;

void power_init() {
  s_lastActivityTime = millis();
}

void power_reset_inactivity_timer() {
  s_lastActivityTime = millis();
}

void power_wake_screen() {
  s_lastActivityTime = millis();
  if (display_is_sleeping()) {
    display_wakeup();
  }
}

bool power_is_screen_sleeping() {
  return display_is_sleeping();
}

void power_update() {
  uint16_t timeoutSec = settings_get().autoSleepSeconds;
  if (timeoutSec == 0) return; // 0 表示从不息屏

  if (!display_is_sleeping()) {
    if (millis() - s_lastActivityTime >= (unsigned long)timeoutSec * 1000) {
      display_sleep();
    }
  }
}
