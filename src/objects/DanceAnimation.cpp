#include "DanceAnimation.h"
#include <libgba-sprite-engine/sprites/sprite_builder.h>
#include "data/content/compiled/spr_arrow_center.h"
#include "utils/SpriteUtils.h"

const u32 STEPS = 4;

DanceAnimation::DanceAnimation(u32 x, u32 y) {
  SpriteBuilder<Sprite> builder;

  sprite =
      builder
          .withSize(SIZE_16_16)
          .withAnimated(5, 2)
          .withLocation(x, y)
          .buildPtr();

  SpriteUtils::reuseTiles(sprite.get());
}

void DanceAnimation::update(u32 beat) {
  int delta = sgn(velocity);
  count += delta;
  sprite->moveTo(sprite->getX() + velocity, sprite->getY());

  if (abs(count) >= STEPS) {
    velocity = -velocity;
    toLeft = !toLeft;
    sprite->flipHorizontally(toLeft);
  }
}

Sprite* DanceAnimation::get() {
  return sprite.get();
}
