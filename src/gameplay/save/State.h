#ifndef STATE_H
#define STATE_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#include "AdminSettings.h"
#include "GameMode.h"
#include "Mods.h"
#include "Settings.h"
#include "gameplay/models/Song.h"

#ifndef DATA_EWRAM
#define DATA_EWRAM __attribute__((section(".ewram")))
#endif

const int REDUCE_MOD_POSITION_Y = 51;
const int REDUCE_MOD_SCORE_POSITION_Y = 34;
const u32 GAME_POSITION_X[] = {0, 72, 144};
const u32 GAME_COOP_POSITION_X = 27;

typedef struct __attribute__((__packed__)) {
  bool isPlaying;
  GameMode gameMode;
} State;

typedef struct {
  int positionX[GAME_MAX_PLAYERS];
  int positionY;
  int scorePositionY;

  Settings settings;
  AdminSettings adminSettings;
  Mods mods;
  GameMode mode;
  bool isShuffleMode;
} RAMState;

extern RAMState GameState;

void STATE_setup(Song* song = NULL, Chart* chart = NULL);

#endif  // STATE_H
