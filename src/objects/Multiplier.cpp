#include "Multiplier.h"

#include <libgba-sprite-engine/gba/tonc_bios.h>
#include <libgba-sprite-engine/sprites/sprite_builder.h>

#include "data/content/_compiled_sprites/spr_multipliers.h"
#include "utils/SpriteUtils.h"

const u32 POSITION_X = 111;
const u32 POSITION_Y = 34;
const u32 BLINK_FRAME = 5;

Multiplier::Multiplier() {
  SpriteBuilder<Sprite> builder;
  sprite = builder.withData(spr_multipliersTiles, sizeof(spr_multipliersTiles))
               .withSize(SIZE_16_16)
               .withLocation(POSITION_X, POSITION_Y)
               .buildPtr();

  SPRITE_goToFrame(sprite.get(), getFrame());
}

void Multiplier::change() {
  value = DivMod(((value - 1) + 1), ARROW_MAX_MULTIPLIER) + 1;
  SPRITE_goToFrame(sprite.get(), BLINK_FRAME);
}

void Multiplier::tick() {
  isNewPressEvent = false;

  if (sprite->getCurrentFrame() == BLINK_FRAME)
    SPRITE_goToFrame(sprite.get(), getFrame());
}
