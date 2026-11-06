#include "player.h"
#include "playlist.h"
#include "hardware/audio.h"
#include "hardware/rgb.h"
#include "hardware/amplifier.h"
#include "storage/database.h"
#include "songs.h"

static PlayerState s_playerState = STATE_STOPPED;
static PlayMode s_playMode = MODE_REPEAT_ALL; 

static uint16_t s_elapsedSeconds = 0;
static uint16_t s_totalSeconds = 180;
static unsigned long s_lastSecondTimer = 0;

static void startCurrentTrack() {
  const TrackInfo* track = playlist_get_current_track();
  if (!track) {
    s_playerState = STATE_ERROR;
    LOG_ERROR("播放失败: 当前列表为空");
    return;
  }

  LOG_PLAYER("开始曲目: [%d] %s (%s)", track->id, track->title, track->path);
  s_playerState = STATE_LOADING;

  s_elapsedSeconds = 0;
  s_totalSeconds = track->durationSec > 0 ? track->durationSec : 180;
  s_lastSecondTimer = millis();

  db_add_recent(track->id);

  if (track->isDemo) {
    LOG_PLAYER("播放内置 Demo 示范曲: [%s]...", track->title);
    int songIdx = 0;
    if (strstr(track->path, "mario") || strstr(track->title, "马里奥")) songIdx = 0;
    else if (strstr(track->path, "tetris") || strstr(track->title, "俄罗斯方块")) songIdx = 1;
    else if (strstr(track->path, "summer") || strstr(track->title, "夏天") || strstr(track->title, "菊次郎")) songIdx = 2;
    else if (strstr(track->path, "canon") || strstr(track->title, "卡农")) songIdx = 3;
    else if (strstr(track->path, "astronomia") || strstr(track->title, "抬棺") || strstr(track->title, "电音")) songIdx = 4;
    else songIdx = track->id % DEMO_SONGS_COUNT;

    if (audio_play_demo(songIdx)) {
      s_playerState = STATE_PLAYING;
      rgb_set_state(RGB_PLAYING);
    } else {
      s_playerState = STATE_ERROR;
      player_next();
    }
    return;
  }

  if (audio_play_mp3(track->path)) {
    s_playerState = STATE_PLAYING;
    rgb_set_state(RGB_PLAYING);
  } else {
    s_playerState = STATE_ERROR;
    LOG_ERROR("无法播放 MP3!");
    player_next(); // Skip to next
  }
}

void player_init() {
  playlist_init();
  s_playerState = STATE_STOPPED;
  s_playMode = MODE_REPEAT_ALL;
  s_elapsedSeconds = 0;
  s_totalSeconds = 0;
}

void player_play() {
  if (s_playerState == STATE_PAUSED) {
    player_resume();
  } else {
    startCurrentTrack();
  }
}

void player_pause() {
  if (s_playerState == STATE_PLAYING) {
    s_playerState = STATE_PAUSED;
    audio_pause();
    rgb_set_state(RGB_PAUSED);
    LOG_PLAYER("已暂停");
  }
}

void player_resume() {
  if (s_playerState == STATE_PAUSED) {
    s_playerState = STATE_PLAYING;
    audio_resume();
    s_lastSecondTimer = millis();
    rgb_set_state(RGB_PLAYING);
    LOG_PLAYER("已恢复");
  }
}

void player_toggle() {
  if (s_playerState == STATE_PLAYING) {
    player_pause();
  } else {
    player_play();
  }
}

void player_stop() {
  s_playerState = STATE_STOPPED;
  audio_stop();
  rgb_set_state(RGB_OFF);
  s_elapsedSeconds = 0;
  LOG_PLAYER("已停止");
}

void player_next() {
  int nextIdx = playlist_get_next_index(s_playMode);
  if (nextIdx >= 0) {
    playlist_set_current_index(nextIdx);
    startCurrentTrack();
  } else {
    player_stop();
  }
}

void player_prev() {
  int prevIdx = playlist_get_prev_index(s_playMode);
  playlist_set_current_index(prevIdx);
  startCurrentTrack();
}

void player_seek(uint16_t seconds) {
  s_elapsedSeconds = constrain(seconds, 0, s_totalSeconds);
  LOG_PLAYER("跳跃时间点: %u 秒", s_elapsedSeconds);
}

void player_seek_offset(int16_t deltaSeconds) {
  int target = (int)s_elapsedSeconds + deltaSeconds;
  s_elapsedSeconds = (uint16_t)constrain(target, 0, (int)s_totalSeconds);
  LOG_PLAYER("相对快进/退: %+d 秒", deltaSeconds);
}

void player_play_track(int trackIndex) {
  playlist_set_current_index(trackIndex);
  startCurrentTrack();
}

PlayerState player_get_state() {
  return s_playerState;
}

PlayMode player_get_mode() {
  return s_playMode;
}

void player_set_mode(PlayMode mode) {
  s_playMode = mode;
  LOG_PLAYER("切换模式: %s", play_mode_to_string(mode));
}

void player_cycle_mode() {
  PlayMode nextMode = (PlayMode)((s_playMode + 1) % 4);
  player_set_mode(nextMode);
}

const TrackInfo* player_get_current_track() {
  return playlist_get_current_track();
}

uint16_t player_get_elapsed_seconds() {
  return s_elapsedSeconds;
}

uint16_t player_get_total_seconds() {
  return s_totalSeconds;
}

uint16_t player_get_current_note_freq() {
  uint16_t f = audio_get_current_freq();
  if (f > 0) return f;
  return random(100, 2000);
}

void player_update() {
  if (s_playerState != STATE_PLAYING) return;

  static unsigned long s_lastBgRgb = 0;
  if (millis() - s_lastBgRgb >= 70) {
    s_lastBgRgb = millis();
    uint16_t f = audio_get_current_freq();
    if (f > 0) {
      uint8_t bright = map(constrain(f, 200, 1600), 200, 1600, 120, 255);
      rgb_beat_pulse(bright, f);
    } else {
      rgb_beat_pulse(random(80, 220), random(150, 1500));
    }
  }

  // 1. 每秒更新播放时间
  if (millis() - s_lastSecondTimer >= 1000) {
    s_lastSecondTimer += 1000;
    if (s_elapsedSeconds < s_totalSeconds) {
      s_elapsedSeconds++;
    } else {
      // 达到当前曲目指定播放时长，自动跳切下一首
      player_next();
      return;
    }
  }

  // 2. 检查 MP3 / 底层是否播放完毕
  if (!audio_is_playing()) {
    s_playerState = STATE_FINISHED;
    player_next();
  }
}
