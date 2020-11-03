#ifndef SONG_H
#define SONG_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#include "Chart.h"
#include "Event.h"
#include "SongFile.h"
#include "gameplay/save/GameMode.h"
#include "utils/parse.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

enum Channel { ORIGINAL, KPOP, WORLD, BOSS };

typedef struct {
  char* title;          // 0x00 (31 bytes - including \0)
  char* artist;         // 0x1F (27 bytes - including \0)
  Channel channel;      // 0x3A (u8)
  u32 lastMillisecond;  // 0x3B (u32)
  u32 sampleStart;      // 0x3F (u32 - in ms)
  u32 sampleLength;     // 0x43 (u32 - in ms)

  u8 applyTo[3];   //   0x47
  u8 isBoss;       //   0x4A
  u8 pixelate;     //   0x4B
  u8 jump;         //   0x4C
  u8 reduce;       //   0x4D
  u8 decolorize;   //   0x4E
  u8 randomSpeed;  //   0x4F
  u8 ___;          //   0x50 (unused)
  u8 hasMessage;   //   0x51
  char* message;   //   0x52 (optional - 107 bytes - including \0)

  u8 chartCount;  // 0x52 if no message, 0xBD otherwise (u8)
  Chart* charts;  // 0x53 if no message, 0xBE otherwise ("chartCount" times)

  // custom fields:
  u32 id;
  std::string audioPath;
  std::string backgroundTilesPath;
  std::string backgroundPalettePath;
  std::string backgroundMapPath;
} Song;

Song* SONG_parse(const GBFS_FILE* fs, SongFile* file, bool full);
Channel SONG_getChannel(const GBFS_FILE* fs,
                        GameMode gameMode,
                        SongFile* file,
                        DifficultyLevel difficultyLevel);
Chart* SONG_findChartByNumericLevelIndex(Song* song, u8 levelIndex);
Chart* SONG_findChartByDifficultyLevel(Song* song,
                                       DifficultyLevel difficultyLevel);
void SONG_free(Song* song);

#endif  // SONG_H
