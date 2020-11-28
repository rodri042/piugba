#include "ArrowSelector.h"

#include <libgba-sprite-engine/gba/tonc_math.h>
#include <libgba-sprite-engine/sprites/sprite_builder.h>

#include "data/content/_compiled_sprites/spr_arrows.h"
#include "utils/SpriteUtils.h"

const u32 ANIMATION_FRAMES = 5;
const u32 ANIMATION_DELAY = 2;
const u32 AUTOFIRE_SWITCH[] = {50, 100, 150, 200};
const u32 AUTOFIRE_DELAY[] = {25, 15, 10, 5};
const u32 AUTOFIRE_SPEEDS = 4;

ArrowSelector::ArrowSelector(ArrowDirection direction,
                             bool reuseTiles,
                             bool reactive) {
  u32 start = 0;
  bool flip = false;
  ARROW_initialize(direction, start, flip);
  this->direction = direction;
  this->start = start;
  this->flip = flip;
  this->reactive = reactive;

  SpriteBuilder<Sprite> builder;
  sprite = builder.withData(spr_arrowsTiles, sizeof(spr_arrowsTiles))
               .withSize(SIZE_16_16)
               .withAnimated(start, ANIMATION_FRAMES, ANIMATION_DELAY)
               .withLocation(0, 0)
               .buildPtr();

  if (reuseTiles)
    SPRITE_reuseTiles(sprite.get());
}

bool ArrowSelector::shouldFireEvent() {
  bool isPressed = getIsPressed();
  if (!isPressed)
    return false;

  if (hasBeenPressedNow()) {
    globalLastPressFrame = 0;
    currentLastPressFrame = 0;
    autoFireSpeed = 0;
    return true;
  }

  // autofire
  bool itsTime = currentLastPressFrame >= AUTOFIRE_DELAY[autoFireSpeed];
  bool canIncreaseSpeed = autoFireSpeed < AUTOFIRE_SPEEDS - 1;
  bool shouldIncrease = globalLastPressFrame >= AUTOFIRE_SWITCH[autoFireSpeed];
  if (itsTime)
    currentLastPressFrame = 0;
  if (canIncreaseSpeed && shouldIncrease) {
    currentLastPressFrame = 0;
    autoFireSpeed++;
  }
  return itsTime;
}

void ArrowSelector::tick() {
  sprite->flipHorizontally(flip);

  globalLastPressFrame++;
  currentLastPressFrame++;

  if (!reactive)
    return;

  u32 currentFrame = sprite->getCurrentFrame();

  if (isPressed && currentFrame < start + ARROW_HOLDER_PRESSED) {
    SPRITE_goToFrame(sprite.get(),
                     max(currentFrame + 1, start + ARROW_HOLDER_IDLE + 1));
  } else if (!isPressed && currentFrame >= start + ARROW_HOLDER_IDLE) {
    sprite->makeAnimated(start, ANIMATION_FRAMES, ANIMATION_DELAY);
  }
}
