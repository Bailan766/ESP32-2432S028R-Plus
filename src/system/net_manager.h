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
