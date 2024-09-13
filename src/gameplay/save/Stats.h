#ifndef STATS_H
#define STATS_H

#include <libgba-sprite-engine/gba/tonc_core.h>

typedef struct __attribute__((__packed__)) {
  u32 playTimeSeconds;
  u32 stagePasses;
  u32 stageBreaks;
  u32 sGrades;
  u32 maxCombo;
  u32 highestLevel;
} Stats;

#endif  // STATS_H
