#ifndef IOPORT_H
#define IOPORT_H

#include <libgba-sprite-engine/gba/tonc_core.h>

inline void IOPORT_scHigh() {
  REG_RCNT |= 0b1;
}

inline void IOPORT_scLow() {
  REG_RCNT &= 0b1111111111111110;
}

inline void IOPORT_sdHigh() {
  REG_RCNT |= 0b10;
}

inline void IOPORT_left() {  // (SO)
  REG_RCNT |= 0b1000;
}

inline void IOPORT_right() {  // (SD)
  REG_RCNT |= 0b10;
}

inline void IOPORT_top() {  // (SC)
  REG_RCNT |= 0b1;
}

inline void IOPORT_sdLow() {
  REG_RCNT &= 0b1111111111110100;
}

inline void IOPORT_low() {
  REG_RCNT &= 0b1111111111110100;
}

#endif  // IOPORT_H
