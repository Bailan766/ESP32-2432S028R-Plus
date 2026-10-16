#pragma once
#include <Arduino.h>
#include "playback_state.h"
#include "storage/database.h"

void playlist_init();
int playlist_get_current_index();
void playlist_set_current_index(int index);
int playlist_get_next_index(PlayMode mode);
int playlist_get_prev_index(PlayMode mode);
const TrackInfo* playlist_get_current_track();
