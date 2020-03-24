#include "Arrow.h"
#include <libgba-sprite-engine/gba/tonc_core.h>  // TODO: REMOVE (qran_range)
#include <libgba-sprite-engine/sprites/sprite_builder.h>
#include "data/content/compiled/spr_arrows.h"
#include "utils/SpriteUtils.h";

const u32 ANIMATION_FRAMES = 5;
const u32 ANIMATION_DELAY = 2;
const u32 END_ANIMATION_START = 5;
const u32 END_ANIMATION_DELAY_MS = 30;

Arrow::Arrow(u32 id) {
  SpriteBuilder<Sprite> builder;
  sprite = builder.withData(spr_arrowsTiles, sizeof(spr_arrowsTiles))
               .withSize(SIZE_16_16)
               .withAnimated(ANIMATION_FRAMES, ANIMATION_DELAY)
               .withLocation(HIDDEN_WIDTH, HIDDEN_HEIGHT)
               .buildPtr();
  sprite->enabled = false;

  if (id > 0)
    SPRITE_reuseTiles(sprite.get());
}

void Arrow::initialize(ArrowType type) {
  u32 start;
  bool flip = false;
  switch (type) {
    case ArrowType::DOWNLEFT:
      start = ARROW_FRAMES * 0;
      break;
    case ArrowType::UPLEFT:
      start = ARROW_FRAMES * 1;
      break;
    case ArrowType::CENTER:
      start = ARROW_FRAMES * 2;
      break;
    case ArrowType::UPRIGHT:
      start = ARROW_FRAMES * 1;
      flip = true;
      break;
    case ArrowType::DOWNRIGHT:
      start = ARROW_FRAMES * 0;
      flip = true;
      break;
  }

  this->type = type;
  this->start = start;
  this->flip = flip;

  sprite->moveTo(ARROW_CORNER_MARGIN + ARROW_MARGIN * type, GBA_SCREEN_HEIGHT);
  sprite->makeAnimated(this->start, ANIMATION_FRAMES, ANIMATION_DELAY);
  sprite->enabled = true;
  endTime = 0;
  feedbackType = FeedbackType::ACTIVE;
}

void Arrow::discard() {
  sprite->enabled = false;
}

FeedbackType Arrow::tick(u32 millis, bool isPressed) {
  sprite->flipHorizontally(flip);

  if (SPRITE_isHidden(sprite.get()))
    return feedbackType;

  if (endTime > 0) {
    u32 diff = abs(millis - endTime);

    if (diff > END_ANIMATION_DELAY_MS) {
      if (diff < END_ANIMATION_DELAY_MS * 2)
        SPRITE_goToFrame(sprite.get(), this->start + END_ANIMATION_START + 1);
      else if (diff < END_ANIMATION_DELAY_MS * 3)
        SPRITE_goToFrame(sprite.get(), this->start + END_ANIMATION_START + 2);
      else if (diff < END_ANIMATION_DELAY_MS * 4)
        SPRITE_goToFrame(sprite.get(), this->start + END_ANIMATION_START + 3);
      else {
        SPRITE_hide(sprite.get());
        sprite->stopAnimating();
      }
    }
  } else if (abs(sprite->getY() - ARROW_CORNER_MARGIN) < 3) {
    endTime = millis;
    feedbackType = static_cast<FeedbackType>(
        qran_range(0, 5));  // TODO: Use isPressed or remove
    SPRITE_goToFrame(sprite.get(), this->start + END_ANIMATION_START);
  } else
    sprite->moveTo(sprite->getX(), sprite->getY() - 3);

  return FeedbackType::ACTIVE;
}

Sprite* Arrow::get() {
  return sprite.get();
}
