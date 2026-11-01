#pragma once
#include <Arduino.h>

struct RadioStation {
  const char* name;
  const char* genre;
  const char* url;
};

void web_radio_init();
void web_radio_start();
void web_radio_stop();
void web_radio_loop();
bool web_radio_is_playing();
void web_radio_toggle_play();
void web_radio_next();
void web_radio_prev();
const RadioStation* web_radio_get_current_station();
int web_radio_get_station_index();
int web_radio_get_total_stations();
const char* web_radio_get_status_text();
