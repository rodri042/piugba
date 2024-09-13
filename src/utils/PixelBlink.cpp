#include "PixelBlink.h"

#include <libgba-sprite-engine/gba/tonc_math.h>

#include "EffectUtils.h"

PixelBlink::PixelBlink(u32 targetValue) {
  this->targetValue = targetValue;
  this->baseTargetValue = targetValue;
}

void PixelBlink::blink() {
  targetValue = baseTargetValue;
  step = 0;
  isBlinking = true;
}

void PixelBlink::blink(u32 customTargetValue) {
  targetValue = customTargetValue;
  step = 0;
  isBlinking = true;
}

bool PixelBlink::tick(u8 minValue) {
  if (!isBlinking && step == 0)
    return false;

  bool hasEnded = false;

  if (isBlinking) {
    step++;
    if (step == targetValue) {
      isBlinking = false;
      hasEnded = true;
    }
  } else
    step--;

  EFFECT_setMosaic(max(step, minValue));

  return hasEnded;
}
