#ifndef CHART_H
#define CHART_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#include <string>
#include "Event.h"

enum DifficultyLevel { NORMAL, HARD, CRAZY, NUMERIC };
enum ChartType { SINGLE_CHART, DOUBLE_CHART, DOUBLE_COOP_CHART };
const u32 MAX_DIFFICULTY = 2;

typedef struct {
  DifficultyLevel difficulty;  // u8
  u8 level;                    // (0~99)
  char variant;                // '\0' or 'a', 'b', ... (for repeated levels)
  char offsetLabel;  // '\0' or 'a', 'b', ... (to identify different offsets)
  ChartType type;    // u8

  u32 eventChunkSize;

  u32 rhythmEventCount;
  Event* rhythmEvents;  // ("rhythmEventCount" times)

  u32 eventCount;
  Event* events;  // ("eventCount" times)

  // custom fields:
  bool isDouble;  // type == ChartType::DOUBLE_CHART ||
                  // type == ChartType::DOUBLE_COOP_CHART
  int customOffset;
  u8 levelIndex;

  std::string getArcadeLevelString() {
    if (difficulty == DifficultyLevel::NORMAL)
      return "NM";

    if (difficulty == DifficultyLevel::HARD)
      return "HD";

    if (difficulty == DifficultyLevel::CRAZY)
      return "CZ";

    if (type == ChartType::DOUBLE_COOP_CHART)
      return ";)";

    return "";
  }

  std::string getLevelString() {
    return (type == ChartType::SINGLE_CHART   ? "s"
            : type == ChartType::DOUBLE_CHART ? "d"
                                              : "m") +
           (level == 99 ? "??" : std::to_string(level));
  }
} Chart;

#endif  // CHART_H
