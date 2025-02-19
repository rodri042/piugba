#include "../LinkUniversal.hpp"
#include "gameplay/multiplayer/PS2Keyboard.h"  // [!]

#define CODE_ROM __attribute__((section(".code")))

// [!]
// This library has some tweaks (marked with "[!]") for piuGBA.
// You should check out the gba-link-connection's original code instead of this.

#pragma GCC push_options
#pragma GCC optimize("Os")

CODE_ROM void LINK_UNIVERSAL_ISR_VBLANK() {
  linkUniversal->_onVBlank();
}

LINK_CODE_IWRAM void LINK_UNIVERSAL_ISR_SERIAL() {
  if (linkUniversal->isActive())
    linkUniversal->_onSerial();
  else if (ps2Keyboard->isActive())
    ps2Keyboard->_onSerial();
}

LINK_CODE_IWRAM void LINK_UNIVERSAL_ISR_TIMER() {
  if (linkUniversal->isActive())
    linkUniversal->_onTimer();
}

#pragma GCC pop_options
