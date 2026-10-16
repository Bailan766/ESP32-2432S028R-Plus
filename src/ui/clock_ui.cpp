#include "clock_ui.h"
#include "ui.h"
#include "hardware/display.h"
#include "font_cn.h"
#include "../wallpaper.h"
#include <time.h>

static bool s_isUnlocking = false;
static char s_pinBuffer[5] = {0};
static int s_pinLen = 0;
static const char* CORRECT_PIN = "2333";

static void drawWallpaper() {
  TFT_eSPI& tft = display_get_tft();
  tft.setSwapBytes(false);
  tft.pushImage(0, 0, 320, 240, wallpaper_data);
}

static void renderClockView() {
  drawWallpaper();
  TFT_eSPI& tft = display_get_tft();
  
  // Design requested: Clock in top left area
  // We use standard fonts for large numbers
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 10);
  
  char timeStr[16];
  if (timeValid) {
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
  } else {
    strcpy(timeStr, "00:00"); // default
  }

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(7); // Font 7 is a large 7-segment style, looks cool for clocks
  // font 7 is quite large, let's test. If too large, font 4 is smoother and medium
  // wait, TFT_eSPI font 7: 48 pixels high, 7-segment.
  tft.drawString(timeStr, 20, 20);
  
  if (timeValid) {
    char dateStr[32];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d  %A", &timeinfo);
    tft.setTextFont(2);
    tft.drawString(dateStr, 20, 80);
  } else {
    font_draw_string(20, 80, "未同步时间 (点击解锁)", TFT_LIGHTGREY, TFT_BLACK, 1);
  }
  
  // Draw sleep button in top right
  tft.fillRoundRect(260, 15, 50, 30, 4, TFT_DARKGREY);
  font_draw_string(269, 23, "息屏", TFT_WHITE, TFT_DARKGREY, 1);
}

static void renderUnlockView() {
  TFT_eSPI& tft = display_get_tft();
  tft.fillScreen(COLOR_BG_DARK);
  font_draw_string_center(160, 20, "请输入密码解锁", COLOR_TEXT_PRIMARY, COLOR_BG_DARK, 1);
  
  // Draw PIN dots
  int dotStartX = 160 - (4 * 24) / 2;
  for (int i=0; i<4; i++) {
    if (i < s_pinLen) {
      tft.fillCircle(dotStartX + i*24 + 12, 50, 6, COLOR_ACCENT_CYAN);
    } else {
      tft.drawCircle(dotStartX + i*24 + 12, 50, 6, COLOR_PANEL_BORDER);
    }
  }

  // Draw Keypad (3x4)
  const char* keys[12] = {
    "1", "2", "3",
    "4", "5", "6",
    "7", "8", "9",
    "C", "0", "OK"
  };
  
  for(int r=0; r<4; r++) {
    for(int c=0; c<3; c++) {
      int idx = r*3 + c;
      int x = 40 + c * 80;
      int y = 70 + r * 40;
      tft.fillRoundRect(x, y, 70, 35, 5, COLOR_PANEL_BG);
      tft.drawRoundRect(x, y, 70, 35, 5, COLOR_PANEL_BORDER);
      
      // text
      tft.setTextFont(2);
      tft.setTextColor(COLOR_TEXT_PRIMARY);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(keys[idx], x + 35, y + 17);
    }
  }
}

void clock_ui_enter() {
  s_isUnlocking = false;
  s_pinLen = 0;
  renderClockView();
}

void clock_ui_update() {
  if (!s_isUnlocking) {
    static unsigned long lastTick = 0;
    if (millis() - lastTick > 1000) {
      lastTick = millis();
      // Only re-render if second is 0 to avoid flickering wallpaper? 
      // Actually we can re-render every minute, since the clock shows HH:MM
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 10)) {
        static int lastMin = -1;
        if (timeinfo.tm_min != lastMin) {
          lastMin = timeinfo.tm_min;
          renderClockView();
        }
      }
    }
  }
}

void clock_ui_handle_touch(int16_t x, int16_t y) {
  if (!s_isUnlocking) {
    if (x >= 250 && y <= 60) {
      display_sleep();
    } else {
      s_isUnlocking = true;
      s_pinLen = 0;
      renderUnlockView();
    }
    return;
  }
  
  // Handle keypad
  for(int r=0; r<4; r++) {
    for(int c=0; c<3; c++) {
      int kx = 40 + c * 80;
      int ky = 70 + r * 40;
      if (x >= kx && x <= kx+70 && y >= ky && y <= ky+35) {
        int idx = r*3 + c;
        if (idx == 9) { // 'C'
          if (s_pinLen > 0) s_pinLen--;
        } else if (idx == 11) { // 'OK'
          if (s_pinLen == 4) {
            if (strcmp(s_pinBuffer, CORRECT_PIN) == 0) {
              ui_switch_to(UI_PAGE_HOME);
              return;
            } else {
              s_pinLen = 0; // Wrong pin
            }
          }
        } else { // 0-9
          if (s_pinLen < 4) {
            if (idx == 10) s_pinBuffer[s_pinLen] = '0';
            else s_pinBuffer[s_pinLen] = '1' + idx;
            s_pinLen++;
          }
        }
        s_pinBuffer[s_pinLen] = '\0';
        renderUnlockView();
        
        if (s_pinLen == 4 && strcmp(s_pinBuffer, CORRECT_PIN) == 0) {
           delay(150); 
           ui_switch_to(UI_PAGE_HOME);
        }
        return;
      }
    }
  }
}
