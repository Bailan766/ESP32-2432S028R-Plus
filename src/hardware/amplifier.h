#pragma once
#include <Arduino.h>
#include "config.h"

void amp_init();
void amp_enable();
void amp_disable();
void amp_mute();
void amp_unmute();
bool amp_is_enabled();
bool amp_is_muted();
