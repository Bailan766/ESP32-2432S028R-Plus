#include "weather.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

static WeatherInfo s_weather = {
  "--", "待同步", "--", "自动定位", false, 0
};

static unsigned long s_lastAttemptTime = 0;
static const unsigned long WEATHER_RETRY_INTERVAL = 30000;          // 失败后 30 秒冷却重试，杜绝主循环卡顿
static const unsigned long WEATHER_UPDATE_INTERVAL = 20 * 60 * 1000UL; // 成功后 20 分钟自动更新一次

// WMO 标准天气代码中文映射
static const char* wmoCodeToCondition(int code) {
  switch (code) {
    case 0: return "晴朗";
    case 1: return "大部晴朗";
    case 2: return "少云多云";
    case 3: return "阴天多云";
    case 45: return "有雾";
    case 48: return "雾凇浓雾";
    case 51: return "毛毛细雨";
    case 53: return "中度毛雨";
    case 55: return "密毛毛雨";
    case 56: case 57: return "冻毛毛雨";
    case 61: return "小雨";
    case 63: return "中雨";
    case 65: return "大雨";
    case 66: case 67: return "冻雨";
    case 71: return "小雪";
    case 73: return "中雪";
    case 75: return "大雪";
    case 77: return "细雪微粒";
    case 80: return "阵雨";
    case 81: return "强阵雨";
    case 82: return "暴烈阵雨";
    case 85: case 86: return "阵雪";
    case 95: return "雷阵雨";
    case 96: case 99: return "雷暴冰雹";
    default: return "晴天";
  }
}

static const char* translateCondition(const char* raw) {
  if (strstr(raw, "Sunny") || strstr(raw, "Clear")) return "晴朗";
  if (strstr(raw, "Partly") || strstr(raw, "cloudy") || strstr(raw, "Cloudy")) return "多云";
  if (strstr(raw, "Overcast")) return "阴天";
  if (strstr(raw, "rain") || strstr(raw, "Rain") || strstr(raw, "Drizzle")) return "小雨";
  if (strstr(raw, "Heavy rain") || strstr(raw, "Shower")) return "阵雨";
  if (strstr(raw, "snow") || strstr(raw, "Snow")) return "降雪";
  if (strstr(raw, "Fog") || strstr(raw, "Mist")) return "雾霾";
  if (strstr(raw, "Thunder")) return "雷雨";
  return raw;
}

// 方案 1: 高速双重 API (ip-api.com 获取中文城市与经纬度 + Open-Meteo 获取气象)
static bool fetchViaOpenMeteo() {
  HTTPClient http;
  http.setTimeout(3500);

  // 默认备用经纬度 (北京)
  float lat = 39.9042f;
  float lon = 116.4074f;
  String cityName = "自动定位";

  // 1. IP 定位城市与经纬度 (免 Key，返回纯中文城市名，全球自适应)
  http.begin("http://ip-api.com/json/?lang=zh-CN");
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err && doc["status"] == "success") {
      lat = doc["lat"] | 39.9042f;
      lon = doc["lon"] | 116.4074f;
      const char* c = doc["city"];
      if (c && strlen(c) > 0) {
        cityName = c;
      }
    }
  }
  http.end();

  // 2. Open-Meteo 高速天气 API (全球免 Key，HTTP 直连，响应低于 500ms)
  char url[180];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,weather_code&timezone=auto",
           lat, lon);

  http.begin(url);
  code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err && doc.containsKey("current")) {
      float temp = doc["current"]["temperature_2m"];
      int hum = doc["current"]["relative_humidity_2m"];
      int wcode = doc["current"]["weather_code"];

      // 截取并精简城市名称（不超过 12 个字节，避免卡片布局溢出）
      if (cityName.length() > 12) {
        cityName = cityName.substring(0, 12);
      }
      strncpy(s_weather.city, cityName.c_str(), sizeof(s_weather.city) - 1);
      s_weather.city[sizeof(s_weather.city) - 1] = '\0';

      snprintf(s_weather.temp, sizeof(s_weather.temp), "%.0f°C", temp);
      strncpy(s_weather.desc, wmoCodeToCondition(wcode), sizeof(s_weather.desc) - 1);
      s_weather.desc[sizeof(s_weather.desc) - 1] = '\0';

      snprintf(s_weather.humidity, sizeof(s_weather.humidity), "湿度:%d%%", hum);

      s_weather.valid = true;
      s_weather.last_update = millis();
      http.end();
      LOG_UI("天气获取成功 [Open-Meteo]: [%s] %s, %s, %s",
             s_weather.city, s_weather.temp, s_weather.desc, s_weather.humidity);
      return true;
    }
  }
  http.end();
  return false;
}

// 方案 2: wttr.in 备用降级方案
static bool fetchViaWttrIn() {
  HTTPClient http;
  http.setTimeout(3500);
  http.begin("http://wttr.in/?format=%l|%t|%C|%h&m");
  http.setUserAgent("curl/7.68.0");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();

    int idx1 = payload.indexOf('|');
    int idx2 = (idx1 >= 0) ? payload.indexOf('|', idx1 + 1) : -1;
    int idx3 = (idx2 >= 0) ? payload.indexOf('|', idx2 + 1) : -1;

    if (idx1 > 0 && idx2 > idx1) {
      String city = payload.substring(0, idx1);
      String temp = payload.substring(idx1 + 1, idx2);
      String desc = (idx3 > idx2) ? payload.substring(idx2 + 1, idx3) : payload.substring(idx2 + 1);
      String hum = (idx3 > idx2) ? payload.substring(idx3 + 1) : "";

      city.trim();
      temp.trim();
      desc.trim();
      hum.trim();

      int comma = city.indexOf(',');
      if (comma > 0) city = city.substring(0, comma);
      if (city.length() > 12) city = city.substring(0, 12);

      strncpy(s_weather.city, city.c_str(), sizeof(s_weather.city) - 1);
      s_weather.city[sizeof(s_weather.city) - 1] = '\0';

      strncpy(s_weather.temp, temp.c_str(), sizeof(s_weather.temp) - 1);
      s_weather.temp[sizeof(s_weather.temp) - 1] = '\0';

      const char* cnDesc = translateCondition(desc.c_str());
      strncpy(s_weather.desc, cnDesc, sizeof(s_weather.desc) - 1);
      s_weather.desc[sizeof(s_weather.desc) - 1] = '\0';

      if (hum.length() > 0) {
        snprintf(s_weather.humidity, sizeof(s_weather.humidity), "湿度:%s", hum.c_str());
      } else {
        strcpy(s_weather.humidity, "");
      }

      s_weather.valid = true;
      s_weather.last_update = millis();
      http.end();
      LOG_UI("天气获取成功 [wttr.in]: [%s] %s, %s, %s",
             s_weather.city, s_weather.temp, s_weather.desc, s_weather.humidity);
      return true;
    }
  }
  http.end();
  return false;
}

void weather_init() {
  s_weather.valid = false;
  s_lastAttemptTime = 0;
}

bool weather_fetch() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_UI("Wi-Fi 未连接，跳过天气获取");
    return false;
  }

  s_lastAttemptTime = millis();
  LOG_UI("正在从网络获取实时天气信息 (双引擎自适应)...");

  // 优先执行高速 Open-Meteo + IP 自动定位
  if (fetchViaOpenMeteo()) {
    return true;
  }

  // 备用降级方案: wttr.in
  LOG_UI("首选气象源暂未响应，切换备用源 wttr.in...");
  if (fetchViaWttrIn()) {
    return true;
  }

  LOG_UI("所有气象源均获取失败，稍后将自动重试");
  return false;
}

void weather_update() {
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long now = millis();
    if (!s_weather.valid) {
      // 失败后等待至少 30 秒再尝试，杜绝主循环高频阻塞
      if (s_lastAttemptTime == 0 || (now - s_lastAttemptTime >= WEATHER_RETRY_INTERVAL)) {
        weather_fetch();
      }
    } else {
      // 成功后每 20 分钟定期刷新
      if (now - s_weather.last_update >= WEATHER_UPDATE_INTERVAL) {
        weather_fetch();
      }
    }
  }
}

const WeatherInfo& weather_get_info() {
  return s_weather;
}
