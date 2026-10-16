#include "playlist.h"

static int s_currentIndex = 0;
static int s_lastShuffleIndex = -1;

void playlist_init() {
  s_currentIndex = 0;
  s_lastShuffleIndex = -1;
}

int playlist_get_current_index() {
  return s_currentIndex;
}

void playlist_set_current_index(int index) {
  int total = db_get_track_count();
  if (total <= 0) {
    s_currentIndex = 0;
    return;
  }
  s_currentIndex = constrain(index, 0, total - 1);
}

int playlist_get_next_index(PlayMode mode) {
  int total = db_get_track_count();
  if (total <= 0) return 0;

  switch (mode) {
    case MODE_REPEAT_ONE:
      return s_currentIndex;

    case MODE_SHUFFLE: {
      if (total == 1) return 0;
      int nextIdx = s_currentIndex;
      // 避免连续随机到同一首
      for (int i = 0; i < 10; i++) {
        nextIdx = random(0, total);
        if (nextIdx != s_currentIndex && nextIdx != s_lastShuffleIndex) break;
      }
      s_lastShuffleIndex = s_currentIndex;
      return nextIdx;
    }

    case MODE_REPEAT_ALL:
      return (s_currentIndex + 1) % total;

    case MODE_SEQUENTIAL:
    default:
      if (s_currentIndex + 1 < total) {
        return s_currentIndex + 1;
      } else {
        return -1; // 顺序播放到末尾停止
      }
  }
}

int playlist_get_prev_index(PlayMode mode) {
  int total = db_get_track_count();
  if (total <= 0) return 0;

  if (mode == MODE_REPEAT_ONE) {
    return s_currentIndex;
  }

  int prev = s_currentIndex - 1;
  if (prev < 0) {
    prev = total - 1;
  }
  return prev;
}

const TrackInfo* playlist_get_current_track() {
  return db_get_track(s_currentIndex);
}
