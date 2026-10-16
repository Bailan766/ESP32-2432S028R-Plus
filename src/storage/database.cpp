#include "database.h"

static TrackInfo s_tracks[MAX_TRACKS];
static int s_trackCount = 0;

static int s_recentIds[MAX_RECENT];
static int s_recentCount = 0;

void db_init() {
  db_clear();
}

void db_clear() {
  s_trackCount = 0;
  s_recentCount = 0;
  for (int i = 0; i < MAX_RECENT; i++) {
    s_recentIds[i] = -1;
  }
}

bool db_add_track(const char* title, const char* artist, const char* path, uint16_t durationSec, bool isDemo) {
  if (s_trackCount >= MAX_TRACKS) return false;

  TrackInfo& t = s_tracks[s_trackCount];
  t.id = s_trackCount;
  strncpy(t.title, title, sizeof(t.title) - 1); t.title[sizeof(t.title) - 1] = '\0';
  strncpy(t.artist, artist, sizeof(t.artist) - 1); t.artist[sizeof(t.artist) - 1] = '\0';
  strncpy(t.path, path, sizeof(t.path) - 1); t.path[sizeof(t.path) - 1] = '\0';
  t.durationSec = durationSec;
  t.isDemo = isDemo;

  s_trackCount++;
  return true;
}

int db_get_track_count() {
  return s_trackCount;
}

const TrackInfo* db_get_track(int index) {
  if (index < 0 || index >= s_trackCount) return nullptr;
  return &s_tracks[index];
}

const TrackInfo* db_get_track_by_id(int id) {
  for (int i = 0; i < s_trackCount; i++) {
    if (s_tracks[i].id == id) return &s_tracks[i];
  }
  return nullptr;
}

void db_add_recent(int trackId) {
  if (trackId < 0 || trackId >= s_trackCount) return;

  // 1. 如果已存在，先移除
  int existingIdx = -1;
  for (int i = 0; i < s_recentCount; i++) {
    if (s_recentIds[i] == trackId) {
      existingIdx = i;
      break;
    }
  }

  if (existingIdx != -1) {
    for (int i = existingIdx; i < s_recentCount - 1; i++) {
      s_recentIds[i] = s_recentIds[i + 1];
    }
    s_recentCount--;
  }

  // 2. 队列后移，插入头部
  int moveCount = (s_recentCount < MAX_RECENT) ? s_recentCount : (MAX_RECENT - 1);
  for (int i = moveCount; i > 0; i--) {
    s_recentIds[i] = s_recentIds[i - 1];
  }
  s_recentIds[0] = trackId;
  if (s_recentCount < MAX_RECENT) s_recentCount++;
}

int db_get_recent_count() {
  return s_recentCount;
}

const TrackInfo* db_get_recent_track(int recentIndex) {
  if (recentIndex < 0 || recentIndex >= s_recentCount) return nullptr;
  int tid = s_recentIds[recentIndex];
  return db_get_track_by_id(tid);
}
