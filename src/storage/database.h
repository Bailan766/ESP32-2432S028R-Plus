#pragma once
#include <Arduino.h>
#include "config.h"

#define MAX_TRACKS 32
#define MAX_RECENT 10

struct TrackInfo {
  int id;
  char title[32];
  char artist[24];
  char path[64];
  uint16_t durationSec;
  bool isDemo;
};

void db_init();
void db_clear();
bool db_add_track(const char* title, const char* artist, const char* path, uint16_t durationSec, bool isDemo);
int db_get_track_count();
const TrackInfo* db_get_track(int index);
const TrackInfo* db_get_track_by_id(int id);

// 最近播放 (LRU 列表)
void db_add_recent(int trackId);
int db_get_recent_count();
const TrackInfo* db_get_recent_track(int recentIndex);
