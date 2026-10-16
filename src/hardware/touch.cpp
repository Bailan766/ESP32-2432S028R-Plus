#include "touch.h"
#include <Preferences.h>
#include <math.h>

static Preferences s_prefs;
static TouchCalibrationCoeffs s_cal;

static uint16_t s_lastRawX = 0;
static uint16_t s_lastRawY = 0;
static uint16_t s_lastRawZ1 = 0;
static uint8_t s_touchRotation = LCD_ROTATION;

#define Z_THRESHOLD 350

// 极限优化的 24 时钟位带操作 SPI
// 采用关中断 (noInterrupts) 防止 FreeRTOS 任务调度打断 SPI 时序，
// 完美模拟 SPI Mode 0：下降沿时从机更新数据，上升沿时主机采样数据。
static uint16_t spi24(uint8_t cmd) {
  noInterrupts(); // 冻结系统中断，确保 SPI 时钟绝对均匀

  // 1) 发送 8-bit 命令
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_SCK_PIN, LOW);
    digitalWrite(TOUCH_MOSI_PIN, (cmd >> i) & 0x01);
    delayMicroseconds(1); // 必须保留：XPT2046 要求时钟周期 > 400ns
    digitalWrite(TOUCH_SCK_PIN, HIGH);
    delayMicroseconds(1);
  }

  // 2) 读取 16-bit 结果
  uint16_t value = 0;
  digitalWrite(TOUCH_MOSI_PIN, LOW); 
  
  for (int i = 15; i >= 0; i--) {
    digitalWrite(TOUCH_SCK_PIN, LOW); // XPT2046 在下降沿更新 DOUT 数据
    delayMicroseconds(1);
    
    digitalWrite(TOUCH_SCK_PIN, HIGH); // 上升沿
    delayMicroseconds(1);
    
    // 关键修复：在时钟拉高后 (数据绝对稳定期) 读取 MISO
    if (digitalRead(TOUCH_MISO_PIN) == HIGH) {
      value |= (1 << i);
    }
  }
  
  interrupts(); // 恢复系统中断

  // 右移 3 位丢弃末尾的 3 个零时钟，得到真正的 12-bit ADC 结果
  return (value >> 3) & 0x0FFF;
}

void touch_reset_default_calibration() {
  // 根据官方硬件自检标准 (TS_MINX=300, TS_MAXX=3900, TS_MINY=400, TS_MAXY=3800)
  if (s_touchRotation == 3) {
    // Rotation = 3 (USB 居左正向握持, 180度翻转)
    s_cal.valid = true;
    s_cal.alphaX = -0.08861f;
    s_cal.betaX  = 0.0f;
    s_cal.deltaX = 345.58f;

    s_cal.alphaY = 0.0f;
    s_cal.betaY  = -0.07029f;
    s_cal.deltaY = 267.12f;
    LOG_TOUCH("恢复出厂默认仿射校准 (Rotation=3, USB居左)");
  } else {
    // Rotation = 1 (USB 居右标准横屏)
    s_cal.valid = true;
    s_cal.alphaX = 0.08861f;
    s_cal.betaX  = 0.0f;
    s_cal.deltaX = -26.58f;

    s_cal.alphaY = 0.0f;
    s_cal.betaY  = 0.07029f;
    s_cal.deltaY = -28.12f;
    LOG_TOUCH("恢复出厂默认仿射校准 (Rotation=1, USB居右)");
  }
}

void touch_set_rotation(uint8_t rotation) {
  if (s_touchRotation == rotation) return;
  s_touchRotation = rotation;

  // 仿射校准参数 180 度翻转变换 (Screen X -> 319 - Screen X, Screen Y -> 239 - Screen Y)
  if (s_cal.valid) {
    s_cal.alphaX = -s_cal.alphaX;
    s_cal.betaX  = -s_cal.betaX;
    s_cal.deltaX = 319.0f - s_cal.deltaX;
    s_cal.alphaY = -s_cal.alphaY;
    s_cal.betaY  = -s_cal.betaY;
    s_cal.deltaY = 239.0f - s_cal.deltaY;
  } else {
    touch_reset_default_calibration();
  }
  LOG_TOUCH("触控映射方向更新: %d", s_touchRotation);
}

uint8_t touch_get_rotation() {
  return s_touchRotation;
}

static void loadCalibration() {
  if (!s_prefs.begin("bailan_touch", true)) {
    touch_reset_default_calibration();
    return;
  }

  bool valid = s_prefs.getBool("cal_ok", false);
  uint8_t calRot = s_prefs.getUChar("cal_rot", 255);

  if (valid && (calRot == s_touchRotation || calRot == 255)) {
    s_cal.valid = true;
    s_cal.alphaX = s_prefs.getFloat("aX", 0.0f);
    s_cal.betaX  = s_prefs.getFloat("bX", 0.0f);
    s_cal.deltaX = s_prefs.getFloat("dX", 0.0f);
    s_cal.alphaY = s_prefs.getFloat("aY", 0.0f);
    s_cal.betaY  = s_prefs.getFloat("bY", 0.0f);
    s_cal.deltaY = s_prefs.getFloat("dY", 0.0f);
    s_prefs.end();

    // 合理性校验
    float det = s_cal.alphaX * s_cal.betaY - s_cal.betaX * s_cal.alphaY;
    bool isSane = (!isnan(det) && fabs(det) > 1e-5 && fabs(det) < 0.1f);

    if (isSane) {
      LOG_TOUCH("已成功从 NVS 加载用户触控校准 (Rotation=%d): X:[%.4f, %.4f, %.1f] Y:[%.4f, %.4f, %.1f]",
                s_touchRotation,
                s_cal.alphaX, s_cal.betaX, s_cal.deltaX,
                s_cal.alphaY, s_cal.betaY, s_cal.deltaY);
      return;
    }
  }

  s_prefs.end();
  touch_reset_default_calibration();
}

void touch_save_calibration() {
  if (!s_prefs.begin("bailan_touch", false)) {
    LOG_TOUCH("无法打开 NVS 写入触摸校准");
    return;
  }
  s_prefs.putBool("cal_ok", s_cal.valid);
  s_prefs.putUChar("cal_rot", s_touchRotation);
  s_prefs.putFloat("aX", s_cal.alphaX);
  s_prefs.putFloat("bX", s_cal.betaX);
  s_prefs.putFloat("dX", s_cal.deltaX);
  s_prefs.putFloat("aY", s_cal.alphaY);
  s_prefs.putFloat("bY", s_cal.betaY);
  s_prefs.putFloat("dY", s_cal.deltaY);
  s_prefs.end();
  LOG_TOUCH("触摸校准参数已永久写入 NVS Flash (Rotation=%d)", s_touchRotation);
}

void touch_init() {
  LOG_TOUCH("初始化 XPT2046 触摸驱动 (PaulStoffregen抗串扰时序+最强besttwoavg去噪)...");
  pinMode(TOUCH_SCK_PIN, OUTPUT);
  pinMode(TOUCH_MOSI_PIN, OUTPUT);
  pinMode(TOUCH_MISO_PIN, INPUT);
  pinMode(TOUCH_CS_PIN, OUTPUT);
  pinMode(TOUCH_IRQ_PIN, INPUT);

  digitalWrite(TOUCH_CS_PIN, HIGH);
  digitalWrite(TOUCH_SCK_PIN, LOW);

  digitalWrite(TOUCH_CS_PIN, LOW);
  spi24(0xD0);
  digitalWrite(TOUCH_CS_PIN, HIGH);

  loadCalibration();
  LOG_TOUCH("触摸引脚: SCK=%d, MOSI=%d, MISO=%d, CS=%d, IRQ=%d",
            TOUCH_SCK_PIN, TOUCH_MOSI_PIN, TOUCH_MISO_PIN, TOUCH_CS_PIN, TOUCH_IRQ_PIN);
}

bool touch_is_touched() {
  return (digitalRead(TOUCH_IRQ_PIN) == LOW);
}

void touch_get_raw(uint16_t &rawX, uint16_t &rawY, uint16_t &rawZ1) {
  rawX = s_lastRawX;
  rawY = s_lastRawY;
  rawZ1 = s_lastRawZ1;
}

const TouchCalibrationCoeffs& touch_get_calibration() {
  return s_cal;
}

void touch_set_calibration(const TouchCalibrationCoeffs &coeffs) {
  s_cal = coeffs;
}

// 传奇滤波器: besttwoavg (来源于 Paul Stoffregen 的 XPT2046 驱动库)
// 取三个样本中最接近的两个求平均，能极其有效剔除各种巨型跳点噪声
static int16_t besttwoavg(int16_t x, int16_t y, int16_t z) {
  int16_t da, db, dc;
  int16_t reta = 0;
  if (x > y) da = x - y; else da = y - x;
  if (x > z) db = x - z; else db = z - x;
  if (z > y) dc = z - y; else dc = y - z;

  if (da <= db && da <= dc) reta = (x + y) >> 1;
  else if (db <= da && db <= dc) reta = (x + z) >> 1;
  else reta = (y + z) >> 1;

  return reta;
}

bool touch_read_raw(uint16_t &outRawX, uint16_t &outRawY, uint16_t &outRawZ) {
  if (digitalRead(TOUCH_IRQ_PIN) != LOW) {
    return false;
  }

  digitalWrite(TOUCH_CS_PIN, LOW);

  // 1) Z 压力检测
  spi24(0xB1); 
  int16_t z1 = (int16_t)spi24(0xB1);
  spi24(0xC1);
  int16_t z2 = (int16_t)spi24(0xC1);
  
  int z = z1 + 4095 - z2;
  if (z < 0) z = 0;

  if (z < Z_THRESHOLD) {
    spi24(0xD0); 
    digitalWrite(TOUCH_CS_PIN, HIGH);
    return false;
  }

  // 2) 完美复刻硬件驱动库的交替采样机制，消除面板残留电荷！
  spi24(0x90); // Dummy Y
  spi24(0xD0); // Dummy X
  spi24(0x90); // Dummy Y
  spi24(0xD0); // Dummy X
  
  int16_t y1 = spi24(0x90);
  int16_t x1 = spi24(0xD0);
  int16_t y2 = spi24(0x90);
  int16_t x2 = spi24(0xD0);
  int16_t y3 = spi24(0x90);
  int16_t x3 = spi24(0xD0);

  digitalWrite(TOUCH_CS_PIN, HIGH);

  // 3) 使用 besttwoavg 剔除极端值
  int16_t rawX = besttwoavg(x1, x2, x3);
  int16_t rawY = besttwoavg(y1, y2, y3);

  if (rawX < 150 || rawX > 3950 || rawY < 150 || rawY > 3950) {
    return false;
  }

  outRawX = (uint16_t)rawX;
  outRawY = (uint16_t)rawY;
  outRawZ = (uint16_t)z;

  s_lastRawX  = outRawX;
  s_lastRawY  = outRawY;
  s_lastRawZ1 = outRawZ;
  return true;
}



static float s_filtX = -1.0f;
static float s_filtY = -1.0f;

bool touch_read(int16_t &outX, int16_t &outY) {
  uint16_t rx = 0, ry = 0, rz = 0;
  if (!touch_read_raw(rx, ry, rz)) {
    s_filtX = -1.0f;
    s_filtY = -1.0f;
    return false;
  }

  // 采用 2D 仿射变换计算屏幕物理坐标
  float sx = s_cal.alphaX * rx + s_cal.betaX * ry + s_cal.deltaX;
  float sy = s_cal.alphaY * rx + s_cal.betaY * ry + s_cal.deltaY;

  // 消除电阻屏“乱飘”的自适应死区平滑算法 (Deadband + Adaptive Filter)
  if (s_filtX < 0.0f) {
    s_filtX = sx;
    s_filtY = sy;
  } else {
    float dx = sx - s_filtX;
    float dy = sy - s_filtY;
    float distSq = dx * dx + dy * dy;

    if (distSq < 9.0f) {
      // 距离变化小于 3 像素：视为模拟热噪声或微颤，锁定原坐标，彻底消除“乱飘”
    } else if (distSq < 64.0f) {
      // 慢速细微拖动：强平滑滤波 (35% 新点 + 65% 历史点)，手感极其沉稳平滑
      s_filtX = 0.35f * sx + 0.65f * s_filtX;
      s_filtY = 0.35f * sy + 0.65f * s_filtY;
    } else {
      // 快速滑动：高跟手度 (75% 新点 + 25% 历史点)，无迟滞感
      s_filtX = 0.75f * sx + 0.25f * s_filtX;
      s_filtY = 0.75f * sy + 0.25f * s_filtY;
    }
  }

  outX = constrain((int)round(s_filtX), 0, SCREEN_WIDTH - 1);
  outY = constrain((int)round(s_filtY), 0, SCREEN_HEIGHT - 1);

  static unsigned long s_lastTouchLog = 0;
  if (millis() - s_lastTouchLog > 250) {
    s_lastTouchLog = millis();
    Serial.printf("[TOUCH] Raw:(%4u, %4u) Z=%4u -> Screen:(%3d, %3d)\n", rx, ry, rz, outX, outY);
  }
  return true;
}

TouchPoint touch_get_point() {
  TouchPoint pt = {0, 0, false};
  pt.isPressed = touch_read(pt.x, pt.y);
  return pt;
}

// 4点最小二乘仿射校准解算 (TI SLYT277 标准算法)
bool touch_calibrate_from_4points(const int screenX[4], const int screenY[4],
                                  const uint16_t rawX[4], const uint16_t rawY[4]) {
  double M00 = 0, M01 = 0, M02 = 0;
  double M11 = 0, M12 = 0;
  double M22 = 4.0;

  for (int i = 0; i < 4; i++) {
    double rx = (double)rawX[i];
    double ry = (double)rawY[i];
    M00 += rx * rx;
    M01 += rx * ry;
    M02 += rx;
    M11 += ry * ry;
    M12 += ry;
  }

  double det = M00 * (M11 * M22 - M12 * M12) -
               M01 * (M01 * M22 - M12 * M02) +
               M02 * (M01 * M12 - M11 * M02);

  if (fabs(det) < 1e-5) {
    LOG_TOUCH("校准矩阵奇异，无法求解！");
    return false;
  }

  double inv00 = (M11 * M22 - M12 * M12) / det;
  double inv01 = (M02 * M12 - M01 * M22) / det;
  double inv02 = (M01 * M12 - M02 * M11) / det;
  double inv10 = inv01;
  double inv11 = (M00 * M22 - M02 * M02) / det;
  double inv12 = (M02 * M01 - M00 * M12) / det;
  double inv20 = inv02;
  double inv21 = inv12;
  double inv22 = (M00 * M11 - M01 * M01) / det;

  double bx0 = 0, bx1 = 0, bx2 = 0;
  double by0 = 0, by1 = 0, by2 = 0;
  for (int i = 0; i < 4; i++) {
    double rx = (double)rawX[i];
    double ry = (double)rawY[i];
    double sx = (double)screenX[i];
    double sy = (double)screenY[i];
    bx0 += rx * sx; bx1 += ry * sx; bx2 += sx;
    by0 += rx * sy; by1 += ry * sy; by2 += sy;
  }

  s_cal.valid = true;
  s_cal.alphaX = (float)(inv00 * bx0 + inv01 * bx1 + inv02 * bx2);
  s_cal.betaX  = (float)(inv10 * bx0 + inv11 * bx1 + inv12 * bx2);
  s_cal.deltaX = (float)(inv20 * bx0 + inv21 * bx1 + inv22 * bx2);

  s_cal.alphaY = (float)(inv00 * by0 + inv01 * by1 + inv02 * by2);
  s_cal.betaY  = (float)(inv10 * by0 + inv11 * by1 + inv12 * by2);
  s_cal.deltaY = (float)(inv20 * by0 + inv21 * by1 + inv22 * by2);

  LOG_TOUCH("仿射校准解算成功！");
  LOG_TOUCH("X 轴参数: alphaX=%.5f, betaX=%.5f, deltaX=%.2f", s_cal.alphaX, s_cal.betaX, s_cal.deltaX);
  LOG_TOUCH("Y 轴参数: alphaY=%.5f, betaY=%.5f, deltaY=%.2f", s_cal.alphaY, s_cal.betaY, s_cal.deltaY);

  touch_save_calibration();
  return true;
}
