#pragma once
#include <Arduino.h>

void net_init();
void net_update();
bool net_is_connected();
String net_get_ip();
void net_sync_time();
void net_start_config_portal();
