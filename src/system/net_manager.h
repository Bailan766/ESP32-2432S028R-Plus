#pragma once
#include <Arduino.h>

void net_init();
void net_update();
bool net_is_connected();
String net_get_ip();
String net_get_ssid();
void net_sync_time();
void net_start_config_portal();
void net_set_wifi(const char* ssid, const char* pass);
void net_scan_and_print();

// ---------------- 省电相关 ----------------
// 开启/关闭 Wi-Fi 省电模式（默认开启）：联网完成同步后自动关闭射频，
// 之后按固定周期短暂唤醒刷新天气，大幅降低待机电流。
void net_set_power_save(bool enable);
bool net_get_power_save();

// 手动唤醒射频并保持 keepMs 毫秒（用于配网、手动刷新天气等）
void net_wake_radio(uint32_t keepMs);

// 射频当前是否开启
bool net_radio_is_on();
