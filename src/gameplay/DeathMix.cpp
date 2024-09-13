#include "DeathMix.h"

#include <libgba-sprite-engine/gba/tonc_bios.h>

#include "save/SaveFile.h"

DeathMix::DeathMix(const GBFS_FILE* fs, MixMode mixMode) {
  this->mixMode = mixMode;

  auto library = std::unique_ptr<Library>{new Library(fs)};
  u32 librarySize = SAVEFILE_getLibrarySize();
  u32 pages =
      Div(librarySize, PAGE_SIZE) + (DivMod(librarySize, PAGE_SIZE) > 0);

  for (u32 i = 0; i < pages; i++)
    library->loadSongs(songFiles, DifficultyLevel::CRAZY, i * PAGE_SIZE);
  // (difficulty level doesn't matter here, as the order will be random anyway)

  // randomize song order
  for (int i = songFiles.size() - 1; i > 0; --i) {
    int j = qran_range(0, i + 1);
    std::swap(songFiles[i], songFiles[j]);
  }

  for (u32 i = 0; i < songFiles.size(); i++)
    songFiles[i]->index = i;

  this->fs = fs;
  this->next = 0;
  this->total = librarySize;
}

SongChart DeathMix::getNextSongChart() {
  if (next >= total)
    return SongChart{.song = NULL, .chart = NULL};

  Song* tempSong = SONG_parse(fs, songFiles[next].get());
  int index = getNextChartIndex(tempSong);
  SONG_free(tempSong);
  if (index == -1)  // (should not happen, just a fail-safe)
    return SongChart{.song = NULL, .chart = NULL};

  Song* song =
      SONG_parse(fs, songFiles[next].get(), std::vector<u8>{(u8)index});
  Chart* chart = song->charts + index;

  next++;

  return SongChart{.song = song, .chart = chart};
}
