#include "LinkUniversal.hpp"
#include "gameplay/multiplayer/PS2Keyboard.h"  // [!]

// [!]
// This library has some tweaks (marked with "[!]") for piuGBA.
// You should check out the gba-link-connection's original code instead of this.

#pragma GCC push_options
#pragma GCC optimize("Os")

LINK_WIRELESS_CODE_IWRAM void LINK_UNIVERSAL_ISR_SERIAL() {
  if (linkUniversal->isActive())
    linkUniversal->_onSerial();
  else if (ps2Keyboard->isActive())
    ps2Keyboard->_onSerial();
}

LINK_WIRELESS_CODE_IWRAM void LINK_UNIVERSAL_ISR_TIMER() {
  linkUniversal->_onTimer();
}

#pragma GCC pop_options
