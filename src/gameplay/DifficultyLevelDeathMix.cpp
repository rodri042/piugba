#include "DifficultyLevelDeathMix.h"

DifficultyLevelDeathMix::DifficultyLevelDeathMix(
    const GBFS_FILE* fs,
    DifficultyLevel difficultyLevel)
    : DeathMix(fs, MixMode::DEATH) {
  this->difficultyLevel = difficultyLevel;
}

int DifficultyLevelDeathMix::getNextChartIndex(Song* tempSong) {
  return SONG_findChartIndexByDifficultyLevel(tempSong, difficultyLevel);
}
