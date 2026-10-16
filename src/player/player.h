#pragma once
#include <Arduino.h>
#include "playback_state.h"
#include "storage/database.h"

void player_init();
void player_update();

void player_play();
void player_pause();
void player_resume();
void player_toggle();
void player_stop();
void player_next();
void player_prev();
void player_seek(uint16_t seconds);
void player_seek_offset(int16_t deltaSeconds);
void player_play_track(int trackIndex);

PlayerState player_get_state();
PlayMode player_get_mode();
void player_set_mode(PlayMode mode);
void player_cycle_mode();

const TrackInfo* player_get_current_track();
uint16_t player_get_elapsed_seconds();
uint16_t player_get_total_seconds();
uint16_t player_get_current_note_freq();
