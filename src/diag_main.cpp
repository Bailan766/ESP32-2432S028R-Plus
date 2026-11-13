// ==============================================================================
// 功放输入引脚扫描 v11.0 —— 在【已知完好的新扬声器】上重做全脚扫描
//
// 背景：早期曾把 14 个 GPIO 逐个当 1kHz 信号源，结论"全部只有嗡声"，
//       但那次测试用的是【坏喇叭】，整个结论作废。现在换了新扬声器，
//       必须重扫，否则永远在错误前提上推理。
//
// 做法：候选脚逐个输出 1kHz 数字方波 2.5 秒，GPIO4 常置 LOW（假定功放使能），
//       屏幕大字显示当前 GPIO 号。听到哪一段响，报出屏幕上的数字即可。
//
// 排除：6~11 (Flash/PSRAM)、1/3 (串口)、34/35/36/39 (输入专用)、
//       12/13/14/15/2/21 (LCD 已占用，扫了会花屏)、4 (固定作使能)。
// ==============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <driver/dac.h>

#define AMP_EN_PIN   4
#define TFT_BL_PIN   21

static const int kPins[] = { 0, 5, 16, 17, 18, 19, 22, 23, 25, 26, 27, 32, 33 };
static const int kCount  = sizeof(kPins) / sizeof(kPins[0]);

static TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("\n=======================================================");
  Serial.println("  AMP INPUT PIN SCANNER v11.0 (fresh speaker)");
  Serial.println("=======================================================");

  ledcSetup(4, 5000, 8);
  ledcAttachPin(TFT_BL_PIN, 4);
  ledcWrite(4, 204);

  tft.init();
  tft.setRotation(3);
  tft.invertDisplay(false);
  tft.fillScreen(TFT_BLACK);

  pinMode(AMP_EN_PIN, OUTPUT);
  digitalWrite(AMP_EN_PIN, LOW);          // 假定低电平开启功放，全程保持
  dac_output_disable(DAC_CHANNEL_2);      // 释放 GPIO26 供数字输出
}

void loop() {
  for (int i = 0; i < kCount; i++) {
    int pin = kPins[i];
    char tag[24];
    snprintf(tag, sizeof(tag), "GPIO %d", pin);

    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, 320, 40, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("PIN SCANNER  /  listen 1kHz", 160, 20, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(tag, 160, 105, 6);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(tag, sizeof(tag), "%d / %d", i + 1, kCount);
    tft.drawString(tag, 160, 170, 2);

    Serial.printf(">>> scanning GPIO %d (%d/%d)\n", pin, i + 1, kCount);

    pinMode(pin, OUTPUT);
    unsigned long end = millis() + 2500;
    while (millis() < end) {
      digitalWrite(pin, HIGH);
      delayMicroseconds(500);
      digitalWrite(pin, LOW);
      delayMicroseconds(500);
    }
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);                 // 扫完释放，避免与其他脚打架

    tft.fillScreen(TFT_BLACK);
    delay(400);                          // 段间静音，方便分辨
  }
}
