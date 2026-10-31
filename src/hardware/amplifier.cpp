#include "amplifier.h"

// ------------------------------------------------------------------------------
// 依据厂商在线烧录页的官方接线说明：
//   「喇叭 8002：GPIO26 DAC2 → AUDIO_IN，GPIO4 LOW 开启功放」
// 即：8002 类功放（PAM8002 / LM8002）的 SHUTDOWN 脚为【低电平使能】。
//
//   GPIO4 = LOW  -> 功放工作
//   GPIO4 = HIGH -> 功放 Shutdown，消除静态底噪
//
// 音频信号走内部 DAC2 (GPIO26)，经 CN1「2.0P 音频接口」引出到功放的 AUDIO_IN。
// ------------------------------------------------------------------------------
#define AMP_ON_LEVEL   LOW
#define AMP_OFF_LEVEL  HIGH

static bool s_ampEnabled = false;
static bool s_ampMuted = false;

void amp_init() {
  LOG_AMP("初始化功放使能脚 (AUDIO_EN = GPIO%d, 低电平有效)...", AMP_ENABLE_PIN);
  pinMode(AMP_ENABLE_PIN, OUTPUT);
  // 默认关闭功放，防止开机瞬间冲击电流与上电 POP 音
  digitalWrite(AMP_ENABLE_PIN, AMP_OFF_LEVEL);
  s_ampEnabled = false;
  s_ampMuted = false;
  LOG_AMP("功放默认置于 SHUTDOWN 状态 (GPIO%d = HIGH)", AMP_ENABLE_PIN);
}

void amp_enable() {
  if (s_ampEnabled && !s_ampMuted) return;
  // 防爆音时序：使能前微延迟，让直流偏置建立
  delayMicroseconds(500);
  digitalWrite(AMP_ENABLE_PIN, AMP_ON_LEVEL);
  s_ampEnabled = true;
  s_ampMuted = false;
  LOG_AMP("功放已使能 (GPIO%d -> LOW)", AMP_ENABLE_PIN);
}

void amp_disable() {
  if (!s_ampEnabled) return;
  // 先关功放使能，彻底切断输出
  digitalWrite(AMP_ENABLE_PIN, AMP_OFF_LEVEL);
  s_ampEnabled = false;
  LOG_AMP("功放已关闭 (GPIO%d -> HIGH)", AMP_ENABLE_PIN);
}

void amp_mute() {
  s_ampMuted = true;
  digitalWrite(AMP_ENABLE_PIN, AMP_OFF_LEVEL);
  LOG_AMP("功放已静音");
}

void amp_unmute() {
  s_ampMuted = false;
  if (s_ampEnabled) {
    digitalWrite(AMP_ENABLE_PIN, AMP_ON_LEVEL);
    LOG_AMP("功放已解除静音");
  }
}

bool amp_is_enabled() {
  return s_ampEnabled && !s_ampMuted;
}

bool amp_is_muted() {
  return s_ampMuted;
}
