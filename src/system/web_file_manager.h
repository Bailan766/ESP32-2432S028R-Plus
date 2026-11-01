#pragma once
#include <Arduino.h>

void web_file_manager_init();
void web_file_manager_start();
void web_file_manager_stop();
void web_file_manager_loop();
bool web_file_manager_is_running();
String web_file_manager_get_ip();
const char* web_file_manager_get_last_action();
uint32_t web_file_manager_get_upload_count();
