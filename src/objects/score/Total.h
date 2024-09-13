#ifndef TOTAL_H
#define TOTAL_H

#include <libgba-sprite-engine/sprites/sprite.h>

#include <vector>

#include "objects/Digit.h"

class Total {
 public:
  Total(u32 x, u32 y, bool has4Digits, bool isFirst);

  void setValue(u32 value);
  void render(std::vector<Sprite*>* sprites);

  ~Total();

 private:
  std::vector<std::unique_ptr<Digit>> digits;
};

#endif  // TOTAL_H
