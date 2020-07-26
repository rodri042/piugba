#ifndef SAVE_FILE_H
#define SAVE_FILE_H

#include <libgba-sprite-engine/gba/tonc_core.h>
#include <libgba-sprite-engine/gba/tonc_memmap.h>

#include "Memory.h"
#include "Progress.h"
#include "Settings.h"
#include "assets.h"
#include "utils/parse.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

typedef struct {
  u32 romId;

  Settings settings;
  Memory memory;
  Progress progressNormal;
  Progress progressHard;
  Progress progressCrazy;
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

inline void SAVEFILE_initialize(const GBFS_FILE* fs) {
  u32 romId = as_le((u8*)gbfs_get_obj(fs, ROM_ID_FILE, NULL));

  if (SAVEFILE_read32(SRAM->romId) != romId) {
    SAVEFILE_write32(SRAM->romId, romId);

    SAVEFILE_write32(SRAM->settings.audioLag, 0);
    SAVEFILE_write8(SRAM->settings.showControls, 1);
    SAVEFILE_write8(SRAM->settings.holderPosition, HolderPosition::LEFT);
    SAVEFILE_write8(SRAM->settings.backgroundType,
                    BackgroundType::HALF_BGA_DARK);
    SAVEFILE_write8(SRAM->settings.bgaDarkBlink, true);

    SAVEFILE_write8(SRAM->memory.pageIndex, 0);
    SAVEFILE_write8(SRAM->memory.songIndex, 0);
    SAVEFILE_write8(SRAM->memory.difficultyLevel, 0);
    SAVEFILE_write8(SRAM->memory.multiplier, 3);

    SAVEFILE_write32(SRAM->progressNormal.completedSongs, 0);
    SAVEFILE_write32(SRAM->progressHard.completedSongs, 0);
    SAVEFILE_write32(SRAM->progressCrazy.completedSongs, 0);
  }
}

#endif  // SAVE_FILE_H
