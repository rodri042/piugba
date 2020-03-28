#ifndef ARROW_HOLDER_H
#define ARROW_HOLDER_H

#include <libgba-sprite-engine/sprites/sprite.h>
#include "Arrow.h"

const u32 ARROW_HOLDER_IDLE = 5;
const u32 ARROW_HOLDER_PRESSED = 7;

class ArrowHolder {
 public:
  ArrowType type;

  ArrowHolder(ArrowType type);

  void blink();
  bool getIsPressed();
  bool setIsPressed(bool isPressed);

  void tick();
  Sprite* get();

 private:
  std::unique_ptr<Sprite> sprite;
  u32 start = 0;
  bool flip = false;
  bool isPressed = false;
  bool isBlinking = false;
};

#endif  // ARROW_HOLDER_H
