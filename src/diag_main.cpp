// ==============================================================================
// 硬件诊断固件 v15 —— 功放使能极性 长时裁决 (每段 10 秒)
//
// 背景: 商家演示视频显示不接外置功放即可发声 → 板上应存在功放芯片。
//       此前 4 秒短段测试用户反馈"分不清"，故本版拉长到 10 秒，仅测两段。
//
// 编排:
//   P1  GPIO4 = HIGH + DAC2(GPIO26) 1kHz 方波 10 秒
//   P2  GPIO4 = LOW  + DAC2(GPIO26) 1kHz 方波 10 秒
//   每段前有 2 秒静音作为"分界提示"，屏幕大字显示当前状态。
// ==============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <driver/dac.h>

static TFT_eSPI tft;

static void showPhase(const char* big, const char* sub, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(big, 10, 40, 7);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(sub, 10, 130, 4);
}

static void playTone(unsigned long ms) {
  dac_output_enable(DAC_CHANNEL_2);
  dacWrite(26, 128);
  const unsigned long start = millis();
  while (millis() - start < ms) {
    dacWrite(26, 240); delayMicroseconds(500);
    dacWrite(26, 15);  delayMicroseconds(500);
  }
  dacWrite(26, 128);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  tft.init();
  tft.setRotation(3);
  tft.invertDisplay(false);

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  Serial.println("########## v15 功放极性长时裁决 (每段 10 秒) ##########");
  showPhase("READY", "10s each", TFT_DARKGREY);
  delay(2000);
}

void loop() {
  // ---- P1: 使能脚 HIGH ----
  digitalWrite(4, LOW);
  showPhase("SILENT", "2s break", TFT_DARKGREY);
  delay(2000);

  digitalWrite(4, HIGH);
  showPhase("EN = HIGH", "1kHz 10s", TFT_GREEN);
  Serial.println("[P1] GPIO4=HIGH + DAC 1kHz 10s");
  playTone(10000);

  // ---- P2: 使能脚 LOW ----
  digitalWrite(4, LOW);
  showPhase("SILENT", "2s break", TFT_DARKGREY);
  delay(2000);

  showPhase("EN = LOW", "1kHz 10s", TFT_ORANGE);
  Serial.println("[P2] GPIO4=LOW + DAC 1kHz 10s");
  playTone(10000);

  showPhase("SILENT", "3s break", TFT_DARKGREY);
  delay(3000);
}
