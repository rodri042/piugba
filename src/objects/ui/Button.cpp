#include "Button.h"

#include <libgba-sprite-engine/sprites/sprite_builder.h>

#include "data/content/_compiled_sprites/spr_buttons.h"
#include "data/content/_compiled_sprites/spr_buttons_mini.h"
#include "data/content/_compiled_sprites/spr_level.h"
#include "utils/SpriteUtils.h"

const u32 SELECTED_START = 3;

Button::Button(ButtonType type, u32 x, u32 y, bool reuseTiles) {
  SpriteBuilder<Sprite> builder;
  sprite = builder
               .withData(
                   type == ButtonType::LEVEL_METER       ? spr_levelTiles
                   : type >= ButtonType::SUB_BUTTON_BLUE ? spr_buttons_miniTiles
                                                         : spr_buttonsTiles,
                   type == ButtonType::LEVEL_METER ? sizeof(spr_levelTiles)
                   : type >= ButtonType::SUB_BUTTON_BLUE
                       ? sizeof(spr_buttons_miniTiles)
                       : sizeof(spr_buttonsTiles))
               .withSize(type >= ButtonType::SUB_BUTTON_BLUE ? SIZE_16_16
                                                             : SIZE_32_32)
               .withLocation(x, y)
               .buildPtr();

  this->type = type;
  this->x = x;
  this->y = y;

  if (reuseTiles)
    SPRITE_reuseTiles(sprite.get());

  if (type != ButtonType::LEVEL_METER && type != ButtonType::SUB_BUTTON_GRAY &&
      type != ButtonType::SUB_BUTTON_ORANGE)
    SPRITE_goToFrame(sprite.get(), type);
  if (type == SUB_BUTTON_BLUE)
    SPRITE_goToFrame(sprite.get(), 0);
  if (type == SUB_BUTTON_ORANGE)
    SPRITE_goToFrame(sprite.get(), 4);
}

void Button::setSelected(bool isSelected) {
  if (type == ButtonType::SUB_BUTTON_BLUE) {
    SPRITE_goToFrame(sprite.get(), isSelected ? 1 : 0);
    return;
  }
  if (type == ButtonType::SUB_BUTTON_GRAY) {
    SPRITE_goToFrame(sprite.get(), isSelected ? 3 : 2);
    return;
  }
  if (type == ButtonType::SUB_BUTTON_ORANGE) {
    SPRITE_goToFrame(sprite.get(), isSelected ? 5 : 4);
    return;
  }

  SPRITE_goToFrame(sprite.get(), (isSelected ? SELECTED_START : 0) + type);
}

void Button::show() {
  sprite->moveTo(x, y);
}

void Button::hide() {
  SPRITE_hide(get());
}
