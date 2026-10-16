#pragma once
#include <Arduino.h>

// 音符频率定义 (Hz)
#define NOTE_REST 0
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093

struct Note {
  uint16_t freq;     // 频率 (Hz)，0 表示休止符
  uint16_t duration; // 持续时间 (毫秒)
};

// 1. 超级马里奥 (Super Mario Bros)
const Note PROGMEM song_mario[] = {
  {NOTE_E5, 130}, {NOTE_REST, 30}, {NOTE_E5, 130}, {NOTE_REST, 120}, {NOTE_E5, 130}, {NOTE_REST, 120},
  {NOTE_C5, 130}, {NOTE_REST, 30}, {NOTE_E5, 130}, {NOTE_REST, 120}, {NOTE_G5, 260}, {NOTE_REST, 200},
  {NOTE_G4, 260}, {NOTE_REST, 200},
  {NOTE_C5, 200}, {NOTE_REST, 80}, {NOTE_G4, 200}, {NOTE_REST, 80}, {NOTE_E4, 200}, {NOTE_REST, 80},
  {NOTE_A4, 180}, {NOTE_REST, 40}, {NOTE_B4, 180}, {NOTE_REST, 40}, {NOTE_AS4, 130}, {NOTE_REST, 40},
  {NOTE_A4, 180}, {NOTE_REST, 40}, {NOTE_G4, 150}, {NOTE_REST, 40}, {NOTE_E5, 150}, {NOTE_REST, 40},
  {NOTE_G5, 150}, {NOTE_REST, 40}, {NOTE_A5, 200}, {NOTE_REST, 40}, {NOTE_F5, 130}, {NOTE_REST, 40},
  {NOTE_G5, 130}, {NOTE_REST, 40}, {NOTE_E5, 200}, {NOTE_REST, 40}, {NOTE_C5, 130}, {NOTE_REST, 40},
  {NOTE_D5, 130}, {NOTE_REST, 40}, {NOTE_B4, 220}, {NOTE_REST, 200}
};

// 2. 俄罗斯方块 (Tetris - Korobeiniki)
const Note PROGMEM song_tetris[] = {
  {NOTE_E5, 220}, {NOTE_REST, 20}, {NOTE_B4, 110}, {NOTE_REST, 20}, {NOTE_C5, 110}, {NOTE_REST, 20},
  {NOTE_D5, 220}, {NOTE_REST, 20}, {NOTE_C5, 110}, {NOTE_REST, 20}, {NOTE_B4, 110}, {NOTE_REST, 20},
  {NOTE_A4, 220}, {NOTE_REST, 20}, {NOTE_A4, 110}, {NOTE_REST, 20}, {NOTE_C5, 110}, {NOTE_REST, 20},
  {NOTE_E5, 220}, {NOTE_REST, 20}, {NOTE_D5, 110}, {NOTE_REST, 20}, {NOTE_C5, 110}, {NOTE_REST, 20},
  {NOTE_B4, 330}, {NOTE_REST, 20}, {NOTE_C5, 110}, {NOTE_REST, 20}, {NOTE_D5, 220}, {NOTE_REST, 20},
  {NOTE_E5, 220}, {NOTE_REST, 20}, {NOTE_C5, 220}, {NOTE_REST, 20}, {NOTE_A4, 220}, {NOTE_REST, 20},
  {NOTE_A4, 300}, {NOTE_REST, 120},
  {NOTE_D5, 260}, {NOTE_REST, 20}, {NOTE_F5, 130}, {NOTE_REST, 20}, {NOTE_A5, 220}, {NOTE_REST, 20},
  {NOTE_G5, 110}, {NOTE_REST, 20}, {NOTE_F5, 110}, {NOTE_REST, 20}, {NOTE_E5, 330}, {NOTE_REST, 20},
  {NOTE_C5, 110}, {NOTE_REST, 20}, {NOTE_E5, 220}, {NOTE_REST, 20}, {NOTE_D5, 110}, {NOTE_REST, 20},
  {NOTE_C5, 110}, {NOTE_REST, 20}, {NOTE_B4, 220}, {NOTE_REST, 20}, {NOTE_B4, 110}, {NOTE_REST, 20},
  {NOTE_C5, 110}, {NOTE_REST, 20}, {NOTE_D5, 220}, {NOTE_REST, 20}, {NOTE_E5, 220}, {NOTE_REST, 20},
  {NOTE_C5, 220}, {NOTE_REST, 20}, {NOTE_A4, 220}, {NOTE_REST, 20}, {NOTE_A4, 300}, {NOTE_REST, 200}
};

// 3. 菊次郎的夏天 (Summer - Joe Hisaishi)
const Note PROGMEM song_summer[] = {
  {NOTE_D5, 180}, {NOTE_E5, 180}, {NOTE_FS5, 300}, {NOTE_REST, 60},
  {NOTE_D5, 180}, {NOTE_E5, 180}, {NOTE_FS5, 300}, {NOTE_REST, 60},
  {NOTE_D5, 180}, {NOTE_E5, 180}, {NOTE_FS5, 180}, {NOTE_A5, 180},
  {NOTE_FS5, 180}, {NOTE_E5, 180}, {NOTE_D5, 360}, {NOTE_REST, 120},
  {NOTE_B4, 180}, {NOTE_CS5, 180}, {NOTE_D5, 300}, {NOTE_REST, 60},
  {NOTE_B4, 180}, {NOTE_CS5, 180}, {NOTE_D5, 300}, {NOTE_REST, 60},
  {NOTE_B4, 180}, {NOTE_CS5, 180}, {NOTE_D5, 180}, {NOTE_FS5, 180},
  {NOTE_E5, 500}, {NOTE_REST, 180}
};

// 4. 卡农 (Canon in D)
const Note PROGMEM song_canon[] = {
  {NOTE_FS4, 260}, {NOTE_E4, 260}, {NOTE_D4, 260}, {NOTE_CS4, 260},
  {NOTE_B3, 260}, {NOTE_A3, 260}, {NOTE_B3, 260}, {NOTE_CS4, 260},
  {NOTE_D4, 260}, {NOTE_FS4, 260}, {NOTE_A4, 260}, {NOTE_G4, 130}, {NOTE_FS4, 130},
  {NOTE_G4, 260}, {NOTE_FS4, 130}, {NOTE_E4, 130}, {NOTE_D4, 260}, {NOTE_CS4, 130}, {NOTE_B3, 130},
  {NOTE_CS4, 260}, {NOTE_D4, 260}, {NOTE_E4, 260}, {NOTE_FS4, 260},
  {NOTE_D4, 400}, {NOTE_REST, 200}
};

// 5. 抬棺/电音神曲 (Astronomia)
const Note PROGMEM song_astronomia[] = {
  {NOTE_AS4, 130}, {NOTE_REST, 40}, {NOTE_AS4, 130}, {NOTE_REST, 40}, {NOTE_AS4, 130}, {NOTE_REST, 40}, {NOTE_AS4, 130}, {NOTE_REST, 40},
  {NOTE_D5, 180}, {NOTE_REST, 40}, {NOTE_C5, 180}, {NOTE_REST, 40}, {NOTE_AS4, 180}, {NOTE_REST, 40}, {NOTE_A4, 180}, {NOTE_REST, 40},
  {NOTE_G4, 180}, {NOTE_REST, 40}, {NOTE_G4, 130}, {NOTE_G4, 130}, {NOTE_G4, 130}, {NOTE_REST, 40},
  {NOTE_D5, 180}, {NOTE_REST, 40}, {NOTE_C5, 180}, {NOTE_REST, 40}, {NOTE_AS4, 180}, {NOTE_REST, 40}, {NOTE_A4, 180}, {NOTE_REST, 40},
  {NOTE_G4, 180}, {NOTE_REST, 40}, {NOTE_D5, 180}, {NOTE_REST, 40}, {NOTE_C5, 180}, {NOTE_REST, 40}, {NOTE_AS4, 180}, {NOTE_REST, 40}, {NOTE_A4, 180}, {NOTE_REST, 150}
};

struct DemoSong {
  const char* title;
  const Note* notes;
  uint16_t length;
};

const DemoSong DEMO_SONGS[] = {
  { "1. Super Mario Bros", song_mario, sizeof(song_mario) / sizeof(Note) },
  { "2. Tetris (Korobeiniki)", song_tetris, sizeof(song_tetris) / sizeof(Note) },
  { "3. Kikujiro - Summer", song_summer, sizeof(song_summer) / sizeof(Note) },
  { "4. Canon in D", song_canon, sizeof(song_canon) / sizeof(Note) },
  { "5. Astronomia (EDM)", song_astronomia, sizeof(song_astronomia) / sizeof(Note) }
};

const int DEMO_SONGS_COUNT = sizeof(DEMO_SONGS) / sizeof(DEMO_SONGS[0]);
