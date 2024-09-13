#include "VideoStore.h"

#include <libgba-sprite-engine/gba/tonc_math.h>

#include "gameplay/models/Song.h"
#include "gameplay/save/SaveFile.h"
#include "objects/ArrowInfo.h"
#include "player/PlaybackState.h"
#include "utils/MathUtils.h"

extern "C" {
#include "utils/flashcartio/flashcartio.h"
}

#define VIDEO_SECTOR 512
#define CLMT_ENTRIES 1024
#define SIZE_CLMT (CLMT_ENTRIES * sizeof(u32))
#define SIZE_HALF_FRAME \
  (VIDEO_SIZE_PALETTE + VIDEO_SIZE_MAP + VIDEO_SIZE_TILES / 2)
#define REQUIRED_MEMORY (SIZE_CLMT + SIZE_HALF_FRAME)

const u32 FRACUMUL_MS_TO_FRAME_AT_30FPS = 128849018;  // (*30/1000)

DATA_EWRAM static FATFS fatfs;
DATA_EWRAM static FIL file;

HQModeOpts getMode() {
  return static_cast<HQModeOpts>(SAVEFILE_read8(SRAM->adminSettings.hqMode));
}

bool VideoStore::isEnabled() {
  return SAVEFILE_read8(SRAM->adminSettings.hqMode);
}

bool VideoStore::isActivating() {
  return getMode() == HQModeOpts::dACTIVATING;
}

void VideoStore::disable() {
  SAVEFILE_write8(SRAM->adminSettings.hqMode, HQModeOpts::dOFF);
}

VideoStore::State VideoStore::activate() {
  setState(OFF);
  auto previousMode = getMode();
  SAVEFILE_write8(SRAM->adminSettings.hqMode, HQModeOpts::dACTIVATING);

  auto result = flashcartio_activate();
  if (result != FLASHCART_ACTIVATED) {
    disable();
    return setState((result == FLASHCART_ACTIVATION_FAILED
                         ? MOUNT_ERROR
                         : NO_SUPPORTED_FLASHCART));
  }

  if (f_mount(&fatfs, "", 1) > 0) {
    disable();
    return setState(MOUNT_ERROR);
  }

  SAVEFILE_write8(SRAM->adminSettings.hqMode, previousMode > HQModeOpts::dACTIVE
                                                  ? previousMode
                                                  : HQModeOpts::dACTIVE);
  setState(ACTIVE, &fatfs);

  return state;
}

VideoStore::LoadResult VideoStore::load(std::string videoPath,
                                        int videoOffset) {
  if (state != ACTIVE)
    return LoadResult::NO_FILE;
  if (getMode() == HQModeOpts::dAUDIO_ONLY)
    return LoadResult::NO_FILE;

  memory = getSecondaryMemory(REQUIRED_MEMORY);
  if (memory == NULL)
    return LoadResult::NO_FILE;

  auto result =
      f_open(&file, (VIDEOS_FOLDER_NAME + videoPath).c_str(), FA_READ);
  if (result > 0)
    return result == FR_NO_FILE || result == FR_NO_PATH ? LoadResult::NO_FILE
                                                        : LoadResult::ERROR;

  isPlaying = true;
  frame = 0;
  this->videoOffset = videoOffset;

  file.cltbl = (DWORD*)memory;
  file.cltbl[0] = CLMT_ENTRIES;
  if (f_lseek(&file, CREATE_LINKMAP) > 0) {
    unload();
    return LoadResult::ERROR;
  }

  if (!seek(0))
    return LoadResult::ERROR;

  return LoadResult::OK;
}

void VideoStore::unload() {
  if (!isPlaying)
    return;

  f_close(&file);
  isPlaying = false;
}

bool VideoStore::seek(u32 msecs) {
  frame = MATH_fracumul(msecs, FRACUMUL_MS_TO_FRAME_AT_30FPS) -
          SGN(videoOffset) *
              MATH_fracumul(ABS(videoOffset), FRACUMUL_MS_TO_FRAME_AT_30FPS);
  frameLatch = false;
  cursor = frame > 0 ? frame * VIDEO_SIZE_FRAME / VIDEO_SECTOR : 0;
  memoryCursor = 0;

  if (f_lseek(&file, cursor * VIDEO_SECTOR) > 0) {
    unload();
    return false;
  }

  return true;
}

CODE_IWRAM bool VideoStore::preRead() {
  u32 readBytes;
  bool success =
      f_read(&file, memory + SIZE_CLMT, SIZE_HALF_FRAME, &readBytes) == 0;
  cursor += SIZE_HALF_FRAME / VIDEO_SECTOR;
  memoryCursor = 0;
  return success;
}

CODE_IWRAM bool VideoStore::endRead(u8* buffer, u32 sectors) {
  u32 readFromMemory = 0;

  while (sectors > 0) {
    u32 availableMemory = SIZE_HALF_FRAME - memoryCursor;
    u32 pendingBytes = sectors * VIDEO_SECTOR;

    if (availableMemory > 0) {
      u32 readableBytes = min(availableMemory, pendingBytes);
      dma3_cpy(buffer, memory + SIZE_CLMT + memoryCursor, readableBytes);
      memoryCursor += readableBytes;
      readFromMemory += readableBytes;
      sectors -= readableBytes / VIDEO_SECTOR;
    } else {
      u32 readBytes;
      if (f_read(&file, buffer + readFromMemory, pendingBytes, &readBytes) > 0)
        return false;
      cursor += sectors;
      sectors = 0;
    }
  }

  return true;
}

VideoStore::State VideoStore::setState(State newState, void* fatfs) {
  state = newState;
  PlaybackState.isPCMDisabled = getMode() == HQModeOpts::dVIDEO_ONLY;
  PlaybackState.fatfs = fatfs;
  return newState;
}
