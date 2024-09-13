#ifndef GBA_SPRITE_ENGINE_BACKGROUND_H
#define GBA_SPRITE_ENGINE_BACKGROUND_H

#pragma GCC system_header

#include <libgba-sprite-engine/gba/tonc_types.h>

#define MAPLAYOUT_32X32 0
#define MAPLAYOUT_32X64 1
#define MAPLAYOUT_64X32 2
#define MAPLAYOUT_64X64 3

class Background {
 public:
  Background(int bgIndex,
             const void* data,
             int size,
             const void* map,
             int mapSize,
             int screenBlockIndex,
             int charBlockIndex,
             int mapLayout,
             bool lz77)
      : Background(bgIndex, data, size, map, mapSize, lz77) {
    this->screenBlockIndex = screenBlockIndex;
    this->charBlockIndex = charBlockIndex;
    this->mapLayout = mapLayout;
  }

  Background(int bgIndex,
             const void* data,
             int size,
             const void* map,
             int mapSize,
             bool lz77 = false)
      : data(data),
        bgIndex(bgIndex),
        size(size),
        map(map),
        mapLayout(MAPLAYOUT_32X32),
        screenBlockIndex(0),
        charBlockIndex(bgIndex),
        priority(bgIndex),
        mapSize(mapSize),
        lz77(lz77) {}

  const int getScreenBlock() { return screenBlockIndex; }
  const int getCharBlock() { return charBlockIndex; }
  void useMapScreenBlock(int block) { screenBlockIndex = block; }
  void useCharBlock(int block) { charBlockIndex = block; }
  void usePriority(int value) { priority = value; }
  void setMosaic(bool enabled) { mosaicEnabled = enabled; }

  virtual void persist();
  void render();
  void clearMap();
  void clearData();
  void scroll(int x, int y);
  void scrollNow(int x, int y);

  template <typename F>
  void persistNow(F copyMemory) {
    copyMemory(screen_block(screenBlockIndex), this->mapSize);
    copyMemory(char_block(charBlockIndex), this->size);
    buildRegister();
  }

 protected:
  const void* data;
  const void* map;
  int size, bgIndex;
  int mapSize, mapLayout;
  int screenBlockIndex, charBlockIndex, priority;
  bool mosaicEnabled = false;
  bool lz77 = false;
  int scrollX = 0;
  int scrollY = 0;

 private:
  void buildRegister();
  u32 getBgControlRegisterIndex();

  // WHY using this instead of Allocation?
  // Because each char block seems to be 16K and there are 4 - there are also 4
  // backgrounds. Use the bgIndex as a hardcoded char block and let the
  // background decide on the map screen block.
  void* screen_block(unsigned long block) {
    return (void*)(0x6000000 + (block * 0x800));
  }

  void* char_block(unsigned long block) {
    return (void*)(0x6000000 + (block * 0x4000));
  }
};

#endif  // GBA_SPRITE_ENGINE_BACKGROUND_H
