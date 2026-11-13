#include "power_manager.h"
#include "settings_manager.h"
#include "hardware/display.h"
#include "hardware/rgb.h"
#include "hardware/touch.h"
#include "hardware/battery.h"
#include "hardware/amplifier.h"
#include "hardware/audio.h"
#include "player/player.h"
#include "bluetooth/bt_sink.h"
#include "radio/web_radio.h"
#include "system/net_manager.h"
#include "ui/font_cn.h"
#include "config.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

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
// 息屏硬件级浅睡眠 (Light Sleep)
//
// ESP32 在 light sleep 下 CPU 时钟被门控，RAM 与大部分外设寄存器保持，
// 自身电流从约 20mA 降到约 1mA，唤醒后程序从 esp_light_sleep_start() 之后继续执行。
//
// 唤醒机制：
//   1) 硬件中断即刻唤醒：配置 GPIO 36 (XPT2046 PENIRQ) 和 GPIO 0 (BOOT按键) 为低电平唤醒源；
//      手指一碰触屏或按键按下，在 < 1ms 内瞬间唤醒，完全无延迟！
//   2) 定时器兜底唤醒：250ms 定时唤醒一次处理后台周期性家务。
// ------------------------------------------------------------------------------
bool power_can_light_sleep() {
#if !POWER_LIGHT_SLEEP
  return false;
#else
  if (!display_is_sleeping()) return false;                       // 屏幕亮着不睡
  if (player_get_state() == STATE_PLAYING) return false;          // 正在放歌不睡
  if (bt_sink_is_active() || web_radio_is_playing() || audio_is_playing()) return false; // 蓝牙/电台/音频流不睡
  if (net_radio_is_on()) return false;                            // 射频开着不睡
  return true;
#endif
}

void power_light_sleep(uint32_t ms) {
#if POWER_LIGHT_SLEEP
  if (!power_can_light_sleep()) return;

  // 1. 睡前准备触控控制器 (使能 PENIRQ 上拉与低功耗态)
  touch_sleep_prepare();

  // 2. 配置硬件级 GPIO 唤醒源：
  //    - GPIO 36 (XPT2046 PENIRQ 低电平有效)
  //    - GPIO 0  (BOOT 实体按键低电平有效)
  gpio_wakeup_enable((gpio_num_t)TOUCH_IRQ_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)KEY_BOOT_PIN,  GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  // 3. 配置定时器兜底唤醒
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);

  // 4. 进入硬件浅睡眠 (CPU时钟门控，功耗降至 ~15mA)
  esp_light_sleep_start();

  // 5. 唤醒后第一时间恢复 CPU 主频基准与微秒定时器，杜绝微秒延时漂移
  setCpuFrequencyMhz(s_cpuMhz);

  // 6. 恢复触控引脚与 SPI 状态
  touch_wake_restore();

  // 7. 统计浅睡眠累计时长
  s_sleepMsAccum += ms;
#else
  (void)ms;
#endif
}

uint32_t power_get_sleep_seconds() {
  return s_sleepMsAccum / 1000;
}

// ------------------------------------------------------------------------------
// 低电量保护
//
// 锂电池放电到 3.3V 以下会造成不可逆损伤，必须在到达截止电压前关机。
// 策略：
//   - 电压 < BATTERY_PROTECT_VOLT(3.35V) 且非 USB 供电 -> 停止播放、关功放、
//     亮屏提示 3.5 秒后进入深度睡眠。
//   - 若电压 < 2.50V 说明未插电池（纯 USB 供电，常见于开发调试），跳过保护不误杀。
//   - 配置 60 秒定时唤醒：防止在不完全断电或插着 USB 时设备永久陷入死锁变砖。
// ------------------------------------------------------------------------------
#define BATTERY_PROTECT_VOLT   3.35f  // 强制关机阈值
#define BATTERY_MIN_VALID_VOLT 2.50f  // 电池有效电压下限（低于此值视为未插电池纯USB供电）

// 深睡前的低压提示画面（此时不再依赖任何后台任务，画完即睡）
static void drawBatteryCriticalScreen() {
  if (display_is_sleeping()) {
    display_wakeup();
  }
  ledcWrite(LCD_BL_LEDC_CH, 160); // 确保背光点亮以看清提示

  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);
  font_draw_string_center(160, 84, "电量耗尽", TFT_RED, COLOR_BG_DARK, 2);
  font_draw_string_center(160, 126, "即将关机，请连接充电器", TFT_YELLOW, COLOR_BG_DARK, 1);
  tft.fillRect(110, 156, 100, 26, TFT_BLACK);
  tft.drawRect(110, 156, 100, 26, TFT_RED);
  tft.drawRect(210, 163, 5, 12, TFT_RED);
  font_draw_string_center(160, 196, "0%", TFT_RED, COLOR_BG_DARK, 1);
}

// 运行期低压检查（每 5 秒一次，需连续 3 次确认以防瞬时电压跌落误关机）
static void batteryGuardUpdate() {
  static unsigned long s_lastGuard = 0;
  static uint8_t s_lowVoltCount = 0;

  if (millis() - s_lastGuard < 5000) return;
  s_lastGuard = millis();

  float v = battery_get_voltage();

  // 1. 若电压低于 2.5V，说明未接入锂电池（纯 USB 供电），跳过保护
  // 2. 若检测到处于 USB 供电或充电状态，跳过保护
  if (v < BATTERY_MIN_VALID_VOLT || battery_is_usb_powered()) {
    s_lowVoltCount = 0;
    return;
  }

  // 3. 电压低于 3.35V 时累计确认计数
  if (v < BATTERY_PROTECT_VOLT) {
    s_lowVoltCount++;
    if (s_lowVoltCount < 3) {
      LOG_SYS("检测到电池低压 %.2fV，连续确认 (%d/3)...", v, s_lowVoltCount);
      return;
    }
  } else {
    s_lowVoltCount = 0;
    return;
  }

  LOG_SYS("低电量保护连续触发: %.2fV < %.2fV，执行安全关机", v, BATTERY_PROTECT_VOLT);
  player_stop();
  amp_mute();
  rgb_set_state(RGB_LOW_BATTERY);
  drawBatteryCriticalScreen();
  delay(3500); // 让用户看清提示
  ledcWrite(LCD_BL_LEDC_CH, 0);
  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // 定时 60 秒唤醒探测充电，防止在不完全断电场景下永久变砖死锁
  esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void power_update() {
  // 0. 低电量保护（优先级最高，独立于息屏策略）
  batteryGuardUpdate();

  // 1. CPU 频率自适应
  if (player_get_state() == STATE_PLAYING || bt_sink_is_active() || web_radio_is_playing() || audio_is_playing()) {
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
