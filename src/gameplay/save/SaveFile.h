#ifndef SAVE_FILE_H
#define SAVE_FILE_H

#include <libgba-sprite-engine/gba/tonc_bios.h>
#include <libgba-sprite-engine/gba/tonc_core.h>
#include <libgba-sprite-engine/gba/tonc_memmap.h>

#include "Memory.h"
#include "Mods.h"
#include "Progress.h"
#include "Settings.h"
#include "State.h"
#include "assets.h"
#include "gameplay/debug/DebugTools.h"
#include "gameplay/models/Chart.h"
#include "utils/MathUtils.h"
#include "utils/parse.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

const u32 PROGRESS_REGISTERS = 6;
const u32 PROGRESS_IMPOSSIBLE = 3;
const u32 LIBRARY_SIZE_MASK = 0x000000FF;

typedef struct __attribute__((__packed__)) {
  u32 romId;

  Settings settings;
  Mods mods;
  Memory memory;
  Progress progress[PROGRESS_REGISTERS];

  State state;
} SaveFile;

#define SRAM ((SaveFile*)sram_mem)

#define SAVEFILE_read8(TARGET) *((u8*)&TARGET)
#define SAVEFILE_write8(DEST, VALUE) *((u8*)&DEST) = VALUE;
#define SAVEFILE_read32(TARGET)                                    \
  (u32)(*(((char*)&TARGET) + 0) + (*(((char*)&TARGET) + 1) << 8) + \
        (*(((char*)&TARGET) + 2) << 16) + (*(((char*)&TARGET) + 3) << 24))
#define SAVEFILE_write32(DEST, VALUE)                        \
  *(((char*)&DEST) + 0) = (((u32)VALUE) & 0x000000ff) >> 0;  \
  *(((char*)&DEST) + 1) = (((u32)VALUE) & 0x0000ff00) >> 8;  \
  *(((char*)&DEST) + 2) = (((u32)VALUE) & 0x00ff0000) >> 16; \
  *(((char*)&DEST) + 3) = (((u32)VALUE) & 0xff000000) >> 24;

inline void SAVEFILE_resetSettings() {
  SAVEFILE_write32(SRAM->settings.audioLag, 0);
  SAVEFILE_write8(SRAM->settings.gamePosition, GamePosition::LEFT);
  SAVEFILE_write8(SRAM->settings.backgroundType, BackgroundType::HALF_BGA_DARK);
  SAVEFILE_write8(SRAM->settings.bgaDarkBlink, true);
}

inline void SAVEFILE_resetMods() {
  SAVEFILE_write8(SRAM->mods.multiplier, 3);
  SAVEFILE_write8(SRAM->mods.stageBreak, StageBreakOpts::sON);
  SAVEFILE_write8(SRAM->mods.pixelate, PixelateOpts::pOFF);
  SAVEFILE_write8(SRAM->mods.jump, false);
  SAVEFILE_write8(SRAM->mods.reduce, ReduceOpts::rOFF);
  SAVEFILE_write8(SRAM->mods.negative, false);
  SAVEFILE_write8(SRAM->mods.randomSpeed, false);
  SAVEFILE_write8(SRAM->mods.mirrorSteps, false);
  SAVEFILE_write8(SRAM->mods.randomSteps, false);
  SAVEFILE_write8(SRAM->mods.extraJudgement, false);
}

inline void SAVEFILE_initialize(const GBFS_FILE* fs) {
  u32 romId = as_le((u8*)gbfs_get_obj(fs, ROM_ID_FILE, NULL));
  bool isNew = SAVEFILE_read32(SRAM->romId) != romId;

  if (isNew) {
    SAVEFILE_write32(SRAM->romId, romId);

    SAVEFILE_resetSettings();
    SAVEFILE_resetMods();

    SAVEFILE_write8(SRAM->memory.pageIndex, 0);
    SAVEFILE_write8(SRAM->memory.songIndex, 0);
    SAVEFILE_write8(SRAM->memory.difficultyLevel, 0);
    SAVEFILE_write8(SRAM->memory.numericLevel, 0);
    SAVEFILE_write8(SRAM->memory.isAudioLagCalibrated, false);

    SAVEFILE_write8(SRAM->progress[DifficultyLevel::NORMAL].completedSongs, 0);
    SAVEFILE_write8(SRAM->progress[DifficultyLevel::HARD].completedSongs, 0);
    SAVEFILE_write8(SRAM->progress[DifficultyLevel::CRAZY].completedSongs, 0);
    SAVEFILE_write8(
        SRAM->progress[PROGRESS_IMPOSSIBLE + DifficultyLevel::NORMAL]
            .completedSongs,
        0);
    SAVEFILE_write8(SRAM->progress[PROGRESS_IMPOSSIBLE + DifficultyLevel::HARD]
                        .completedSongs,
                    0);
    SAVEFILE_write8(SRAM->progress[PROGRESS_IMPOSSIBLE + DifficultyLevel::CRAZY]
                        .completedSongs,
                    0);

    for (u32 i = 0; i < MAX_PROGRESS_REGISTERS; i++) {
      SAVEFILE_write8(SRAM->progress[DifficultyLevel::NORMAL].grades[i],
                      GradeType::UNPLAYED);
      SAVEFILE_write8(SRAM->progress[DifficultyLevel::HARD].grades[i],
                      GradeType::UNPLAYED);
      SAVEFILE_write8(SRAM->progress[DifficultyLevel::CRAZY].grades[i],
                      GradeType::UNPLAYED);
      SAVEFILE_write8(
          SRAM->progress[PROGRESS_IMPOSSIBLE + DifficultyLevel::NORMAL]
              .grades[i],
          GradeType::UNPLAYED);
      SAVEFILE_write8(
          SRAM->progress[PROGRESS_IMPOSSIBLE + DifficultyLevel::HARD].grades[i],
          GradeType::UNPLAYED);
      SAVEFILE_write8(
          SRAM->progress[PROGRESS_IMPOSSIBLE + DifficultyLevel::CRAZY]
              .grades[i],
          GradeType::UNPLAYED);
    }

    SAVEFILE_write8(SRAM->state.isPlaying, false);
    SAVEFILE_write8(SRAM->state.gameMode, GameMode::CAMPAIGN);
  }
}

inline bool SAVEFILE_isWorking(const GBFS_FILE* fs) {
  u32 romId = as_le((u8*)gbfs_get_obj(fs, ROM_ID_FILE, NULL));
  return SAVEFILE_read32(SRAM->romId) == romId;
}

inline u8 SAVEFILE_getLibrarySize() {
  return (SAVEFILE_read32(SRAM->romId)) & LIBRARY_SIZE_MASK;
}

inline u8 SAVEFILE_getCompletedSongs() {
  u8 a = SAVEFILE_read8(SRAM->progress[DifficultyLevel::NORMAL].completedSongs);
  u8 b = SAVEFILE_read8(SRAM->progress[DifficultyLevel::HARD].completedSongs);
  u8 c = SAVEFILE_read8(SRAM->progress[DifficultyLevel::CRAZY].completedSongs);

  return MATH_max(a, b, c);
}

inline bool SAVEFILE_isModeUnlocked(GameMode gameMode) {
  if (IGNORE_LOCKS)
    return true;

  u8 completedSongs = SAVEFILE_getCompletedSongs();

  if (gameMode == GameMode::ARCADE)
    return completedSongs >= 1;

  if (gameMode == GameMode::IMPOSSIBLE)
    return completedSongs == SAVEFILE_getLibrarySize();

  return true;
}

inline GradeType SAVEFILE_getGradeOf(u8 songIndex, DifficultyLevel level) {
  auto gameMode = static_cast<GameMode>(SAVEFILE_read8(SRAM->state.gameMode));
  if (gameMode == GameMode::ARCADE)
    return GradeType::UNPLAYED;

  u32 index =
      (gameMode == GameMode::IMPOSSIBLE ? PROGRESS_IMPOSSIBLE : 0) + level;
  int lastIndex = SAVEFILE_read8(SRAM->progress[index].completedSongs) - 1;
  if (songIndex > lastIndex)
    return GradeType::UNPLAYED;

  return static_cast<GradeType>(
      SAVEFILE_read8(SRAM->progress[index].grades[songIndex]));
}

inline bool SAVEFILE_setGradeOf(u8 songIndex,
                                DifficultyLevel level,
                                GradeType grade) {
  auto gameMode = static_cast<GameMode>(SAVEFILE_read8(SRAM->state.gameMode));
  if (gameMode == GameMode::ARCADE)
    return false;

  u32 index =
      (gameMode == GameMode::IMPOSSIBLE ? PROGRESS_IMPOSSIBLE : 0) + level;
  int lastIndex = SAVEFILE_read8(SRAM->progress[index].completedSongs) - 1;
  u8 librarySize = SAVEFILE_getLibrarySize();
  bool firstTime = songIndex > lastIndex;

  if (firstTime) {
    auto nextSongIndex = (u8)min(songIndex + 1, librarySize - 1);
    auto completedSongs = (u8)min(songIndex + 1, librarySize);
    SAVEFILE_write8(SRAM->progress[index].completedSongs, completedSongs);
    SAVEFILE_write8(SRAM->memory.pageIndex, Div(nextSongIndex, 4));
    SAVEFILE_write8(SRAM->memory.songIndex, DivMod(nextSongIndex, 4));
  }

  if (firstTime ||
      grade < SAVEFILE_read8(SRAM->progress[index].grades[songIndex]))
    SAVEFILE_write8(SRAM->progress[index].grades[songIndex], grade);

  return songIndex == SAVEFILE_getLibrarySize() - 1;
}

#endif  // SAVE_FILE_H
