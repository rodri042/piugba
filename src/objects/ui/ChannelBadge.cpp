#include "ChannelBadge.h"

#include <libgba-sprite-engine/sprites/sprite_builder.h>

#include "data/content/_compiled_sprites/spr_channels.h"
#include "utils/SpriteUtils.h"

#define ANIMATION_TIME 6

ChannelBadge::ChannelBadge(u32 x, u32 y, bool reuseTiles) {
  SpriteBuilder<Sprite> builder;
  sprite = builder.withData(spr_channelsTiles, sizeof(spr_channelsTiles))
               .withSize(SIZE_16_16)
               .withLocation(x, y)
               .buildPtr();

  this->x = x;
  this->y = y;

  SPRITE_goToFrame(sprite.get(), 0);

  if (reuseTiles)
    SPRITE_reuseTiles(sprite.get());
}

void ChannelBadge::tick() {
  if (currentType != Channel::BOSS)
    return;

  animationCount++;
  if (animationCount >= ANIMATION_TIME) {
    animationCount = 0;
    animationFlag = !animationFlag;
    sprite->flipHorizontally(animationFlag);
  }
}

void ChannelBadge::setType(Channel type) {
  currentType = type;
  SPRITE_goToFrame(sprite.get(), type);
  sprite->moveTo(x, y);
  sprite->flipHorizontally(false);
  animationCount = 0;
  animationFlag = 0;
}

void ChannelBadge::hide() {
  SPRITE_hide(sprite.get());
}
