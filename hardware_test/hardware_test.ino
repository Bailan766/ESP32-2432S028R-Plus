// ============================================================
// ESP32-2432S028R Plus 硬件自检程序
// 功能：LCD、触摸、RGB灯、按键、音频、电池、SD卡、GPS
// Arduino IDE 版本
// ============================================================

#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"

// ============================================================
// 全局对象
// ============================================================
TFT_eSPI tft = TFT_eSPI();

// 触摸状态
struct TouchPoint {
  int16_t x;
  int16_t y;
  bool pressed;
};

// 菜单状态
enum AppState {
  STATE_MENU = 0,
  STATE_TEST_LCD,
  STATE_TEST_TOUCH,
  STATE_TEST_RGB,
  STATE_TEST_KEY,
  STATE_TEST_AUDIO,
  STATE_TEST_BATTERY,
  STATE_TEST_SD,
  STATE_TEST_GPS,
};

AppState currentState = STATE_MENU;
AppState lastState = (AppState)-1;

// 按钮结构
struct Button {
  int16_t x, y, w, h;
  const char* label;
  uint16_t color;
  AppState targetState;
};

// 主菜单按钮 (2x4 布局)
Button menuButtons[] = {
  { 20,  40, 135, 45, "LCD 测试",    TFT_BLUE,    STATE_TEST_LCD },
  { 165, 40, 135, 45, "触摸测试",    TFT_GREEN,   STATE_TEST_TOUCH },
  { 20,  95, 135, 45, "RGB 灯",      TFT_RED,     STATE_TEST_RGB },
  { 165, 95, 135, 45, "按键测试",    TFT_ORANGE,  STATE_TEST_KEY },
  { 20, 150, 135, 45, "音频测试",    TFT_PURPLE,  STATE_TEST_AUDIO },
  { 165, 150, 135, 45, "电池检测",    TFT_CYAN,    STATE_TEST_BATTERY },
  { 20, 205, 135, 30, "SD 卡",       TFT_DARKGREY,STATE_TEST_SD },
  { 165, 205, 135, 30, "GPS",        TFT_MAGENTA, STATE_TEST_GPS },
};
const int MENU_COUNT = sizeof(menuButtons) / sizeof(menuButtons[0]);

// ============================================================
// XPT2046 软件SPI 触摸驱动
// ============================================================
class XPT2046_SoftSPI {
public:
  void begin() {
    pinMode(TOUCH_CS, OUTPUT);
    pinMode(TOUCH_SCK, OUTPUT);
    pinMode(TOUCH_MOSI, OUTPUT);
    pinMode(TOUCH_MISO, INPUT);
    pinMode(TOUCH_IRQ, INPUT);
    digitalWrite(TOUCH_CS, HIGH);
    digitalWrite(TOUCH_SCK, LOW);
  }

  bool isTouched() {
    return digitalRead(TOUCH_IRQ) == LOW;
  }

  TouchPoint getPoint() {
    TouchPoint p = {0, 0, false};
    if (!isTouched()) return p;

    int16_t rawX = readADC(0xD0);  // X+
    int16_t rawY = readADC(0x90);  // Y+

    // 软件校准：映射到屏幕坐标 (横屏模式)
    p.x = map(rawX, TS_MINX, TS_MAXX, 0, TFT_WIDTH);
    p.y = map(rawY, TS_MINY, TS_MAXY, 0, TFT_HEIGHT);

    // 范围限制
    p.x = constrain(p.x, 0, TFT_WIDTH - 1);
    p.y = constrain(p.y, 0, TFT_HEIGHT - 1);
    p.pressed = true;
    return p;
  }

private:
  uint16_t readADC(uint8_t cmd) {
    digitalWrite(TOUCH_CS, LOW);
    delayMicroseconds(1);

    // 发送命令 (高位在前)
    for (int i = 7; i >= 0; i--) {
      digitalWrite(TOUCH_SCK, LOW);
      digitalWrite(TOUCH_MOSI, (cmd >> i) & 0x01);
      delayMicroseconds(1);
      digitalWrite(TOUCH_SCK, HIGH);
      delayMicroseconds(1);
    }

    // 读取12位数据
    uint16_t value = 0;
    for (int i = 0; i < 12; i++) {
      digitalWrite(TOUCH_SCK, LOW);
      delayMicroseconds(1);
      value <<= 1;
      digitalWrite(TOUCH_SCK, HIGH);
      delayMicroseconds(1);
      if (digitalRead(TOUCH_MISO)) value |= 1;
    }

    // 额外时钟
    digitalWrite(TOUCH_SCK, LOW);
    delayMicroseconds(1);
    digitalWrite(TOUCH_SCK, HIGH);
    delayMicroseconds(1);

    digitalWrite(TOUCH_CS, HIGH);
    return value;
  }
};

XPT2046_SoftSPI touch;

// ============================================================
// RGB 灯控制 (低电平点亮, PWM)
// ============================================================
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  // 低电平点亮，所以PWM反向
  ledcWrite(1, 255 - r);
  ledcWrite(2, 255 - g);
  ledcWrite(3, 255 - b);
}

// ============================================================
// 音频 PWM
// ============================================================
void audioOn(uint16_t freq) {
  digitalWrite(AUDIO_EN, HIGH);
  ledcWriteTone(0, freq);
  ledcWrite(0, 128);  // 50% 占空比
}

void audioOff() {
  ledcWrite(0, 0);
  digitalWrite(AUDIO_EN, LOW);
}

// ============================================================
// 电池电压读取
// ============================================================
float readBatteryVoltage() {
  int raw = analogRead(BATT_ADC);
  float v = raw * 3.3 / 4095.0 * BATT_DIV_RATIO;
  return v;
}

// ============================================================
// UI 辅助函数
// ============================================================
void drawButton(Button& btn) {
  tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 6, btn.color);
  tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 6, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(btn.label, btn.x + btn.w / 2, btn.y + btn.h / 2);
}

bool buttonClicked(Button& btn, TouchPoint& tp) {
  return tp.pressed &&
         tp.x >= btn.x && tp.x <= btn.x + btn.w &&
         tp.y >= btn.y && tp.y <= btn.y + btn.h;
}

void drawBackButton() {
  tft.fillRoundRect(10, 10, 60, 28, 4, TFT_RED);
  tft.drawRoundRect(10, 10, 60, 28, 4, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("返回", 40, 24);
}

bool backButtonClicked(TouchPoint& tp) {
  return tp.pressed && tp.x >= 10 && tp.x <= 70 && tp.y >= 10 && tp.y <= 38;
}

// ============================================================
// 主菜单
// ============================================================
void drawMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("ESP32-2432S028R Plus", 160, 5);
  tft.setTextSize(1);
  tft.drawString("硬件自检工具 v1.0", 160, 24);

  for (int i = 0; i < MENU_COUNT; i++) {
    drawButton(menuButtons[i]);
  }
}

// ============================================================
// LCD 测试
// ============================================================
void testLCD() {
  static int phase = 0;
  static unsigned long lastTime = 0;

  if (lastState != STATE_TEST_LCD) {
    phase = 0;
    lastTime = 0;
    lastState = STATE_TEST_LCD;
  }

  if (millis() - lastTime > 800) {
    lastTime = millis();

    uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW,
                         TFT_CYAN, TFT_MAGENTA, TFT_WHITE, TFT_BLACK};
    const char* names[] = {"RED", "GREEN", "BLUE", "YELLOW",
                           "CYAN", "MAGENTA", "WHITE", "BLACK"};

    tft.fillScreen(colors[phase]);
    tft.setTextColor(phase >= 6 ? TFT_RED : TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(names[phase], 160, 120);
    drawBackButton();

    phase++;
    if (phase > 7) phase = 0;
  }
}

// ============================================================
// 触摸测试
// ============================================================
void testTouch() {
  static int lastX = -1, lastY = -1;

  if (lastState != STATE_TEST_TOUCH) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("触摸测试 - 点击屏幕", 160, 5);
    tft.setTextSize(1);
    tft.drawString("坐标会实时显示", 160, 28);
    lastX = -1;
    lastY = -1;
    lastState = STATE_TEST_TOUCH;
    drawBackButton();
    return;
  }

  TouchPoint tp = touch.getPoint();

  if (tp.pressed) {
    tft.fillCircle(tp.x, tp.y, 3, TFT_GREEN);
    tft.drawCircle(tp.x, tp.y, 8, TFT_YELLOW);

    if (tp.x != lastX || tp.y != lastY) {
      tft.fillRect(80, 210, 160, 25, TFT_BLACK);
      tft.setTextColor(TFT_GREEN);
      tft.setTextSize(2);
      tft.setTextDatum(TC_DATUM);
      char buf[32];
      sprintf(buf, "X:%d Y:%d", tp.x, tp.y);
      tft.drawString(buf, 160, 215);
      lastX = tp.x;
      lastY = tp.y;
    }
  }
}

// ============================================================
// RGB 灯测试
// ============================================================
void testRGB() {
  static int phase = 0;
  static unsigned long lastTime = 0;

  if (lastState != STATE_TEST_RGB) {
    phase = 0;
    lastTime = 0;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("RGB 灯测试", 160, 50);
    tft.setTextSize(1);
    tft.drawString("R=22  G=17  B=16 (低电平点亮)", 160, 80);
    lastState = STATE_TEST_RGB;
    drawBackButton();
    return;
  }

  if (millis() - lastTime > 600) {
    lastTime = millis();
    phase++;
    if (phase > 7) phase = 0;

    uint8_t r = (phase & 1) ? 255 : 0;
    uint8_t g = (phase & 2) ? 255 : 0;
    uint8_t b = (phase & 4) ? 255 : 0;
    setRGBColor(r, g, b);

    const char* names[] = {"OFF", "R", "G", "R+G", "B", "R+B", "G+B", "W"};
    tft.fillRect(100, 120, 120, 60, TFT_BLACK);
    tft.setTextColor(r || g || b ? TFT_GREEN : TFT_RED);
    tft.setTextSize(3);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(names[phase], 160, 150);
  }
}

// ============================================================
// 按键测试
// ============================================================
void testKey() {
  static int pressCount = 0;
  static bool lastBtn = HIGH;

  if (lastState != STATE_TEST_KEY) {
    pressCount = 0;
    lastBtn = HIGH;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("按键测试", 160, 40);
    tft.setTextSize(1);
    tft.drawString("请按下 BOOT 按键 (GPIO0)", 160, 70);
    lastState = STATE_TEST_KEY;
    drawBackButton();
    return;
  }

  bool cur = digitalRead(KEY_BOOT);

  tft.fillRect(60, 110, 200, 40, TFT_BLACK);
  tft.setTextColor(cur == LOW ? TFT_GREEN : TFT_RED);
  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(cur == LOW ? "PRESSED" : "RELEASED", 160, 130);

  if (lastBtn == HIGH && cur == LOW) {
    pressCount++;
  }
  lastBtn = cur;

  tft.fillRect(60, 170, 200, 30, TFT_BLACK);
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  char buf[32];
  sprintf(buf, "按下次数: %d", pressCount);
  tft.drawString(buf, 160, 185);
}

// ============================================================
// 音频测试
// ============================================================
void testAudio() {
  static int phase = 0;
  static unsigned long lastTime = 0;

  if (lastState != STATE_TEST_AUDIO) {
    phase = 0;
    lastTime = 0;
    audioOff();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("音频测试", 160, 40);
    tft.setTextSize(1);
    tft.drawString("PWM=GPIO26  EN=GPIO4", 160, 70);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("播放音阶: Do Re Mi Fa Sol La Si Do", 160, 100);
    lastState = STATE_TEST_AUDIO;
    drawBackButton();
    return;
  }

  if (millis() - lastTime > 400) {
    lastTime = millis();
    uint16_t notes[] = {262, 294, 330, 349, 392, 440, 494, 523};
    const char* names[] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "Do+"};

    if (phase < 8) {
      audioOn(notes[phase]);
      tft.fillRect(100, 140, 120, 50, TFT_BLACK);
      tft.setTextColor(TFT_GREEN);
      tft.setTextSize(3);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(names[phase], 160, 165);
      phase++;
    } else {
      audioOff();
      tft.fillRect(100, 140, 120, 50, TFT_BLACK);
      tft.setTextColor(TFT_RED);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("停止", 160, 165);
      phase = 0;
      lastTime = millis() + 1000;
    }
  }
}

// ============================================================
// 电池检测
// ============================================================
void testBattery() {
  static unsigned long lastTime = 0;

  if (lastState != STATE_TEST_BATTERY) {
    lastTime = 0;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("电池检测", 160, 30);
    tft.setTextSize(1);
    tft.drawString("ADC=GPIO34", 160, 55);
    lastState = STATE_TEST_BATTERY;
    drawBackButton();
    return;
  }

  if (millis() - lastTime > 300) {
    lastTime = millis();
    float voltage = readBatteryVoltage();
    int raw = analogRead(BATT_ADC);

    int barW = 200, barH = 40, barX = 60, barY = 90;
    tft.drawRect(barX, barY, barW, barH, TFT_WHITE);
    tft.fillRect(barX + barW, barY + 12, 8, 16, TFT_WHITE);

    float percent = (voltage - 3.3) / (4.2 - 3.3) * 100;
    percent = constrain(percent, 0, 100);
    int fillW = (int)(barW * percent / 100.0);

    uint16_t barColor = percent > 30 ? TFT_GREEN : (percent > 15 ? TFT_YELLOW : TFT_RED);
    tft.fillRect(barX + 2, barY + 2, max(0, fillW - 4), barH - 4, barColor);

    tft.fillRect(0, 150, 320, 60, TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    char buf[64];
    sprintf(buf, "%.2f V  (%.0f%%)", voltage, percent);
    tft.drawString(buf, 160, 165);

    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY);
    sprintf(buf, "ADC raw: %d", raw);
    tft.drawString(buf, 160, 190);
  }
}

// ============================================================
// SD 卡测试
// ============================================================
void testSD() {
  static bool tested = false;
  static bool cardOK = false;
  static uint32_t cardSizeMB = 0;
  static char typeStr[16] = "";
  static bool writeOK = false;
  static String readBack = "";

  if (lastState != STATE_TEST_SD) {
    tested = false;
    cardOK = false;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("SD 卡测试", 160, 30);
    tft.setTextSize(1);
    tft.drawString("SCK=18 MISO=19 MOSI=23 CS=5", 160, 55);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("正在初始化...", 160, 100);
    lastState = STATE_TEST_SD;
    drawBackButton();

    // 初始化 SD 卡 (使用 VSPI)
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) {
      cardOK = true;
      cardSizeMB = SD.cardSize() / (1024 * 1024);

      uint8_t cardType = SD.cardType();
      switch (cardType) {
        case CARD_NONE: strcpy(typeStr, "None"); break;
        case CARD_MMC:  strcpy(typeStr, "MMC"); break;
        case CARD_SD:   strcpy(typeStr, "SDSC"); break;
        case CARD_SDHC: strcpy(typeStr, "SDHC"); break;
        default:        strcpy(typeStr, "Unknown"); break;
      }

      // 写入测试
      File f = SD.open("/test.txt", FILE_WRITE);
      if (f) {
        f.println("ESP32 SD Test OK!");
        f.close();
        writeOK = true;
      }

      // 读取测试
      f = SD.open("/test.txt", FILE_READ);
      if (f) {
        readBack = f.readStringUntil('\n');
        f.close();
      }

      tested = true;
    } else {
      cardOK = false;
      tested = true;
    }
    return;
  }

  if (tested) {
    tft.fillRect(0, 80, 320, 160, TFT_BLACK);
    if (cardOK) {
      tft.setTextColor(TFT_GREEN);
      tft.setTextSize(2);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("SD 卡检测成功!", 160, 90);

      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(1);
      char buf[64];
      sprintf(buf, "类型: %s", typeStr);
      tft.drawString(buf, 160, 125);
      sprintf(buf, "容量: %lu MB", cardSizeMB);
      tft.drawString(buf, 160, 145);

      tft.setTextColor(writeOK ? TFT_GREEN : TFT_RED);
      sprintf(buf, "写入测试: %s", writeOK ? "PASS" : "FAIL");
      tft.drawString(buf, 160, 175);

      tft.setTextColor(TFT_CYAN);
      tft.drawString("读取: " + readBack, 160, 200);
    } else {
      tft.setTextColor(TFT_RED);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("未检测到 SD 卡", 160, 120);
      tft.setTextSize(1);
      tft.setTextColor(TFT_LIGHTGREY);
      tft.drawString("请插入 TF 卡后返回重试", 160, 150);
    }
    tested = false;
  }
}

// ============================================================
// GPS 测试
// ============================================================
void testGPS() {
  static char gpsBuffer[256];
  static int bufIdx = 0;
  static unsigned long lastTime = 0;
  static String lines[6];
  static int lineCount = 0;

  if (lastState != STATE_TEST_GPS) {
    bufIdx = 0;
    lineCount = 0;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("GPS 测试", 160, 5);
    tft.setTextSize(1);
    tft.drawString("UART2 RX=GPIO1 @9600", 160, 25);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("注意: 下载固件前请断开GPS!", 160, 40);

    Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, -1);
    lastState = STATE_TEST_GPS;
    drawBackButton();
    return;
  }

  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || bufIdx >= 250) {
      gpsBuffer[bufIdx] = '\0';
      for (int i = 5; i > 0; i--) lines[i] = lines[i - 1];
      lines[0] = String(gpsBuffer);
      lineCount = min(lineCount + 1, 6);
      bufIdx = 0;
    } else if (c != '\r') {
      gpsBuffer[bufIdx++] = c;
    }
  }

  if (millis() - lastTime > 500) {
    lastTime = millis();
    tft.fillRect(0, 55, 320, 185, TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);

    if (lineCount == 0) {
      tft.drawString("等待 GPS 数据...", 10, 60);
      tft.setTextColor(TFT_LIGHTGREY);
      tft.drawString("请确认 GPS 模块已连接", 10, 80);
      tft.drawString("GPS TX -> GPIO1", 10, 95);
    } else {
      for (int i = 0; i < lineCount && i < 6; i++) {
        tft.drawString(lines[i], 10, 60 + i * 18);
      }
    }
    tft.setTextDatum(TC_DATUM);
  }
}

// ============================================================
// 状态机
// ============================================================
void updateState() {
  TouchPoint tp = touch.getPoint();
  static bool lastPressed = false;

  if (tp.pressed && !lastPressed) {
    lastPressed = true;
    delay(20);  // 消抖

    if (currentState == STATE_MENU) {
      for (int i = 0; i < MENU_COUNT; i++) {
        if (buttonClicked(menuButtons[i], tp)) {
          currentState = menuButtons[i].targetState;
          lastState = (AppState)-1;
          break;
        }
      }
    } else {
      if (backButtonClicked(tp)) {
        // 清理
        if (currentState == STATE_TEST_AUDIO) audioOff();
        if (currentState == STATE_TEST_RGB)   setRGBColor(0, 0, 0);
        if (currentState == STATE_TEST_SD)    { SD.end(); SPI.end(); }
        if (currentState == STATE_TEST_GPS)   Serial2.end();
        currentState = STATE_MENU;
        lastState = (AppState)-1;
      }
    }
  }

  if (!tp.pressed) lastPressed = false;

  switch (currentState) {
    case STATE_MENU:
      if (lastState != STATE_MENU) { drawMenu(); lastState = STATE_MENU; }
      break;
    case STATE_TEST_LCD:     testLCD();     break;
    case STATE_TEST_TOUCH:   testTouch();   break;
    case STATE_TEST_RGB:     testRGB();     break;
    case STATE_TEST_KEY:     testKey();     break;
    case STATE_TEST_AUDIO:   testAudio();   break;
    case STATE_TEST_BATTERY: testBattery(); break;
    case STATE_TEST_SD:      testSD();      break;
    case STATE_TEST_GPS:     testGPS();     break;
  }
}

// ============================================================
// setup / loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP32-2432S028R Plus 硬件自检 ===");

  // ---------- RGB LED (LEDC PWM) ----------
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH);  // 低电平点亮，所以HIGH=熄灭
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);

  ledcSetup(1, 5000, 8);  ledcAttachPin(LED_R, 1);  // R
  ledcSetup(2, 5000, 8);  ledcAttachPin(LED_G, 2);  // G
  ledcSetup(3, 5000, 8);  ledcAttachPin(LED_B, 3);  // B
  setRGBColor(0, 0, 0);

  // ---------- 按键 ----------
  pinMode(KEY_BOOT, INPUT_PULLUP);

  // ---------- 音频 ----------
  pinMode(AUDIO_EN, OUTPUT);
  digitalWrite(AUDIO_EN, LOW);
  ledcSetup(0, 1000, 8);
  ledcAttachPin(AUDIO_PWM, 0);
  ledcWrite(0, 0);

  // ---------- 电池 ADC ----------
  analogSetPinAttenuation(BATT_ADC, ADC_11db);
  analogReadResolution(12);

  // ---------- LCD ----------
  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);

  // 背光
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // ---------- 触摸 ----------
  touch.begin();

  // ---------- 启动提示 ----------
  Serial.println("初始化完成");
  setRGBColor(0, 255, 0);  // 绿灯闪一下
  delay(200);
  setRGBColor(0, 0, 0);
}

void loop() {
  updateState();
  delay(10);
}
