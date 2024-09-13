#ifndef DEATH_MIX_H
#define DEATH_MIX_H

#include <array>
#include <memory>
#include <vector>

#include "Library.h"
#include "models/Song.h"
#include "objects/score/Score.h"

enum MixMode { DEATH, SHUFFLE };

typedef struct {
  Song* song;
  Chart* chart;
} SongChart;

class DeathMix {
 public:
  MixMode mixMode;
  bool didStartScroll = false;
  u32 multiplier = 1;
  u32 combo = 0;
  int life = INITIAL_LIFE;
  bool hasMissCombo = false;
  bool halfLifeBonus = false;
  u32 maxCombo = 0;
  std::array<u32, FEEDBACK_TYPES_TOTAL> counters;
  u32 points = 0;
  u32 longNotes = 0;

  DeathMix(const GBFS_FILE* fs, MixMode mixMode);

  bool isInitialSong() { return next == 1; }
  SongChart getNextSongChart();
  u32 getCurrentSongNumber() { return next - 1; }
  bool isEmpty() { return songFiles.empty(); }

 protected:
  virtual int getNextChartIndex(Song* tempSong) = 0;
  std::vector<std::unique_ptr<SongFile>> songFiles;
  u32 next;
  u32 total;

 private:
  const GBFS_FILE* fs;
};

#endif  // DEATH_MIX_H
