#ifndef SAVE_FILE_H
#define SAVE_FILE_H

#include <libgba-sprite-engine/gba/tonc_bios.h>
#include <libgba-sprite-engine/gba/tonc_core.h>
#include <libgba-sprite-engine/gba/tonc_memmap.h>

#include "AdminSettings.h"
#include "ArcadeProgress.h"
#include "CustomOffsetTable.h"
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

#define ROM_ID_MASK 0xffffff00

const u32 PROGRESS_REGISTERS = 6;
const u32 PROGRESS_IMPOSSIBLE = 3;
const u32 LIBRARY_SIZE_MASK = 0x000000FF;

const u32 AUTOVELOCITY_VALUES[] = {500, 600, 700, 750, 800, 850};

typedef struct __attribute__((__packed__)) {
  u32 romId;

  Settings settings;
  char padding[10];
  Memory memory;
  Progress progress[PROGRESS_REGISTERS];

  State state;

  u8 singleArcadeProgress[ARCADE_PROGRESS_SIZE];
  u8 doubleArcadeProgress[ARCADE_PROGRESS_SIZE];

  AdminSettings adminSettings;
  char padding2[10];
  u8 beat;

  Mods mods;
  char padding3[10];

  s8 customOffsets[CUSTOM_OFFSET_TABLE_SIZE];
} SaveFile;

#define SRAM ((SaveFile*)sram_mem)

#define SAVEFILE_read8(TARGET) (*((vu8*)&TARGET))
#define SAVEFILE_write8(DEST, VALUE) *((vu8*)&DEST) = VALUE;
#define SAVEFILE_read32(TARGET)                     \
  ((u32)(*(((volatile char*)&TARGET) + 0) +         \
         (*(((volatile char*)&TARGET) + 1) << 8) +  \
         (*(((volatile char*)&TARGET) + 2) << 16) + \
         (*(((volatile char*)&TARGET) + 3) << 24)))
#define SAVEFILE_write32(DEST, VALUE)                                   \
  *(((volatile char*)&DEST) + 0) = (((u32)(VALUE)) & 0x000000ff) >> 0;  \
  *(((volatile char*)&DEST) + 1) = (((u32)(VALUE)) & 0x0000ff00) >> 8;  \
  *(((volatile char*)&DEST) + 2) = (((u32)(VALUE)) & 0x00ff0000) >> 16; \
  *(((volatile char*)&DEST) + 3) = (((u32)(VALUE)) & 0xff000000) >> 24;

inline void SAVEFILE_resetSettings() {
  SAVEFILE_write32(SRAM->settings.audioLag, 0);
  SAVEFILE_write8(SRAM->settings.gamePosition, GamePosition::LEFT);
  SAVEFILE_write8(SRAM->settings.backgroundType, BackgroundType::FULL_BGA_DARK);
  SAVEFILE_write8(SRAM->settings.bgaDarkBlink, true);
}

inline void SAVEFILE_resetMods() {
  SAVEFILE_write8(SRAM->mods.multiplier, 3);
  SAVEFILE_write8(SRAM->mods.stageBreak, StageBreakOpts::sON);
  SAVEFILE_write8(SRAM->mods.pixelate, PixelateOpts::pOFF);
  SAVEFILE_write8(SRAM->mods.jump, JumpOpts::jOFF);
  SAVEFILE_write8(SRAM->mods.reduce, ReduceOpts::rOFF);
  SAVEFILE_write8(SRAM->mods.bounce, BounceOpts::bOFF);
  SAVEFILE_write8(SRAM->mods.colorFilter, ColorFilter::NO_FILTER);
  SAVEFILE_write8(SRAM->mods.speedHack, SpeedHackOpts::hOFF);
  SAVEFILE_write8(SRAM->mods.mirrorSteps, false);
  SAVEFILE_write8(SRAM->mods.randomSteps, false);
  SAVEFILE_write8(SRAM->mods.autoMod, false);
  SAVEFILE_write8(SRAM->mods.trainingMode, TrainingModeOpts::tOFF);
}

inline void SAVEFILE_resetAdminSettings() {
  SAVEFILE_write8(SRAM->adminSettings.arcadeCharts, ArcadeChartsOpts::SINGLE);
  SAVEFILE_write8(SRAM->adminSettings.rumble, false);
  SAVEFILE_write8(SRAM->adminSettings.ioBlink, IOBlinkOpts::IO_BLINK_OFF);
  SAVEFILE_write8(SRAM->adminSettings.sramBlink, SRAMBlinkOpts::SRAM_BLINK_OFF);
  SAVEFILE_write8(SRAM->adminSettings.navigationStyle,
                  NavigationStyleOpts::GBA);
  SAVEFILE_write8(SRAM->adminSettings.offsetEditingEnabled, false);

#ifdef SENV_DEVELOPMENT
  SAVEFILE_write8(SRAM->adminSettings.navigationStyle,
                  NavigationStyleOpts::PIU);
#endif
}

inline u32 SAVEFILE_normalize(u32 librarySize) {
  u32 fixes = 0;

  // validate settings
  int audioLag = (int)SAVEFILE_read32(SRAM->settings.audioLag);
  u8 gamePosition = SAVEFILE_read8(SRAM->settings.gamePosition);
  u8 backgroundType = SAVEFILE_read8(SRAM->settings.backgroundType);
  u8 bgaDarkBlink = SAVEFILE_read8(SRAM->settings.bgaDarkBlink);
  if (audioLag < -3000 || audioLag > 3000 || gamePosition >= 3 ||
      backgroundType >= 3 || bgaDarkBlink >= 2) {
    SAVEFILE_resetSettings();
    fixes |= 1;
  }

  // validate memory
  u8 pageIndex = SAVEFILE_read8(SRAM->memory.pageIndex);
  u8 songIndex = SAVEFILE_read8(SRAM->memory.songIndex);
  u8 difficultyLevel = SAVEFILE_read8(SRAM->memory.difficultyLevel);
  u8 numericLevel = SAVEFILE_read8(SRAM->memory.numericLevel);
  u8 isAudioLagCalibrated = SAVEFILE_read8(SRAM->memory.isAudioLagCalibrated);
  if (pageIndex > librarySize / 4 || songIndex >= 4 || difficultyLevel >= 4 ||
      numericLevel >= 100 || isAudioLagCalibrated >= 2) {
    SAVEFILE_write8(SRAM->memory.pageIndex, 0);
    SAVEFILE_write8(SRAM->memory.songIndex, 0);
    SAVEFILE_write8(SRAM->memory.difficultyLevel, 0);
    SAVEFILE_write8(SRAM->memory.numericLevel, 0);
    SAVEFILE_write8(SRAM->memory.isAudioLagCalibrated, 0);
    fixes |= 0b10;
  }

  // validate progress
  u8 maxNormal =
      SAVEFILE_read8(SRAM->progress[DifficultyLevel::NORMAL].completedSongs);
  u8 maxHard =
      SAVEFILE_read8(SRAM->progress[DifficultyLevel::HARD].completedSongs);
  u8 maxCrazy =
      SAVEFILE_read8(SRAM->progress[DifficultyLevel::CRAZY].completedSongs);
  if (maxNormal > librarySize || maxHard > librarySize ||
      maxCrazy > librarySize) {
    SAVEFILE_write8(SRAM->progress[DifficultyLevel::NORMAL].completedSongs,
                    min(maxNormal, librarySize));
    SAVEFILE_write8(SRAM->progress[DifficultyLevel::HARD].completedSongs,
                    min(maxHard, librarySize));
    SAVEFILE_write8(SRAM->progress[DifficultyLevel::CRAZY].completedSongs,
                    min(maxCrazy, librarySize));
    fixes |= 0b100;
  }

  // validate state
  u8 isPlaying = SAVEFILE_read8(SRAM->state.isPlaying);
  u8 gameMode = SAVEFILE_read8(SRAM->state.gameMode);
  if (isPlaying >= 2 || gameMode >= 5) {
    SAVEFILE_write8(SRAM->state.isPlaying, 0);
    SAVEFILE_write8(SRAM->state.gameMode, 0);
    fixes |= 0b1000;
  }

  // validate arcade progress
  if (!ARCADE_isInitialized()) {
    ARCADE_initialize();
    fixes |= 0b10000;
  }

  // validate admin settings
  u8 arcadeCharts = SAVEFILE_read8(SRAM->adminSettings.arcadeCharts);
  u8 rumble = SAVEFILE_read8(SRAM->adminSettings.rumble);
  u8 ioBlink = SAVEFILE_read8(SRAM->adminSettings.ioBlink);
  u8 sramBlink = SAVEFILE_read8(SRAM->adminSettings.sramBlink);
  u8 navigationStyle = SAVEFILE_read8(SRAM->adminSettings.navigationStyle);
  u8 offsetEditingEnabled =
      SAVEFILE_read8(SRAM->adminSettings.offsetEditingEnabled);
  if (arcadeCharts >= 2 || rumble >= 2 || ioBlink >= 3 || sramBlink >= 3 ||
      navigationStyle >= 2 || offsetEditingEnabled >= 2) {
    SAVEFILE_resetAdminSettings();
    fixes |= 0b100000;
  }

  // validate mods
  u8 multiplier = SAVEFILE_read8(SRAM->mods.multiplier);
  u8 stageBreak = SAVEFILE_read8(SRAM->mods.stageBreak);
  u8 pixelate = SAVEFILE_read8(SRAM->mods.pixelate);
  u8 jump = SAVEFILE_read8(SRAM->mods.jump);
  u8 reduce = SAVEFILE_read8(SRAM->mods.reduce);
  u8 bounce = SAVEFILE_read8(SRAM->mods.bounce);
  u8 colorFilter = SAVEFILE_read8(SRAM->mods.colorFilter);
  u8 speedHack = SAVEFILE_read8(SRAM->mods.speedHack);
  u8 mirrorSteps = SAVEFILE_read8(SRAM->mods.mirrorSteps);
  u8 randomSteps = SAVEFILE_read8(SRAM->mods.randomSteps);
  u8 autoMod = SAVEFILE_read8(SRAM->mods.autoMod);
  u8 trainingMode = SAVEFILE_read8(SRAM->mods.trainingMode);
  if (multiplier < 1 || multiplier > 6 || stageBreak >= 3 || pixelate >= 6 ||
      jump >= 3 || reduce >= 5 || bounce >= 3 || colorFilter >= 17 ||
      speedHack >= 3 || mirrorSteps >= 2 || randomSteps >= 2 || autoMod >= 3 ||
      trainingMode >= 3) {
    SAVEFILE_resetMods();
    fixes |= 0b1000000;
  }

  // validate offsets
  if (!OFFSET_isInitialized()) {
    OFFSET_initialize();
    fixes |= 0b10000000;
  }

  return fixes;
}

inline u32 SAVEFILE_initialize(const GBFS_FILE* fs) {
  u32 romId = as_le((u8*)gbfs_get_obj(fs, ROM_ID_FILE, NULL));
  u32 librarySize = romId & LIBRARY_SIZE_MASK;
  bool isNew =
      (SAVEFILE_read32(SRAM->romId) & ROM_ID_MASK) != (romId & ROM_ID_MASK);

  // write rom id
  SAVEFILE_write32(SRAM->romId, romId);

  // create save file if needed
  if (isNew) {
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

    SAVEFILE_write8(SRAM->state.isPlaying, false);
    SAVEFILE_write8(SRAM->state.gameMode, GameMode::CAMPAIGN);

    ARCADE_initialize();
    OFFSET_initialize();
    SAVEFILE_resetAdminSettings();
  }

  return SAVEFILE_normalize(librarySize);
}

inline bool SAVEFILE_isWorking(const GBFS_FILE* fs) {
  u32 romId = as_le((u8*)gbfs_get_obj(fs, ROM_ID_FILE, NULL));
  return SAVEFILE_read32(SRAM->romId) == romId;
}

inline bool SAVEFILE_isUsingGBAStyle() {
  return static_cast<NavigationStyleOpts>(SAVEFILE_read8(
             SRAM->adminSettings.navigationStyle)) == NavigationStyleOpts::GBA;
}

inline u8 SAVEFILE_getLibrarySize() {
  return SAVEFILE_read32(SRAM->romId) & LIBRARY_SIZE_MASK;
}

inline GameMode SAVEFILE_getGameMode() {
  return static_cast<GameMode>(SAVEFILE_read8(SRAM->state.gameMode));
}

inline bool SAVEFILE_isPlayingSinglePlayerDouble() {
  auto gameMode = SAVEFILE_getGameMode();
  auto arcadeCharts = static_cast<ArcadeChartsOpts>(
      SAVEFILE_read8(SRAM->adminSettings.arcadeCharts));

  return gameMode == GameMode::ARCADE &&
         arcadeCharts == ArcadeChartsOpts::DOUBLE;
}

inline bool SAVEFILE_isPlayingDouble() {
  return SAVEFILE_getGameMode() == GameMode::MULTI_COOP ||
         SAVEFILE_isPlayingSinglePlayerDouble();
}

inline u8 SAVEFILE_getMaxCompletedSongs() {
  u8 a = SAVEFILE_read8(SRAM->progress[DifficultyLevel::NORMAL].completedSongs);
  u8 b = SAVEFILE_read8(SRAM->progress[DifficultyLevel::HARD].completedSongs);
  u8 c = SAVEFILE_read8(SRAM->progress[DifficultyLevel::CRAZY].completedSongs);

  return MATH_max(a, b, c);
}

inline bool SAVEFILE_isModeUnlocked(GameMode gameMode) {
  if (ENV_ARCADE || ENV_DEVELOPMENT)
    return true;

  u8 maxCompletedSongs = SAVEFILE_getMaxCompletedSongs();

  if (gameMode == GameMode::ARCADE || gameMode == GameMode::MULTI_VS ||
      gameMode == GameMode::MULTI_COOP)
    return maxCompletedSongs >= 1;

  if (gameMode == GameMode::IMPOSSIBLE)
    return maxCompletedSongs >= SAVEFILE_getLibrarySize();

  return true;
}

inline u32 SAVEFILE_getCompletedSongsOf(GameMode gameMode,
                                        DifficultyLevel difficultyLevel) {
  return SAVEFILE_read8(
      SRAM->progress[(gameMode == GameMode::IMPOSSIBLE) * PROGRESS_IMPOSSIBLE +
                     difficultyLevel]
          .completedSongs);
}

inline u32 SAVEFILE_getCompletedSongsOf(DifficultyLevel difficultyLevel) {
  return SAVEFILE_getCompletedSongsOf(SAVEFILE_getGameMode(), difficultyLevel);
}

inline bool SAVEFILE_didComplete(GameMode gameMode,
                                 DifficultyLevel difficultyLevel) {
  auto completedSongs = SAVEFILE_getCompletedSongsOf(gameMode, difficultyLevel);
  auto librarySize = SAVEFILE_getLibrarySize();

  return completedSongs >= librarySize;
}

inline DifficultyLevel SAVEFILE_getMaxLibraryType() {
  DifficultyLevel maxLevel = DifficultyLevel::NORMAL;
  u32 max = 0;

  for (u32 i = 0; i < MAX_DIFFICULTY + 1; i++) {
    auto difficultyLevel = static_cast<DifficultyLevel>(i);
    auto completedSongs = SAVEFILE_getCompletedSongsOf(difficultyLevel);

    if (completedSongs >= max) {
      maxLevel = difficultyLevel;
      max = completedSongs;
    }
  }

  return maxLevel;
}

inline GradeType SAVEFILE_getStoryGradeOf(u8 songIndex, DifficultyLevel level) {
  auto gameMode = SAVEFILE_getGameMode();

  u32 index = (gameMode == GameMode::IMPOSSIBLE) * PROGRESS_IMPOSSIBLE + level;
  int lastIndex = SAVEFILE_read8(SRAM->progress[index].completedSongs) - 1;
  if (songIndex > lastIndex)
    return GradeType::UNPLAYED;

  return static_cast<GradeType>(
      SAVEFILE_read8(SRAM->progress[index].grades[songIndex]));
}

inline GradeType SAVEFILE_getArcadeGradeOf(u8 songId, u8 numericLevel) {
  return SAVEFILE_isPlayingDouble() ? ARCADE_readDouble(songId, numericLevel)
                                    : ARCADE_readSingle(songId, numericLevel);
}

inline GradeType SAVEFILE_toggleDefectiveGrade(u8 currentGrade) {
  return currentGrade == GradeType::DEFECTIVE ? GradeType::UNPLAYED
                                              : GradeType::DEFECTIVE;
}

inline bool SAVEFILE_setGradeOf(u8 songIndex,
                                DifficultyLevel level,
                                u8 songId,
                                u8 numericLevel,
                                GradeType grade) {
  if (SAVEFILE_isPlayingDouble()) {
    u8 currentGrade = ARCADE_readDouble(songId, numericLevel);

    if (grade == GradeType::DEFECTIVE) {
      ARCADE_writeDouble(songId, numericLevel,
                         SAVEFILE_toggleDefectiveGrade(currentGrade));
      return false;
    }

    if (GameState.mods.stageBreak == StageBreakOpts::sOFF ||
        GameState.mods.trainingMode != TrainingModeOpts::tOFF)
      return false;

    if (grade < currentGrade)
      ARCADE_writeDouble(songId, numericLevel, grade);

    return false;
  }

  auto gameMode = SAVEFILE_getGameMode();

  switch (gameMode) {
    {
      case GameMode::CAMPAIGN:
      case GameMode::IMPOSSIBLE:
        u32 index =
            (gameMode == GameMode::IMPOSSIBLE) * PROGRESS_IMPOSSIBLE + level;
        int lastIndex =
            SAVEFILE_read8(SRAM->progress[index].completedSongs) - 1;
        u8 librarySize = SAVEFILE_getLibrarySize();
        bool firstTime = songIndex > lastIndex;

        if (firstTime) {
          auto nextSongIndex = (u8)min(songIndex + 1, librarySize - 1);
          auto completedSongs = (u8)min(songIndex + 1, librarySize);
          SAVEFILE_write8(SRAM->progress[index].completedSongs, completedSongs);
          SAVEFILE_write8(SRAM->memory.pageIndex, Div(nextSongIndex, 4));
          SAVEFILE_write8(SRAM->memory.songIndex, DivMod(nextSongIndex, 4));
        }

        u8 currentGrade =
            SAVEFILE_read8(SRAM->progress[index].grades[songIndex]);
        if (firstTime || grade < currentGrade)
          SAVEFILE_write8(SRAM->progress[index].grades[songIndex], grade);

        return songIndex == SAVEFILE_getLibrarySize() - 1;
    }
    {
      case GameMode::ARCADE:
      case GameMode::MULTI_VS:
        u8 currentGrade = ARCADE_readSingle(songId, numericLevel);

        if (grade == GradeType::DEFECTIVE) {
          ARCADE_writeSingle(songId, numericLevel,
                             SAVEFILE_toggleDefectiveGrade(currentGrade));
          return false;
        }

        if (GameState.mods.stageBreak == StageBreakOpts::sOFF ||
            GameState.mods.trainingMode != TrainingModeOpts::tOFF)
          return false;

        if (grade < currentGrade)
          ARCADE_writeSingle(songId, numericLevel, grade);

        return false;
    }
    default: {
    }
  }

  return false;
}

inline void SAVEFILE_resetOffsets() {
  OFFSET_initialize();
}

inline void SAVEFILE_resetArcade() {
  ARCADE_initialize();
}

inline void SAVEFILE_reset() {
  SAVEFILE_resetArcade();
  SAVEFILE_write32(SRAM->romId, 0);
}

#endif  // SAVE_FILE_H
