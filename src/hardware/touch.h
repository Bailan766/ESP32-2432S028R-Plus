#pragma once
#include <Arduino.h>
#include "config.h"

struct TouchPoint {
  int16_t x;
  int16_t y;
  bool isPressed;
};

// 仿射变换校准参数结构 (TI SLYT277 算法)
// screenX = alphaX * rawX + betaX * rawY + deltaX
// screenY = alphaY * rawX + betaY * rawY + deltaY
struct TouchCalibrationCoeffs {
  bool valid;
  float alphaX;
  float betaX;
  float deltaX;
  float alphaY;
  float betaY;
  float deltaY;
};

void touch_init();
bool touch_read(int16_t &outX, int16_t &outY);
TouchPoint touch_get_point();
void touch_get_raw(uint16_t &rawX, uint16_t &rawY, uint16_t &rawZ1);
bool touch_read_raw(uint16_t &outRawX, uint16_t &outRawY, uint16_t &outRawZ);
bool touch_is_touched();

// 仿射校准计算与存储
const TouchCalibrationCoeffs& touch_get_calibration();
void touch_set_calibration(const TouchCalibrationCoeffs &coeffs);
bool touch_calibrate_from_4points(const int screenX[4], const int screenY[4],
                                  const uint16_t rawX[4], const uint16_t rawY[4]);
void touch_save_calibration();
void touch_reset_default_calibration();
void touch_set_rotation(uint8_t rotation);
uint8_t touch_get_rotation();

// 校准数据合理性自检
// 正常标定下 X 主要由 rawX 决定、Y 主要由 rawY 决定，主系数应在 0.05~0.15 量级；
// 若主系数接近 0 而交叉项很大，说明 4 点采样点错了位置（矩阵被转置/镜像），
// 此时触摸坐标会整体错乱（点左下会跳到右上），必须重新校准。
bool touch_calibration_is_sane();

// 当前是否使用用户实测校准（false = 正在用出厂默认值，坐标可能与实际屏不匹配）
bool touch_has_user_calibration();
