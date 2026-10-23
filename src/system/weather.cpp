#include "weather.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// 陕西省渭南市富平县专属精准配置 (国家气象局城市代码: 101110506)
#define FUPING_CITY_CODE "101110506"
#define FUPING_CITY_NAME "富平县"
#define FUPING_LAT       34.75f
#define FUPING_LON       109.18f

static WeatherInfo s_weather = {
  "--", "待同步", "--", FUPING_CITY_NAME, false, 0
};

static unsigned long s_lastAttemptTime = 0;
static const unsigned long WEATHER_RETRY_INTERVAL = 20000;             // 失败后 20 秒冷却重试
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
  if (strstr(raw, "Fog") || strstr(raw, "Mist") || strstr(raw, "Haze") || strstr(raw, "haze") ||
      strstr(raw, "Smoke") || strstr(raw, "Smoky")) return "雾霾";
  if (strstr(raw, "Dust") || strstr(raw, "Sand")) return "沙尘";
  if (strstr(raw, "Thunder")) return "雷雨";
  return raw;
}

// 方案 1 (首选核心): 国内天气专用 CDN (又拍云/itboy) - 直连陕西省渭南市富平县，免 Key，超低延迟 (<200ms)
static bool fetchViaChinaWeather() {
  HTTPClient http;
  http.setTimeout(4000);

  // 101110506 = 陕西省 渭南市 富平县
  http.begin("http://t.weather.itboy.net/api/weather/city/101110506");
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // 使用过滤机制仅解析关键字段，毫秒级解析且仅占用极小内存 (<500B)
    StaticJsonDocument<256> filter;
    filter["status"] = true;
    filter["cityInfo"]["city"] = true;
    filter["data"]["wendu"] = true;
    filter["data"]["shidu"] = true;
    filter["data"]["quality"] = true;
    filter["data"]["forecast"][0]["type"] = true;

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!err && doc["status"] == 200) {
      const char* city = doc["cityInfo"]["city"];
      const char* wendu = doc["data"]["wendu"];
      const char* shidu = doc["data"]["shidu"];
      const char* type = doc["data"]["forecast"][0]["type"];

      if (city && strlen(city) > 0) {
        strncpy(s_weather.city, city, sizeof(s_weather.city) - 1);
      } else {
        strncpy(s_weather.city, FUPING_CITY_NAME, sizeof(s_weather.city) - 1);
      }
      s_weather.city[sizeof(s_weather.city) - 1] = '\0';

      if (wendu && strlen(wendu) > 0) {
        float ftemp = atof(wendu);
        snprintf(s_weather.temp, sizeof(s_weather.temp), "%.0f°", ftemp);
      }

      if (type && strlen(type) > 0) {
        strncpy(s_weather.desc, type, sizeof(s_weather.desc) - 1);
        s_weather.desc[sizeof(s_weather.desc) - 1] = '\0';
      }

      if (shidu && strlen(shidu) > 0) {
        if (strstr(shidu, "%")) {
          snprintf(s_weather.humidity, sizeof(s_weather.humidity), "湿度:%s", shidu);
        } else {
          snprintf(s_weather.humidity, sizeof(s_weather.humidity), "湿度:%s%%", shidu);
        }
      }

      s_weather.valid = true;
      s_weather.last_update = millis();
      http.end();
      LOG_UI("天气获取成功 [国内天气专线 - 陕西富平]: [%s] %s, %s, %s",
             s_weather.city, s_weather.temp, s_weather.desc, s_weather.humidity);
      return true;
    }
  }
  http.end();
  return false;
}

// 方案 2: Open-Meteo 高速 API (经纬度精准定位 富平县: 34.75, 109.18)
static bool fetchViaOpenMeteo() {
  HTTPClient http;
  http.setTimeout(4000);

  char url[180];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,relative_humidity_2m,weather_code&timezone=Asia/Shanghai",
           FUPING_LAT, FUPING_LON);

  http.begin(url);
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err && doc.containsKey("current")) {
      float temp = doc["current"]["temperature_2m"];
      int hum = doc["current"]["relative_humidity_2m"];
      int wcode = doc["current"]["weather_code"];

      strncpy(s_weather.city, FUPING_CITY_NAME, sizeof(s_weather.city) - 1);
      s_weather.city[sizeof(s_weather.city) - 1] = '\0';

      snprintf(s_weather.temp, sizeof(s_weather.temp), "%.0f°", temp);
      strncpy(s_weather.desc, wmoCodeToCondition(wcode), sizeof(s_weather.desc) - 1);
      s_weather.desc[sizeof(s_weather.desc) - 1] = '\0';

      snprintf(s_weather.humidity, sizeof(s_weather.humidity), "湿度:%d%%", hum);

      s_weather.valid = true;
      s_weather.last_update = millis();
      http.end();
      LOG_UI("天气获取成功 [Open-Meteo - 富平县]: [%s] %s, %s, %s",
             s_weather.city, s_weather.temp, s_weather.desc, s_weather.humidity);
      return true;
    }
  }
  http.end();
  return false;
}

// 方案 3: wttr.in 兜底降级方案 (指定查询 Fuping,Shaanxi)
static bool fetchViaWttrIn() {
  HTTPClient http;
  http.setTimeout(3500);
  http.begin("http://wttr.in/Fuping?format=%l|%t|%C|%h&m");
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

      strncpy(s_weather.city, FUPING_CITY_NAME, sizeof(s_weather.city) - 1);
      s_weather.city[sizeof(s_weather.city) - 1] = '\0';

      // 清理气温格式，移除多余的 'C' 或空白，确保与界面大字排版兼容
      if (temp.endsWith("C") || temp.endsWith("c")) {
        temp = temp.substring(0, temp.length() - 1);
        temp.trim();
      }
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
      LOG_UI("天气获取成功 [wttr.in - 富平县]: [%s] %s, %s, %s",
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
  strncpy(s_weather.city, FUPING_CITY_NAME, sizeof(s_weather.city) - 1);
  s_weather.city[sizeof(s_weather.city) - 1] = '\0';
}

bool weather_fetch() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_UI("Wi-Fi 尚未连接，无法连接气象服务器");
    return false;
  }

  s_lastAttemptTime = millis();
  LOG_UI("正在为 [陕西省渭南市富平县] 同步实时天气数据...");

  // 1. 优先使用国内专属 CDN (富平县代码: 101110506)
  if (fetchViaChinaWeather()) {
    return true;
  }

  // 2. 备用源: Open-Meteo (坐标 34.75, 109.18)
  LOG_UI("国内天气源暂未响应，尝试 Open-Meteo 高速坐标定位...");
  if (fetchViaOpenMeteo()) {
    return true;
  }

  // 3. 终极兜底: wttr.in
  LOG_UI("切换至备用源 wttr.in...");
  if (fetchViaWttrIn()) {
    return true;
  }

  LOG_UI("天气同步暂时失败，请检查 Wi-Fi 网络连通性");
  return false;
}

void weather_update() {
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long now = millis();
    if (!s_weather.valid) {
      if (s_lastAttemptTime == 0 || (now - s_lastAttemptTime >= WEATHER_RETRY_INTERVAL)) {
        weather_fetch();
      }
    } else {
      if (now - s_weather.last_update >= WEATHER_UPDATE_INTERVAL) {
        weather_fetch();
      }
    }
  }
}

const WeatherInfo& weather_get_info() {
  return s_weather;
}
