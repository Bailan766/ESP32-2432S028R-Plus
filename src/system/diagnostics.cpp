#include "diagnostics.h"
#include "config.h"
#include "hardware/battery.h"
#include "hardware/display.h"
#include "storage/sdcard.h"
#include "player/player.h"
#include "system/net_manager.h"
#include "system/power_manager.h"

static unsigned long s_lastDiagTime = 0;

void diagnostics_init() {
  s_lastDiagTime = millis();
}

uint32_t diagnostics_get_free_heap() {
  return ESP.getFreeHeap();
}

uint32_t diagnostics_get_min_free_heap() {
  return ESP.getMinFreeHeap();
}

uint32_t diagnostics_get_uptime_sec() {
  return millis() / 1000;
}

void diagnostics_dump_now() {
  LOG_SYS("----------------- 竞赛级系统实时运行快照 -----------------");
  LOG_SYS("运行时间: %lu s | CPU 主频: %d MHz", diagnostics_get_uptime_sec(), ESP.getCpuFreqMHz());
  LOG_SYS("当前可用堆内存: %u B | 历史最小可用: %u B", diagnostics_get_free_heap(), diagnostics_get_min_free_heap());
  LOG_SYS("电池状态: %.2f V (%d%%, ADC: %d) | 背光亮度: %d%%",
          battery_get_voltage(), battery_get_percent(), battery_get_raw_adc(), display_get_brightness());
  LOG_SYS("TF卡状态: %s (%u MB) | 播放状态: %s",
          sdcard_get_state_str(), sdcard_get_size_mb(), player_state_to_string(player_get_state()));
  LOG_SYS("----------------------------------------------------------");
}

void diagnostics_update() {
  if (millis() - s_lastDiagTime >= 10000) { // 每 10 秒输出一次诊断简报
    s_lastDiagTime = millis();
    LOG_SYS("Uptime: %lus | Heap: %uB (Min:%uB) | Batt: %.2fV | TF: %s | Player: %s | CPU: %dMHz | WiFi: %s | Sleep: %lus | BOOT(GPIO%d)=%d",
            diagnostics_get_uptime_sec(),
            diagnostics_get_free_heap(),
            diagnostics_get_min_free_heap(),
            battery_get_voltage(),
            sdcard_get_state_str(),
            player_state_to_string(player_get_state()),
            ESP.getCpuFreqMHz(),
            net_radio_is_on() ? "ON" : "OFF(省电)",
            power_get_sleep_seconds(),
            KEY_BOOT_PIN,
            digitalRead(KEY_BOOT_PIN));
  }
}
