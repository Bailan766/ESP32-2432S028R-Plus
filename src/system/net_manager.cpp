#include "net_manager.h"
#include <WiFiManager.h>
#include <time.h>
#include "hardware/display.h"
#include "ui/font_cn.h"
#include "weather.h"
#include "config.h"

static bool s_timeSynced = false;
static unsigned long s_lastWifiCheck = 0;

void net_sync_time() {
  LOG_UI("正在通过 NTP 服务同步北京时间...");
  configTime(8 * 3600, 0, "ntp1.aliyun.com", "pool.ntp.org", "time.asia.apple.com");
  s_timeSynced = true;
}

void net_init() {
  // 快速非阻塞初始化：尝试使用已存储的 WiFi 密码在后台连接
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  LOG_UI("Wi-Fi 后台静默重连启动中...");
}

void net_update() {
  // 周期性检查连接状态
  if (millis() - s_lastWifiCheck >= 2000) {
    s_lastWifiCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (!s_timeSynced) {
        net_sync_time();
        weather_fetch();
      }
      weather_update();
    }
  }
}

bool net_is_connected() {
  return (WiFi.status() == WL_CONNECTED);
}

String net_get_ip() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return "未连接";
}

void net_start_config_portal() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);
  font_draw_string_center(160, 40, "进入 Wi-Fi 智能配网模式", COLOR_ACCENT_GREEN, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 75, "1. 手机打开 Wi-Fi 搜索热点:", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 105, "Bailan_Clock", COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 140, "2. 手机浏览器打开网页配网", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 170, "配网超时时间: 90 秒", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 205, "- 点击屏幕任意处可跳过 -", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(90); // 90 秒超时退出
  wifiManager.startConfigPortal("Bailan_Clock");

  if (WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(COLOR_BG_DARK);
    font_draw_string_center(160, 110, "Wi-Fi 连接成功！正在同步时间与天气...", COLOR_ACCENT_GREEN, COLOR_BG_DARK, 1);
    net_sync_time();
    weather_fetch();
    delay(1000);
  }
}
