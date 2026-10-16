#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"

enum SDCardState {
  SD_STATE_UNCHECKED,
  SD_STATE_AVAILABLE,
  SD_STATE_NOT_FOUND,
  SD_STATE_ERROR
};

void sdcard_init();
bool sdcard_reinit();
SDCardState sdcard_get_state();
bool sdcard_is_available();
uint32_t sdcard_get_size_mb();
const char* sdcard_get_state_str();
