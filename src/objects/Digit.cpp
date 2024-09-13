#include "Digit.h"

#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/sprites/sprite_builder.h>

#include "data/content/_compiled_sprites/spr_numbers.h"
#include "data/content/_compiled_sprites/spr_numbers_mdrn.h"
#include "data/content/_compiled_sprites/spr_numbers_mini.h"
#include "data/content/_compiled_sprites/spr_numbers_mini_mdrn.h"
#include "gameplay/save/SaveFile.h"
#include "utils/SpriteUtils.h"

const u32 DIGIT_WIDTHS[] = {26, 19, 13};
const u32 TOTAL_NUMBERS = 10;

Digit::Digit(DigitSize size, u32 x, u32 y, u32 index, bool reuseTiles) {
  this->modern = SAVEFILE_isUsingModernTheme();
  this->size = size;
  this->currentIndex = index;
  reloadPosition(x, y, DIGIT_WIDTHS[size]);
  animationDirection = -1;

  SpriteBuilder<Sprite> builder;
  sprite =
      builder
          .withData(size == DigitSize::BIG
                        ? (modern ? spr_numbers_mdrnTiles : spr_numbersTiles)
                        : (modern ? spr_numbers_mini_mdrnTiles
                                  : spr_numbers_miniTiles),
                    size == DigitSize::BIG
                        ? (modern ? sizeof(spr_numbers_mdrnTiles)
                                  : sizeof(spr_numbersTiles))
                        : (modern ? sizeof(spr_numbers_mini_mdrnTiles)
                                  : sizeof(spr_numbers_miniTiles)))
          .withSize(SIZE_32_16)
          .withLocation(HIDDEN_WIDTH, HIDDEN_HEIGHT)
          .buildPtr();

  if (reuseTiles || index > 0)
    SPRITE_reuseTiles(sprite.get());
}

void Digit::set(u32 value, bool isRed) {
  currentValue = value;
  SPRITE_goToFrame(
      sprite.get(),
      value + (!modern && size == DigitSize::MINI_NARROW ? TOTAL_NUMBERS : 0) +
          (isRed ? TOTAL_NUMBERS : 0));
}

void Digit::relocate(u32 x, u32 y, u32 spacing) {
  reloadPosition(x, y, spacing);

  if (!shouldBeVisible()) {
    SPRITE_hide(get());
    return;
  }
}

void Digit::reloadPosition(u32 x, u32 y, u32 spacing) {
  animationPositionX = x + currentIndex * spacing;
  animationPositionY = y;
}

Sprite* Digit::get() {
  return sprite.get();
}
