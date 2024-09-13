#ifndef DIGIT_H
#define DIGIT_H

#include <libgba-sprite-engine/sprites/sprite.h>

#include "objects/base/AnimatedIndicator.h"

enum DigitSize { BIG, MINI, MINI_NARROW };

class Digit : public AnimatedIndicator {
 public:
  Digit(DigitSize size, u32 x, u32 y, u32 index, bool reuseTiles);

  void set(u32 value, bool isRed);
  void relocate(u32 x, u32 y, u32 spacing = 26);
  void reloadPosition(u32 x, u32 y, u32 spacing = 26);
  bool shouldBeVisible() { return currentIndex != 0 || currentValue != 0; }

  Sprite* get() override;

 private:
  std::unique_ptr<Sprite> sprite;
  DigitSize size;
  u32 currentIndex;
  u32 currentValue = 0;
  bool modern = false;
};

#endif  // DIGIT_H
