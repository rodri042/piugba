#ifndef PIXEL_BLINK_H
#define PIXEL_BLINK_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#include <functional>

class PixelBlink {
 public:
  PixelBlink(u32 targetValue);

  void blink();
  void blink(u32 customTargetValue);

  inline bool tick() { return tick(0); }
  bool tick(u8 minValue);

 private:
  u32 targetValue;
  u32 baseTargetValue;
  u32 step = 0;
  bool isBlinking = false;
};

#endif  // PIXEL_BLINK_H
