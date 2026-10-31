#pragma once
#include <Arduino.h>

void power_init();
void power_update();
void power_reset_inactivity_timer();
bool power_is_screen_sleeping();
void power_wake_screen();

// 当前 CPU 工作频率（MHz），用于诊断显示
uint8_t power_get_cpu_mhz();

// 是否允许进入浅睡眠（息屏 + 未播放 + Wi-Fi 射频已关）
bool power_can_light_sleep();

// 进入 light sleep 指定毫秒（内部用定时器唤醒）。调用前必须确认 power_can_light_sleep()
void power_light_sleep(uint32_t ms);

// 统计：本次开机累计浅睡眠时长（秒），用于诊断
uint32_t power_get_sleep_seconds();
