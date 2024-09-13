#ifndef BG_SELECTIONMASK_H
#define BG_SELECTIONMASK_H

extern const unsigned int BG_SELECTIONMASK_TILES[][496];
extern const unsigned int BG_SELECTIONMASK_MAP[];
extern const unsigned int BG_SELECTIONMASK_PALETTE[];

const struct {
  const unsigned int TILES_BANK;
  const unsigned int TILES_START_INDEX;
  const unsigned int TILES_END_INDEX;
  const unsigned int TILES_LENGTH;
  const unsigned int MAP_BANK;
  const unsigned int MAP_START_INDEX;
  const unsigned int MAP_END_INDEX;
  const unsigned int MAP_TOTAL_TILES;
  const unsigned int PALETTE_START_INDEX;
  const unsigned int PALETTE_LENGTH;
} BG_SELECTIONMASK_METADATA = {2, 224, 256, 496, 18, 224, 416, 1024, 250, 4};

#endif  // BG_SELECTIONMASK_H
