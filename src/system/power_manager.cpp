#include "power_manager.h"
#include "settings_manager.h"
#include "hardware/display.h"
#include "hardware/rgb.h"
#include "player/player.h"
#include "system/net_manager.h"
#include "config.h"
#include <esp_sleep.h>

static unsigned long s_lastActivityTime = 0;
static uint32_t s_sleepMsAccum = 0;   // 累计浅睡眠时长，用于诊断

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

// ------------------------------------------------------------------------------
// 息屏浅睡眠
//
// ESP32 在 light sleep 下 CPU 时钟被门控，RAM 与大部分外设寄存器保持，
// 自身电流从约 20mA 降到约 1mA，唤醒后程序从 esp_light_sleep_start() 之后继续执行。
//
// 为什么用定时器轮询而不是 GPIO 中断唤醒：
//   XPT2046 的 PENIRQ 接 GPIO36，而 ESP32 的 GPIO34~39 是输入专用脚、没有内部上下拉；
//   若板上没给这根线加外部上拉，它平时是浮空的，用作 ext1 唤醒源会持续误唤醒。
//   200ms 定时轮询不依赖任何额外硬件，代价只有 200ms 的触摸响应延迟。
//
// 为什么 Wi-Fi 射频开着时不睡：
//   浅睡眠会暂停 CPU，射频协议栈的定时任务无法及时响应，容易掉线。
//   本项目省电策略下射频只在同步窗口内开启，其余时间已关闭，因此不影响使用。
// ------------------------------------------------------------------------------
bool power_can_light_sleep() {
#if !POWER_LIGHT_SLEEP
  return false;
#else
  if (!display_is_sleeping()) return false;                       // 屏幕亮着不睡
  if (player_get_state() == STATE_PLAYING) return false;          // 正在放歌不睡
  if (net_radio_is_on()) return false;                            // 射频开着不睡
  return true;
#endif
}

void power_light_sleep(uint32_t ms) {
#if POWER_LIGHT_SLEEP
  if (!power_can_light_sleep()) return;

  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();

  // 浅睡眠只门控时钟，唤醒后 CPU 频率与绝大部分外设寄存器保持原状，无需重新配置
  s_sleepMsAccum += ms;
#else
  (void)ms;
#endif
}

uint32_t power_get_sleep_seconds() {
  return s_sleepMsAccum / 1000;
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
