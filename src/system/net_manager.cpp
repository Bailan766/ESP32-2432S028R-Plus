#include "net_manager.h"
#include <WiFiManager.h>
#include <Preferences.h>
#include <time.h>
#include <SD.h>
#include "hardware/display.h"
#include "ui/font_cn.h"
#include "weather.h"
#include "config.h"
#include "hardware/audio.h"
#include "hardware/amplifier.h"
#include "player/player.h"
#include <driver/dac.h>

static bool s_timeSynced = false;
static unsigned long s_lastWifiCheck = 0;
static bool s_wasConnected = false;
static Preferences s_prefs;

// ---------------- 省电：Wi-Fi 射频按需开关 ----------------
// 联网完成 NTP + 天气同步后关闭射频，之后每 30 分钟短暂唤醒一次刷新天气。
// 实测 Wi-Fi 常开时待机电流约 100~120mA，关射频后可降到 20mA 量级。
#define NET_RADIO_KEEPALIVE_MS   25000UL              // 联网后保持射频的时间
#define NET_WEATHER_REFRESH_MS   (30UL * 60UL * 1000UL) // 天气刷新周期：30 分钟

static bool s_powerSave = true;
static bool s_radioOn = true;
static bool s_btSuspended = false;
static unsigned long s_radioOffAt = 0;   // 到点关闭射频
static unsigned long s_lastRefresh = 0;  // 上次联网刷新时间
static uint8_t s_connectRetry = 0;       // 未联网时的射频续期次数

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
  WiFi.setSleep(true);   // 开启 modem sleep，空闲时进一步省电
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

  s_radioOn = true;
  s_radioOffAt = millis() + NET_RADIO_KEEPALIVE_MS;
  s_lastRefresh = millis();

  checkSdCardWifiFile();
}

// ---------------- 省电：射频开关 ----------------

void net_set_power_save(bool enable) {
  s_powerSave = enable;
  LOG_SYS("Wi-Fi 省电模式: %s", enable ? "开启" : "关闭");
  if (!enable) {
    // 关闭省电时确保射频处于工作状态
    net_wake_radio(NET_RADIO_KEEPALIVE_MS);
  }
}

bool net_get_power_save() {
  return s_powerSave;
}

bool net_radio_is_on() {
  return s_radioOn;
}

void net_suspend_for_bt() {
  s_btSuspended = true;
  if (s_radioOn || WiFi.getMode() != WIFI_OFF) {
    LOG_SYS("蓝牙音箱模式启动: 挂起并关闭 Wi-Fi 射频，释放 2.4GHz 天线与堆内存");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    s_radioOn = false;
    s_wasConnected = false;
    s_radioOffAt = 0;
    s_connectRetry = 0;
  }
}

void net_resume_after_bt() {
  s_btSuspended = false;
  s_lastRefresh = millis();
}

void net_wake_radio(uint32_t keepMs) {
  if (s_btSuspended) return;
  if (s_radioOn) {
    s_radioOffAt = millis() + keepMs;
    return;
  }

  LOG_SYS("开启 Wi-Fi 射频 (保持 %lu ms)", (unsigned long)keepMs);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);

  s_prefs.begin("wifi_cfg", true);
  String ssid = s_prefs.getString("ssid", "");
  String pass = s_prefs.getString("pass", "");
  s_prefs.end();

  if (ssid.length() > 0) {
    WiFi.begin(ssid.c_str(), pass.c_str());
  } else {
    WiFi.begin();
  }

  s_radioOn = true;
  s_wasConnected = false;
  s_connectRetry = 0;
  s_radioOffAt = millis() + keepMs;
  s_lastWifiCheck = 0;
}

static void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("BEEP") || cmd.equalsIgnoreCase("TEST")) {
      Serial.println("[CMD] 播放测试音 1000Hz 500ms (GPIO4=LOW, GPIO26=DAC2)...");
      audio_test_beep(1000, 500);
      Serial.println("[CMD] 测试音完成");
    } else if (cmd.equalsIgnoreCase("PLAY")) {
      Serial.println("[CMD] 执行播放 player_play()...");
      player_play();
      Serial.printf("[CMD] 播放器状态: %s\n", player_state_to_string(player_get_state()));
    } else if (cmd.equalsIgnoreCase("STOP")) {
      Serial.println("[CMD] 执行停止 player_stop()...");
      player_stop();
    } else if (cmd.equalsIgnoreCase("NEXT")) {
      Serial.println("[CMD] 执行下一曲 player_next()...");
      player_next();
    } else if (cmd.equalsIgnoreCase("PREV")) {
      Serial.println("[CMD] 执行上一曲 player_prev()...");
      player_prev();
    } else if (cmd.startsWith("SONG:") || cmd.startsWith("song:")) {
      int idx = cmd.substring(5).toInt();
      Serial.printf("[CMD] 播放曲目 [%d]...\n", idx);
      player_play_track(idx);
    } else if (cmd.startsWith("VOL:") || cmd.startsWith("vol:")) {
      int vol = cmd.substring(4).toInt();
      audio_set_volume(vol);
      Serial.printf("[CMD] 设置音量为 %d%%\n", audio_get_volume());
    } else if (cmd.equalsIgnoreCase("AMP:LOW")) {
      Serial.println("[CMD] 强制 GPIO4 = LOW (开启 8002 功放)...");
      pinMode(AMP_ENABLE_PIN, OUTPUT);
      digitalWrite(AMP_ENABLE_PIN, LOW);
    } else if (cmd.equalsIgnoreCase("AMP:HIGH")) {
      Serial.println("[CMD] 强制 GPIO4 = HIGH (关闭 8002 功放)...\n");
      pinMode(AMP_ENABLE_PIN, OUTPUT);
      digitalWrite(AMP_ENABLE_PIN, HIGH);
    } else if (cmd.equalsIgnoreCase("TONE:HIGH")) {
      Serial.println("[CMD] 测试反向极性: GPIO4 = HIGH 状态下蜂鸣 1000Hz 500ms...");
      pinMode(AMP_ENABLE_PIN, OUTPUT);
      digitalWrite(AMP_ENABLE_PIN, HIGH);
      dac_output_enable(DAC_CHANNEL_2);
      unsigned long st = millis();
      while (millis() - st < 500) {
        dac_output_voltage(DAC_CHANNEL_2, 128 + 45);
        delayMicroseconds(500);
        dac_output_voltage(DAC_CHANNEL_2, 128 - 45);
        delayMicroseconds(500);
      }
      digitalWrite(AMP_ENABLE_PIN, LOW);
      Serial.println("[CMD] 反向极性测试完成");
    } else if (cmd.equalsIgnoreCase("SCAN")) {
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
      net_wake_radio(NET_RADIO_KEEPALIVE_MS);
      weather_fetch();
    } else if (cmd.equalsIgnoreCase("SAVE") || cmd.equalsIgnoreCase("WIFI:SAVE")) {
      net_set_power_save(true);
    } else if (cmd.equalsIgnoreCase("ALWAYS") || cmd.equalsIgnoreCase("WIFI:ALWAYS")) {
      net_set_power_save(false);
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
  if (s_btSuspended) return;

  const unsigned long now = millis();

  if (s_radioOn) {
    // 周期性检查连接状态
    if (now - s_lastWifiCheck >= 2000) {
      s_lastWifiCheck = now;
      bool connected = (WiFi.status() == WL_CONNECTED);

      if (connected && !s_wasConnected) {
        s_wasConnected = true;
        Serial.printf("[WIFI] Wi-Fi 连接成功! IP 地址: %s (SSID: %s)\n",
                      WiFi.localIP().toString().c_str(), WiFi.SSID().c_str());
        net_sync_time();
        weather_fetch();
        s_lastRefresh = now;
        s_radioOffAt = now + NET_RADIO_KEEPALIVE_MS;
        s_connectRetry = 0;
      } else if (!connected && s_wasConnected) {
        s_wasConnected = false;
        Serial.println("[WIFI] Wi-Fi 连接断开");
      }

      if (connected) {
        weather_update();
      }
    }

    // 到点关射频：若一直没连上，先续期两次（共约 75 秒）再放弃，避免白白错过同步
    if (s_powerSave && s_radioOffAt != 0 && (long)(now - s_radioOffAt) >= 0) {
      if (!s_wasConnected && s_connectRetry < 2) {
        s_connectRetry++;
        LOG_SYS("省电: 尚未联网，射频续期 %d/2 (再等 25 秒)", s_connectRetry);
        s_radioOffAt = now + NET_RADIO_KEEPALIVE_MS;
      } else {
        LOG_SYS("省电: 关闭 Wi-Fi 射频 (待机电流下降)");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        s_radioOn = false;
        s_wasConnected = false;
        s_radioOffAt = 0;
        s_connectRetry = 0;
      }
    }
  } else {
    // 射频关闭中：按周期短暂唤醒刷新天气与时间
    if (s_powerSave && (now - s_lastRefresh >= NET_WEATHER_REFRESH_MS)) {
      LOG_SYS("省电: 周期性唤醒 Wi-Fi 刷新天气");
      s_lastRefresh = now;
      net_wake_radio(NET_RADIO_KEEPALIVE_MS);
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

  // 配网结束后重新开始省电计时
  s_radioOn = true;
  s_radioOffAt = millis() + NET_RADIO_KEEPALIVE_MS;
  s_lastRefresh = millis();
}
