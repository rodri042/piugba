#ifndef NUMERIC_LEVEL_DEATH_MIX_H
#define NUMERIC_LEVEL_DEATH_MIX_H

#include "DeathMix.h"

class NumericLevelDeathMix : public DeathMix {
 public:
  NumericLevelDeathMix(const GBFS_FILE* fs, u8 numericLevel);

 protected:
  int getNextChartIndex(Song* tempSong) override;

 private:
  const GBFS_FILE* fs;
  u8 numericLevel;
};

#endif  // NUMERIC_LEVEL_DEATH_MIX_H
