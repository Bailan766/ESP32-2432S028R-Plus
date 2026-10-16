#include "delta_pwd_ui.h"
#include "ui.h"
#include "hardware/display.h"
#include "ui/font_cn.h"
#include "system/net_manager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static String s_infoLines[10];
static int s_infoCount = 0;
static bool s_loaded = false;

static void renderDeltaUI() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);
  ui_draw_header("三角洲每日密码", true); // Includes back button

  tft.fillRoundRect(10, 40, 300, 150, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(10, 40, 300, 150, 5, COLOR_PANEL_BORDER);

  int y = 50;
  for (int i = 0; i < s_infoCount; i++) {
    font_draw_string(20, y, s_infoLines[i].c_str(), TFT_WHITE, COLOR_PANEL_BG, 1);
    y += 20;
    if (y > 170) break; // Screen space limits
  }

  // Refresh button
  tft.fillRoundRect(100, 200, 120, 30, 4, 0x10A2);
  tft.drawRoundRect(100, 200, 120, 30, 4, COLOR_ACCENT_CYAN);
  font_draw_string_center(160, 207, "重新获取", COLOR_ACCENT_CYAN, 0x10A2, 1);
}

void delta_ui_enter() {
  s_infoCount = 1;
  s_infoLines[0] = "正在连接...";
  s_loaded = false;
  renderDeltaUI();
}

void delta_ui_update() {
  if (!s_loaded) {
    if (!net_is_connected()) {
      s_infoCount = 1;
      s_infoLines[0] = "错误: Wi-Fi 未连接";
    } else {
      s_infoCount = 1;
      s_infoLines[0] = "正在获取最新数据...";
      renderDeltaUI(); // Give visual feedback of request start

      HTTPClient http;
      http.begin("https://api.suyanw.cn/api/delta_mima.php?type=json");
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, payload);
        s_infoCount = 0;
        
        if (!err) {
          if (doc.containsKey("error")) {
            s_infoLines[s_infoCount++] = "服务器返回:";
            s_infoLines[s_infoCount++] = doc["error"].as<String>();
          } else if (doc.containsKey("data")) {
            s_infoLines[s_infoCount++] = "今日密码获取成功:";
            JsonObject dataObj = doc["data"].as<JsonObject>();
            for (JsonPair kv : dataObj) {
              if (s_infoCount < 10) {
                s_infoLines[s_infoCount++] = String(kv.key().c_str()) + ": " + kv.value().as<String>();
              }
            }
          } else {
            s_infoLines[s_infoCount++] = "JSON格式未识别";
          }
        } else {
          s_infoLines[s_infoCount++] = "解析JSON失败";
        }
      } else {
        s_infoCount = 1;
        s_infoLines[0] = "请求失败: " + String(http.errorToString(httpCode).c_str());
      }
      http.end();
    }
    s_loaded = true;
    renderDeltaUI();
  }
}

void delta_ui_handle_touch(int16_t x, int16_t y) {
  // Back
  if (x >= 0 && x <= 95 && y >= 0 && y <= 32) {
    ui_switch_to(UI_PAGE_HOME);
    return;
  }
  // Reload
  if (x >= 100 && x <= 220 && y >= 200 && y <= 230) {
    s_loaded = false;
  }
}
