#pragma once
#include <Arduino.h>
#include "config.h"

// 曲库容量上限。单条 TrackInfo 约 132 字节，80 首约占 10.5KB 静态 RAM，
// 对 320KB 可用 RAM 来说可忽略；实测卡内 68 首可全部收录。
#define MAX_TRACKS 80
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
