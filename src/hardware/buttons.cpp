#include "buttons.h"

static bool s_lastState = HIGH;
static unsigned long s_pressStartTime = 0;
static bool s_longPressTriggered = false;

void button_init() {
  LOG_SYS("初始化 BOOT 实体按键 (GPIO%d, 内部上拉输入)...", KEY_BOOT_PIN);
  pinMode(KEY_BOOT_PIN, INPUT_PULLUP);
}

ButtonEvent button_update() {
  bool currentState = digitalRead(KEY_BOOT_PIN);
  ButtonEvent event = BTN_EVENT_NONE;

  // 按键从未按下 (HIGH) 变为按下 (LOW)
  if (s_lastState == HIGH && currentState == LOW) {
    s_pressStartTime = millis();
    s_longPressTriggered = false;
  }
  // 按键持续按下中
  else if (s_lastState == LOW && currentState == LOW) {
    if (!s_longPressTriggered && (millis() - s_pressStartTime >= 600)) {
      s_longPressTriggered = true;
      event = BTN_EVENT_LONG_PRESS;
      LOG_SYS("按键检测: BOOT 长按触发 (>600ms)");
    }
  }
  // 按键松开 (LOW -> HIGH)
  else if (s_lastState == LOW && currentState == HIGH) {
    unsigned long duration = millis() - s_pressStartTime;
    // 消抖时间 > 40ms 且未触发长按，判定为短按单次点击
    if (!s_longPressTriggered && duration >= 40 && duration < 600) {
      event = BTN_EVENT_CLICK;
      LOG_SYS("按键检测: BOOT 短按点击 (%lu ms)", duration);
    }
  }

  s_lastState = currentState;
  return event;
}

bool button_is_pressed() {
  return digitalRead(KEY_BOOT_PIN) == LOW;
}
