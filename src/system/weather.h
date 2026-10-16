#pragma once
#include <Arduino.h>

struct WeatherInfo {
  char temp[16];      // 例如: "+24°C"
  char desc[32];      // 例如: "晴朗"
  char humidity[16];  // 例如: "45%"
  char city[32];      // 例如: "北京 / 自动"
  bool valid;         // 是否已成功获取数据
  unsigned long last_update;
};

void weather_init();
void weather_update();
bool weather_fetch();
const WeatherInfo& weather_get_info();
