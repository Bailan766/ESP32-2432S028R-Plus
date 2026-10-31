#include "power_manager.h"
#include "settings_manager.h"
#include "hardware/display.h"
#include "hardware/rgb.h"
#include "player/player.h"

static unsigned long s_lastActivityTime = 0;

// 当前 CPU 频率（MHz）。ESP32 默认 240MHz，是待机功耗的主要来源之一。
static uint8_t s_cpuMhz = 240;

// 分档策略：
//   播放音乐      -> 240MHz（解码 + DAC 节流需要余量）
//   亮屏待机      -> 160MHz（UI 流畅，功耗约为 240MHz 的 2/3）
//   息屏待机      ->  80MHz（Wi-Fi 要求的最低频率，此时通常射频已关）
#define CPU_PLAYING_MHZ  240
#define CPU_ACTIVE_MHZ   160
#define CPU_IDLE_MHZ      80

static void applyCpuFreq(uint8_t mhz) {
  if (mhz == s_cpuMhz) return;
  setCpuFrequencyMhz(mhz);
  s_cpuMhz = (uint8_t)getCpuFrequencyMhz();
  LOG_SYS("CPU 频率调整为 %d MHz", s_cpuMhz);
}

void power_init() {
  s_lastActivityTime = millis();
  s_cpuMhz = (uint8_t)getCpuFrequencyMhz();
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

uint8_t power_get_cpu_mhz() {
  return s_cpuMhz;
}

void power_update() {
  // 1. CPU 频率自适应
  if (player_get_state() == STATE_PLAYING) {
    applyCpuFreq(CPU_PLAYING_MHZ);
  } else if (display_is_sleeping()) {
    applyCpuFreq(CPU_IDLE_MHZ);
  } else {
    applyCpuFreq(CPU_ACTIVE_MHZ);
  }

  // 2. 息屏时关闭 RGB 灯，避免额外耗电
  static bool s_rgbOffForSleep = false;
  if (display_is_sleeping()) {
    if (!s_rgbOffForSleep) {
      rgb_set_state(RGB_OFF);
      s_rgbOffForSleep = true;
    }
  } else {
    s_rgbOffForSleep = false;
  }

  // 3. 自动息屏倒计时
  uint16_t timeoutSec = settings_get().autoSleepSeconds;
  if (timeoutSec == 0) return; // 0 表示从不息屏

  if (!display_is_sleeping()) {
    if (millis() - s_lastActivityTime >= (unsigned long)timeoutSec * 1000) {
      display_sleep();
    }
  }
}
