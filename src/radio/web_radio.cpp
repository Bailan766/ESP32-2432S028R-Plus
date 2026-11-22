#include "web_radio.h"
#include <WiFi.h>
#include <SD.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include "hardware/audio.h"
#include "hardware/amplifier.h"
#include "hardware/rgb.h"
#include "storage/sdcard.h"
#include "system/net_manager.h"
#include "config.h"

static const RadioStation STATIONS[] = {
  {"经典流行金曲", "华语流行", "http://stream.zeno.fm/f3wvbbqmdg8uv"},
  {"Lo-Fi 治愈轻音", "学习专注", "http://stream.zeno.fm/4v7rz7hbg8uv"},
  {"轻松爵士时光", "Smooth Jazz", "http://stream.zeno.fm/0r0xa792kwzuv"},
  {"复古怀旧经典", "80/90年代", "http://stream.zeno.fm/k2k04d57kwzuv"},
  {"自然纯音放松", "Piano & Rain", "http://stream.zeno.fm/a818788tg8uv"}
};

static const int NUM_STATIONS = sizeof(STATIONS) / sizeof(STATIONS[0]);
static const int MAX_CUSTOM_STATIONS = 16;
static RadioStation s_customStations[MAX_CUSTOM_STATIONS];
static char s_customNames[MAX_CUSTOM_STATIONS][32];
static char s_customGenres[MAX_CUSTOM_STATIONS][24];
static char s_customUrls[MAX_CUSTOM_STATIONS][96];
static int s_customCount = 0;

static void loadCustomStationsFromSD() {
  s_customCount = 0;
  if (!sdcard_is_available() || !SD.exists("/radio.txt")) return;

  File f = SD.open("/radio.txt", FILE_READ);
  if (!f) return;

  while (f.available() && s_customCount < MAX_CUSTOM_STATIONS) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;

    int p1 = line.indexOf('|');
    if (p1 <= 0) continue;
    int p2 = line.indexOf('|', p1 + 1);

    String name = line.substring(0, p1);
    String genre = (p2 > p1) ? line.substring(p1 + 1, p2) : "自定义电台";
    String url = (p2 > p1) ? line.substring(p2 + 1) : line.substring(p1 + 1);
    name.trim();
    genre.trim();
    url.trim();
    if (!url.startsWith("http://") && !url.startsWith("https://")) continue;

    int i = s_customCount;
    snprintf(s_customNames[i], sizeof(s_customNames[i]), "%s", name.c_str());
    snprintf(s_customGenres[i], sizeof(s_customGenres[i]), "%s", genre.c_str());
    snprintf(s_customUrls[i], sizeof(s_customUrls[i]), "%s", url.c_str());
    s_customStations[i] = { s_customNames[i], s_customGenres[i], s_customUrls[i] };
    s_customCount++;
  }
  f.close();
  if (s_customCount > 0) {
    LOG_AUDIO("网络电台: 已从 /radio.txt 加载 %d 个自定义电台", s_customCount);
  }
}

static int activeStationCount() {
  return (s_customCount > 0) ? s_customCount : NUM_STATIONS;
}

static const RadioStation* activeStationAt(int idx) {
  int total = activeStationCount();
  int safeIdx = ((idx % total) + total) % total;
  return (s_customCount > 0) ? &s_customStations[safeIdx] : &STATIONS[safeIdx];
}
static int s_curStation = 0;
static bool s_isPlaying = false;
static char s_statusText[48] = "已停止";

static AudioFileSourceHTTPStream* s_httpStream = nullptr;
static AudioFileSourceBuffer* s_buff = nullptr;
static AudioGeneratorMP3* s_radioMp3 = nullptr;
static unsigned long s_lastRgbBeat = 0;

static void cleanStream() {
  if (s_radioMp3) {
    if (s_radioMp3->isRunning()) s_radioMp3->stop();
    delete s_radioMp3;
    s_radioMp3 = nullptr;
  }
  if (s_buff) {
    delete s_buff;
    s_buff = nullptr;
  }
  if (s_httpStream) {
    s_httpStream->close();
    delete s_httpStream;
    s_httpStream = nullptr;
  }
  amp_disable();
}

static bool startStream(int idx) {
  cleanStream();
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(s_statusText, sizeof(s_statusText), "WiFi 未连接");
    s_isPlaying = false;
    return false;
  }

  int total = activeStationCount();
  s_curStation = ((idx % total) + total) % total;
  const RadioStation& st = *activeStationAt(s_curStation);
  snprintf(s_statusText, sizeof(s_statusText), "正在连接电台...");
  LOG_AUDIO("网络电台: 正在连接 %s (%s)", st.name, st.url);

  s_httpStream = new AudioFileSourceHTTPStream(st.url);
  if (!s_httpStream) {
    snprintf(s_statusText, sizeof(s_statusText), "创建流失败");
    s_isPlaying = false;
    return false;
  }

  // 4KB 缓冲环，平衡流畅度与内存
  s_buff = new AudioFileSourceBuffer(s_httpStream, 4096);
  s_radioMp3 = new AudioGeneratorMP3();

  amp_enable();
  AudioOutput* out = audio_get_output();
  if (out && s_radioMp3->begin(s_buff, out)) {
    s_isPlaying = true;
    snprintf(s_statusText, sizeof(s_statusText), "正在直播");
    rgb_set_state(RGB_PLAYING);
    LOG_AUDIO("网络电台: 开始解码播放 %s", st.name);
    return true;
  }

  snprintf(s_statusText, sizeof(s_statusText), "连接失败或超时");
  cleanStream();
  s_isPlaying = false;
  return false;
}

void web_radio_init() {
}

void web_radio_start() {
  net_wake_radio(0xFFFFFFFF); // 开启常开 Wi-Fi
  audio_stop();              // 停止本地 MP3 播放
  loadCustomStationsFromSD();
  startStream(s_curStation);
}

void web_radio_stop() {
  cleanStream();
  s_isPlaying = false;
  snprintf(s_statusText, sizeof(s_statusText), "已停止");
  rgb_set_state(RGB_OFF);
  net_set_power_save(true);
}

void web_radio_loop() {
  if (s_isPlaying && s_radioMp3) {
    if (s_radioMp3->isRunning()) {
      if (!s_radioMp3->loop()) {
        s_radioMp3->stop();
        snprintf(s_statusText, sizeof(s_statusText), "电台流断开");
        s_isPlaying = false;
      } else {
        // 随真实 PCM 音频能量呼吸灯律动
        if (millis() - s_lastRgbBeat >= 70) {
          s_lastRgbBeat = millis();
          uint8_t lvl = audio_stream_get_level();
          uint8_t energy = constrain(map(lvl, 0, 100, 30, 255), 30, 255);
          rgb_beat_pulse(energy, 300 + lvl * 12);
        }
      }
    }
  }
}

bool web_radio_is_playing() {
  return s_isPlaying;
}

void web_radio_toggle_play() {
  if (s_isPlaying) {
    web_radio_stop();
  } else {
    web_radio_start();
  }
}

void web_radio_next() {
  startStream(s_curStation + 1);
}

void web_radio_prev() {
  startStream(s_curStation - 1);
}

const RadioStation* web_radio_get_current_station() {
  return activeStationAt(s_curStation);
}

int web_radio_get_station_index() {
  int total = activeStationCount();
  return (s_curStation >= 0 && s_curStation < total) ? s_curStation : 0;
}

int web_radio_get_total_stations() {
  return activeStationCount();
}

const char* web_radio_get_status_text() {
  return s_statusText;
}
