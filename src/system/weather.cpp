#include "weather.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"

static WeatherInfo s_weather = {
  "--", "待同步", "--", "自动定位", false, 0
};

static unsigned long s_lastWeatherCheck = 0;

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

void weather_init() {
  s_weather.valid = false;
}

bool weather_fetch() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_UI("Wi-Fi 未连接，跳过天气获取");
    return false;
  }

  LOG_UI("正在从网络获取实时天气信息...");
  HTTPClient http;
  http.setTimeout(4000);
  // wttr.in 格式: 城市|温度|天气状态|湿度, &m 代表公制摄氏度
  http.begin("http://wttr.in/?format=%l|%t|%C|%h&m");
  http.setUserAgent("curl/7.68.0");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    LOG_UI("天气原始响应: %s", payload.c_str());

    // 解析城市、温度、天气描述、湿度
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

      // 截取城市名（如果包含长省份/国家名）
      int comma = city.indexOf(',');
      if (comma > 0) city = city.substring(0, comma);

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
      LOG_UI("天气解析成功: [%s] %s, %s, %s", s_weather.city, s_weather.temp, s_weather.desc, s_weather.humidity);
      return true;
    }
  } else {
    LOG_UI("天气 HTTP 请求失败, 状态码: %d", httpCode);
  }

  http.end();
  return false;
}

void weather_update() {
  if (WiFi.status() == WL_CONNECTED) {
    // 首次开机获取，或每隔 20 分钟自动更新一次天气
    if (!s_weather.valid || (millis() - s_weather.last_update >= 20 * 60 * 1000UL)) {
      weather_fetch();
    }
  }
}

const WeatherInfo& weather_get_info() {
  return s_weather;
}
