#ifndef DIFFICULTY_LEVEL_DEATH_MIX_H
#define DIFFICULTY_LEVEL_DEATH_MIX_H

#include "DeathMix.h"

class DifficultyLevelDeathMix : public DeathMix {
 public:
  DifficultyLevelDeathMix(const GBFS_FILE* fs, DifficultyLevel difficultyLevel);

 protected:
  int getNextChartIndex(Song* tempSong) override;

 private:
  const GBFS_FILE* fs;
  DifficultyLevel difficultyLevel;
};

#endif  // DIFFICULTY_LEVEL_DEATH_MIX_H
