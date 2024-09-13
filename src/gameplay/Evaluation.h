#ifndef EVALUATION_H
#define EVALUATION_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#include "objects/score/Grade.h"

class Evaluation {
 public:
  Evaluation() {}

  u32 perfects = 0;
  u32 greats = 0;
  u32 goods = 0;
  u32 bads = 0;
  u32 misses = 0;

  u32 maxCombo = 0;
  u32 points = 0;
  u32 longNotes = 0;
  u32 percent = 0;

  inline bool needs4Digits() {
    return perfects > 999 || greats > 999 || goods > 999 || bads > 999 ||
           misses > 999 || maxCombo > 999;
  }

  inline GradeType getGrade() {
    if (percent >= 95 && misses == 0)
      return GradeType::S;
    else if (percent >= 95)
      return GradeType::A;
    else if (percent >= 90)
      return GradeType::B;
    else if (percent >= 85)
      return GradeType::C;
    else if (percent >= 75)
      return GradeType::D;
    else
      return GradeType::F;
  }
};

#endif  // EVALUATION_H
