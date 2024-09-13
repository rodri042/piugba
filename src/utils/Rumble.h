#ifndef RUMBLE_H
#define RUMBLE_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#define IO_RW ((vu16*)0x80000C8)
#define IO_DIRECTION ((vu16*)0x80000C6)
#define IO_DATA ((vu16*)0x80000C4)

inline void RUMBLE_start() {
  *(vu16*)0x09FE0000 = 0xd200;
  *(vu16*)0x08000000 = 0x1500;
  *(vu16*)0x08020000 = 0xd200;
  *(vu16*)0x08040000 = 0x1500;
  *(vu16*)0x09E20000 = 0x7;
  *(vu16*)0x09FC0000 = 0x1500;

  *(vu16*)0x08001000 = 0x2;
}

inline void RUMBLE_stop() {
  *(vu16*)0x09FE0000 = 0xd200;
  *(vu16*)0x08000000 = 0x1500;
  *(vu16*)0x08020000 = 0xd200;
  *(vu16*)0x08040000 = 0x1500;
  *(vu16*)0x09E20000 = 0x8;
  *(vu16*)0x09FC0000 = 0x1500;

  *(vu16*)0x08001000 = 0x0;
}

inline void RUMBLE_init() {
  RUMBLE_stop();
}

#endif  // RUMBLE_H
