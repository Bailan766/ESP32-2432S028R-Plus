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
  s_settings.bootCalibrate = false; // 严禁自动进入校准，仅物理长按 BOOT 键触发
  s_settings.lastTrackId = s_prefs.getInt("last_t", 0);
  s_settings.lastPositionSec = s_prefs.getUShort("last_p", 0);
  s_settings.eqMode = s_prefs.getUChar("eq", 0);
  if (s_settings.eqMode > 2) s_settings.eqMode = 0;
  s_settings.clockAlwaysOn = s_prefs.getBool("clk_on", false);
  s_settings.hourlyChime = s_prefs.getBool("chime", false);
  String cityCd = s_prefs.getString("city_cd", "101110506");
  String cityNm = s_prefs.getString("city_nm", "富平县");
  snprintf(s_settings.weatherCityCode, sizeof(s_settings.weatherCityCode), "%s", cityCd.c_str());
  snprintf(s_settings.weatherCityName, sizeof(s_settings.weatherCityName), "%s", cityNm.c_str());

  LOG_SYS("配置加载完成: 音量=%d%%, 亮度=%d%%, 息屏=%ds, 音效=%d, 时钟常亮=%s, 城市=%s(%s)",
          s_settings.volume, s_settings.brightness, s_settings.autoSleepSeconds,
          s_settings.eqMode, s_settings.clockAlwaysOn ? "ON" : "OFF",
          s_settings.weatherCityName, s_settings.weatherCityCode);
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
  s_prefs.putUChar("eq", s_settings.eqMode);
  s_prefs.putBool("clk_on", s_settings.clockAlwaysOn);
  s_prefs.putBool("chime", s_settings.hourlyChime);
  s_prefs.putString("city_cd", s_settings.weatherCityCode);
  s_prefs.putString("city_nm", s_settings.weatherCityName);
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
