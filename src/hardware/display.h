#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

// 获取全局 TFT_eSPI 绘图实例
TFT_eSPI& display_get_tft();

// 显示子系统生命周期与亮度控制
void display_init();
void display_set_rotation(uint8_t rotation);
uint8_t display_get_rotation();
void display_set_brightness(uint8_t percent); // 0 ~ 100%
uint8_t display_get_brightness();
void display_sleep();
void display_wakeup();
bool display_is_sleeping();
