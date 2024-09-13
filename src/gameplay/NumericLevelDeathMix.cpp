#include "NumericLevelDeathMix.h"

NumericLevelDeathMix::NumericLevelDeathMix(const GBFS_FILE* fs, u8 numericLevel)
    : DeathMix(fs, MixMode::SHUFFLE) {
  this->numericLevel = numericLevel;

  for (auto it = songFiles.begin(); it != songFiles.end();) {
    Song* tempSong = SONG_parse(fs, it->get());
    int index = getNextChartIndex(tempSong);
    SONG_free(tempSong);

    if (index == -1)
      it = songFiles.erase(it);
    else
      ++it;
  }

  this->total = songFiles.size();
}

int NumericLevelDeathMix::getNextChartIndex(Song* tempSong) {
  return SONG_findSingleChartIndexByNumericLevel(tempSong, numericLevel);
}
