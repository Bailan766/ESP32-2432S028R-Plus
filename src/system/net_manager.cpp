#include "net_manager.h"
#include <WiFiManager.h>
#include <Preferences.h>
#include <time.h>
#include <SD.h>
#include "hardware/display.h"
#include "ui/font_cn.h"
#include "weather.h"
#include "config.h"

static bool s_timeSynced = false;
static unsigned long s_lastWifiCheck = 0;
static bool s_wasConnected = false;
static Preferences s_prefs;

void net_sync_time() {
  LOG_UI("正在通过 NTP 服务同步北京时间...");
  configTime(8 * 3600, 0, "ntp1.aliyun.com", "pool.ntp.org", "time.asia.apple.com");
  s_timeSynced = true;
}

void net_scan_and_print() {
  Serial.println("[WIFI] 正在扫描周围 2.4GHz Wi-Fi 网络...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("[WIFI] 未扫描到任何可用 Wi-Fi 网络");
  } else {
    Serial.printf("[WIFI] 扫描到 %d 个 Wi-Fi 网络:\n", n);
    for (int i = 0; i < n; ++i) {
      Serial.printf("  [%02d] SSID: %-24s | 信号: %3d dBm | 加密: %s\n",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "开放" : "加密");
    }
    Serial.println("[WIFI] 可在串口发送 'WIFI:WiFi名称,密码' 快速连接！");
  }
}

void net_set_wifi(const char* ssid, const char* pass) {
  if (!ssid || strlen(ssid) == 0) return;

  Serial.printf("[WIFI] 收到配置指令: SSID='%s'\n", ssid);
  s_prefs.begin("wifi_cfg", false);
  s_prefs.putString("ssid", ssid);
  s_prefs.putString("pass", pass ? pass : "");
  s_prefs.end();

  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, pass ? pass : "");
  Serial.println("[WIFI] 正在连接指定 Wi-Fi...");
}

static void checkSdCardWifiFile() {
  if (SD.exists("/wifi.txt")) {
    File f = SD.open("/wifi.txt", "r");
    if (f) {
      String line = f.readStringUntil('\n');
      line.trim();
      int comma = line.indexOf(',');
      if (comma > 0) {
        String ssid = line.substring(0, comma);
        String pass = line.substring(comma + 1);
        ssid.trim();
        pass.trim();
        Serial.printf("[WIFI] 从 TF 卡 /wifi.txt 读取到配置: SSID='%s'\n", ssid.c_str());
        net_set_wifi(ssid.c_str(), pass.c_str());
      }
      f.close();
    }
  }
}

void net_init() {
  WiFi.mode(WIFI_STA);
  s_prefs.begin("wifi_cfg", true);
  String savedSsid = s_prefs.getString("ssid", "");
  String savedPass = s_prefs.getString("pass", "");
  s_prefs.end();

  if (savedSsid.length() > 0) {
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    Serial.printf("[WIFI] 正在自动连接已存 Wi-Fi: '%s'...\n", savedSsid.c_str());
  } else {
    WiFi.begin(); // 尝试使用 SDK 原生 NVS
    Serial.println("[WIFI] 后台静默重连启动中...");
  }

  checkSdCardWifiFile();
}

static void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("SCAN")) {
      net_scan_and_print();
    } else if (cmd.equalsIgnoreCase("STATUS") || cmd.equalsIgnoreCase("WIFI?")) {
      Serial.println("==================================================");
      Serial.printf("Wi-Fi 状态: %s (代码=%d)\n",
                    (WiFi.status() == WL_CONNECTED) ? "已连接" : "未连接",
                    (int)WiFi.status());
      Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
      Serial.printf("IP 地址: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("信号强度: %d dBm\n", WiFi.RSSI());
      const WeatherInfo& winfo = weather_get_info();
      Serial.printf("天气状态: [%s] %s, %s, %s (有效=%s)\n",
                    winfo.city, winfo.temp, winfo.desc, winfo.humidity,
                    winfo.valid ? "YES" : "NO");
      Serial.println("==================================================");
    } else if (cmd.equalsIgnoreCase("WEATHER")) {
      Serial.println("[WIFI] 强制手动刷新天气...");
      weather_fetch();
    } else if (cmd.startsWith("WIFI:") || cmd.startsWith("wifi:")) {
      String params = cmd.substring(5);
      params.trim();
      int comma = params.indexOf(',');
      if (comma > 0) {
        String s = params.substring(0, comma);
        String p = params.substring(comma + 1);
        s.trim();
        p.trim();
        net_set_wifi(s.c_str(), p.c_str());
      } else {
        net_set_wifi(params.c_str(), "");
      }
    }
  }
}

void net_update() {
  handleSerialCommands();

  // 周期性检查连接状态
  if (millis() - s_lastWifiCheck >= 2000) {
    s_lastWifiCheck = millis();
    bool connected = (WiFi.status() == WL_CONNECTED);

    if (connected && !s_wasConnected) {
      s_wasConnected = true;
      Serial.printf("[WIFI] Wi-Fi 连接成功! IP 地址: %s (SSID: %s)\n",
                    WiFi.localIP().toString().c_str(), WiFi.SSID().c_str());
      net_sync_time();
      weather_fetch();
    } else if (!connected && s_wasConnected) {
      s_wasConnected = false;
      Serial.println("[WIFI] Wi-Fi 连接断开");
    }

    if (connected) {
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

String net_get_ssid() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  }
  return "未连接";
}

void net_start_config_portal() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);
  font_draw_string_center(160, 24, "进入 Wi-Fi 智能配网模式", COLOR_ACCENT_GREEN, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 56, "1. 手机连接热点: Bailan_Clock", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 80, "热点无密码，直接连接即可", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 108, "2. 打开手机浏览器输入网址:", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 132, "192.168.4.1", COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 156, "选择家中 WiFi 并输入密码", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 184, "3. 也可串口发送 WIFI:名称,密码", COLOR_ACCENT_ORANGE, COLOR_BG_DARK, 1);
  font_draw_string_center(160, 214, "配网超时时间: 90 秒", COLOR_TEXT_MUTED, COLOR_BG_DARK, 1);

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(90); // 90 秒超时退出
  wifiManager.startConfigPortal("Bailan_Clock");

  if (WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(COLOR_BG_DARK);
    font_draw_string_center(160, 90, "Wi-Fi 连接成功！", COLOR_ACCENT_GREEN, COLOR_BG_DARK, 1);
    font_draw_string_center(160, 120, "正在同步北京时间与富平县天气...", COLOR_ACCENT_CYAN, COLOR_BG_DARK, 1);
    net_sync_time();
    weather_fetch();
    delay(1200);
  }
}
