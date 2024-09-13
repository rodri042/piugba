#include "LinkWireless.hpp"

// [!]
// This library has some tweaks (marked with "[!]") for piuGBA.
// You should check out the gba-link-connection's original code instead of this.

// [!]
#pragma GCC push_options
#pragma GCC optimize("Os")

#ifdef LINK_WIRELESS_PUT_ISR_IN_IWRAM

LINK_WIRELESS_CODE_IWRAM void LinkWireless::_onSerial() {
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  interrupt = true;
  LINK_WIRELESS_BARRIER;
  Link::_REG_IME = 1;
#endif

  __onSerial();

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  irqEnd();
#endif
}
LINK_WIRELESS_CODE_IWRAM void LinkWireless::_onTimer() {
#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  if (interrupt)
    return;

  interrupt = true;
  LINK_WIRELESS_BARRIER;
  Link::_REG_IME = 1;
#endif

  __onTimer();

#ifdef LINK_WIRELESS_ENABLE_NESTED_IRQ
  irqEnd();
#endif
}

#endif

#pragma GCC pop_options
