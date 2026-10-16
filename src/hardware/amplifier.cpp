#include "amplifier.h"

static bool s_ampEnabled = false;
static bool s_ampMuted = false;

void amp_init() {
  LOG_AMP("初始化板载功放控制引脚 (AMP_ENABLE = GPIO%d)...", AMP_ENABLE_PIN);
  pinMode(AMP_ENABLE_PIN, OUTPUT);
  // 默认关闭功放，防止开机瞬间冲击电流与啸叫
  digitalWrite(AMP_ENABLE_PIN, LOW);
  s_ampEnabled = false;
  s_ampMuted = false;
  LOG_AMP("功放默认置于 SHUTDOWN 节能消噪状态");
}

void amp_enable() {
  if (s_ampEnabled && !s_ampMuted) return;
  // 防爆音时序：使能前微延迟，让直流偏置建立
  delayMicroseconds(500);
  digitalWrite(AMP_ENABLE_PIN, HIGH);
  s_ampEnabled = true;
  s_ampMuted = false;
  LOG_AMP("功放已使能 (GPIO4 -> HIGH)");
}

void amp_disable() {
  if (!s_ampEnabled) return;
  // 先关功放使能，彻底切断输出
  digitalWrite(AMP_ENABLE_PIN, LOW);
  s_ampEnabled = false;
  LOG_AMP("功放已关闭 (GPIO4 -> LOW)");
}

void amp_mute() {
  s_ampMuted = true;
  digitalWrite(AMP_ENABLE_PIN, LOW);
  LOG_AMP("功放已静音");
}

void amp_unmute() {
  s_ampMuted = false;
  if (s_ampEnabled) {
    digitalWrite(AMP_ENABLE_PIN, HIGH);
    LOG_AMP("功放已解除静音");
  }
}

bool amp_is_enabled() {
  return s_ampEnabled && !s_ampMuted;
}

bool amp_is_muted() {
  return s_ampMuted;
}
