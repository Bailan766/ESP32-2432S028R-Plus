#include "net_manager.h"
#include <WiFiManager.h>
#include <time.h>
#include "hardware/display.h"
#include "ui/font_cn.h"

void net_init() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(TFT_BLACK);
  font_draw_string_center(160, 100, "正在连接 Wi-Fi...", TFT_WHITE, TFT_BLACK, 1);
  font_draw_string_center(160, 130, "若失败请连接热点: Bailan_Clock", TFT_GREEN, TFT_BLACK, 1);
  font_draw_string_center(160, 160, "配网时间限制 2 分钟", TFT_DARKGREY, TFT_BLACK, 1);

  WiFiManager wifiManager;
  wifiManager.setTimeout(120); // 2 minutes timeout
  // Auto connect, if it fails it will open AP "Bailan_Clock"
  if(!wifiManager.autoConnect("Bailan_Clock")) {
    // Timeout, but we continue anyway
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(TFT_BLACK);
    font_draw_string_center(160, 120, "Wi-Fi 连接成功！正在同步时间...", TFT_GREEN, TFT_BLACK, 1);
    configTime(8 * 3600, 0, "pool.ntp.org", "ntp1.aliyun.com");
    delay(1000); // Give it a second to fetch time
  }
}

bool net_is_connected() {
  return WiFi.status() == WL_CONNECTED;
}
