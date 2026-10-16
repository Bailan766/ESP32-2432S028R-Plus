#include "settings_manager.h"
#include "config.h"

static Preferences s_prefs;
static SystemSettings s_settings;
static bool s_dirty = false;
static unsigned long s_lastSaveTime = 0;

void settings_init() {
  LOG_SYS("正在从 NVS 读取持久化配置...");
  s_prefs.begin("bailan_mp3", false);

  s_settings.volume = s_prefs.getUChar("vol", 70);
  s_settings.brightness = s_prefs.getUChar("bright", 80);
  s_settings.autoSleepSeconds = s_prefs.getUShort("sleep", 60); // 默认 60 秒息屏
  s_settings.playMode = s_prefs.getUChar("mode", (uint8_t)MODE_REPEAT_ALL);
  s_settings.resumeEnable = s_prefs.getBool("resume", true);
  s_settings.screenRotation = s_prefs.getUChar("rot", 3);
  if (s_settings.screenRotation != 1 && s_settings.screenRotation != 3) {
    s_settings.screenRotation = 3;
  }
  s_settings.bootCalibrate = s_prefs.getBool("boot_cal", true); // 默认开机自动校准
  s_settings.lastTrackId = s_prefs.getInt("last_t", 0);
  s_settings.lastPositionSec = s_prefs.getUShort("last_p", 0);

  LOG_SYS("配置加载完成: 音量=%d%%, 亮度=%d%%, 息屏=%ds, 方向=%d, 开机校准=%s, 播放模式=%d, 断点续播=%s",
          s_settings.volume, s_settings.brightness, s_settings.autoSleepSeconds,
          s_settings.screenRotation, s_settings.bootCalibrate ? "ON" : "OFF",
          s_settings.playMode, s_settings.resumeEnable ? "ON" : "OFF");
}

SystemSettings& settings_get() {
  return s_settings;
}

void settings_save() {
  LOG_SYS("保存配置到 NVS...");
  s_prefs.putUChar("vol", s_settings.volume);
  s_prefs.putUChar("bright", s_settings.brightness);
  s_prefs.putUShort("sleep", s_settings.autoSleepSeconds);
  s_prefs.putUChar("mode", s_settings.playMode);
  s_prefs.putBool("resume", s_settings.resumeEnable);
  s_prefs.putUChar("rot", s_settings.screenRotation);
  s_prefs.putBool("boot_cal", s_settings.bootCalibrate);
  s_prefs.putInt("last_t", s_settings.lastTrackId);
  s_prefs.putUShort("last_p", s_settings.lastPositionSec);
}

void settings_save_playback_position(int trackId, uint16_t positionSec) {
  if (!s_settings.resumeEnable) return;

  s_settings.lastTrackId = trackId;
  s_settings.lastPositionSec = positionSec;

  // 避免每秒写入 Flash 造成颗粒磨损，仅每隔 15 秒或切歌时写入一次
  if (millis() - s_lastSaveTime > 15000) {
    s_lastSaveTime = millis();
    s_prefs.putInt("last_t", s_settings.lastTrackId);
    s_prefs.putUShort("last_p", s_settings.lastPositionSec);
  }
}
