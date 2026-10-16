// ==============================================================================
// Bailan MP3 Player - 主入口程序 (v0.1.0)
// 适配硬件: ESP32-2432S028R Plus 一体化开发板 (CYD Plus)
// 架构: 模块化分层驱动 (Hardware -> Storage -> Player -> System -> UI)
// ==============================================================================

#include <Arduino.h>
#include "config.h"

// 硬件驱动层
#include "hardware/display.h"
#include "hardware/touch.h"
#include "hardware/audio.h"
#include "hardware/amplifier.h"
#include "hardware/battery.h"
#include "hardware/rgb.h"
#include "hardware/buttons.h"

// 存储管理层
#include "storage/sdcard.h"
#include "storage/database.h"
#include "storage/scanner.h"

// 播放引擎层
#include "player/player.h"

// 系统服务层
#include "system/settings_manager.h"
#include "system/power_manager.h"
#include "system/diagnostics.h"
#include "system/net_manager.h"

// 用户交互层
#include "ui/ui.h"

// 触摸去抖定时
static unsigned long s_lastTouchProcessed = 0;

void setup() {
  // 1. 初始化串口调试 (115200 波特率)
  Serial.begin(115200);
  delay(150);
  LOG_BOOT("==================================================");
  LOG_BOOT("  %s %s %s 启动中...", PROJECT_NAME, PROJECT_VERSION, PROJECT_EDITION);
  LOG_BOOT("  开发者: %s", PROJECT_AUTHOR);
  LOG_BOOT("  硬件目标: %s", HARDWARE_REVISION);
  LOG_BOOT("  编译时间: %s", FIRMWARE_BUILD_DATE);
  LOG_BOOT("==================================================");

  // 2. 初始化持久化配置系统 (NVS / Preferences)
  settings_init();

  // 3. 初始化板载 RGB 状态指示灯
  rgb_init();
  rgb_set_state(RGB_BOOT); // 开机点亮青蓝提示

  // 4. 初始化实体按键 (BOOT GPIO 0)
  button_init();

  // 5. 初始化功放与音频 PWM (GPIO 4 / GPIO 26)
  audio_init();
  audio_set_volume(settings_get().volume);

  // 6. 初始化电池 ADC (GPIO 34)
  battery_init();

  // 7. 初始化 LCD 显示与背光 (ST7789, GPIO 14/13/15/2/21)
  display_init();
  display_set_rotation(settings_get().screenRotation);
  display_set_brightness(settings_get().brightness);

  // 8. 初始化电阻式触摸屏 (XPT2046 软件 SPI)
  touch_init();
  touch_set_rotation(settings_get().screenRotation);

  // 9. 初始化存储与 TF 卡扫描 (HSPI: GPIO 18/19/23/5)
  sdcard_init();
  scanner_init();

  // 10. 初始化播放引擎与最近播放
  player_init();
  player_set_mode((PlayMode)settings_get().playMode);

  // 11. 初始化电源管理与运行诊断
  power_init();
  diagnostics_init();

  // 12. 初始化 UI 并展示启动画面后进入主菜单
  net_init();
  ui_init();

  // 恢复 RGB 为默认就绪
  rgb_set_state(RGB_OFF);
  LOG_BOOT("系统全部底层服务初始化完成，正式进入主运行循环!");
}

void loop() {
  // 1. 触摸事件处理 (工业级防连击、防穿透、两帧稳定判定状态机)
  if (ui_get_current_page() != UI_PAGE_TOUCH_CAL) {
    static bool s_touchWasDown = false;
    static int16_t s_firstX = -1, s_firstY = -1;
    static unsigned long s_firstTouchTime = 0;
    static bool s_clickFired = false;

    int16_t curX, curY;
    bool isDown = touch_read(curX, curY);

    if (isDown) {
      power_reset_inactivity_timer();

      // 修正手指未抬起时的睡眠唤醒死循环
      if (power_is_screen_sleeping() && !s_touchWasDown) {
        power_wake_screen();
        s_clickFired = true;
        s_touchWasDown = true;
      } else if (!power_is_screen_sleeping()) {
        if (!s_touchWasDown) {
          // 初次检测到按下接触，记录第一帧坐标与时间戳
          s_touchWasDown = true;
          s_firstX = curX;
          s_firstY = curY;
          s_firstTouchTime = millis();
          s_clickFired = false;
        } else if (!s_clickFired) {
          // 触笔持续按下中：等待接触机械弹跳稳定 (持续 >= 20ms，且坐标在 25px 抖动半径内)
          if (millis() - s_firstTouchTime >= 20) {
            if (abs(curX - s_firstX) < 25 && abs(curY - s_firstY) < 25) {
              s_clickFired = true; // 单次物理按下仅触发一次点击，彻底防止穿透击穿后续页面
              ui_handle_touch(curX, curY);
            } else {
              // 移动过大（可能是在滑动或边缘晃动），更新基准坐标
              s_firstX = curX;
              s_firstY = curY;
              s_firstTouchTime = millis();
            }
          }
        }
      }
    } else {
      // 触笔/手指已完全抬起离开屏幕，释放状态机，准备接收下一次全新点击
      s_touchWasDown = false;
      s_clickFired = false;
      s_firstX = -1;
      s_firstY = -1;
    }
  }

  // 2. 实体按键轮询检测 (BOOT 短按/长按)
  ButtonEvent btnEvt = button_update();
  if (btnEvt != BTN_EVENT_NONE) {
    power_reset_inactivity_timer();
    if (btnEvt == BTN_EVENT_CLICK) {
      ui_handle_button_click();
    } else if (btnEvt == BTN_EVENT_LONG_PRESS) {
      ui_handle_button_long_press();
    }
  }

  // 3. 音乐播放引擎步进 (非阻塞音符调度)
  player_update();

  // 4. 音频辅助定时处理 (测试音自动停止)
  audio_update();

  // 5. RGB 灯效动画刷新 (呼吸/律动)
  rgb_update();

  // 6. 电池电量平滑采样 (每 1 秒)
  battery_update();

  // 7. 电源管理 (自动息屏倒计时检测)
  power_update();

  // 8. UI 界面动态内容刷新 (频谱跳动、进度更新)
  ui_update();

  // 9. 竞赛级诊断信息上报 (串口统计输出)
  diagnostics_update();

  // 10. 网络与天气服务状态机更新
  net_update();

  // 11. 释放极微弱时间片保证看门狗与后台任务运行
  delay(4);
}