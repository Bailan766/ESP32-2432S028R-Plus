#pragma once
#include <Arduino.h>

void diagnostics_init();
void diagnostics_update();
void diagnostics_dump_now();
uint32_t diagnostics_get_free_heap();
uint32_t diagnostics_get_min_free_heap();
uint32_t diagnostics_get_uptime_sec();
