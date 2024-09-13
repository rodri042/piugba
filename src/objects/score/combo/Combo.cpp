#include "Combo.h"

#include <libgba-sprite-engine/gba/tonc_math.h>

#include "gameplay/multiplayer/Syncer.h"
#include "objects/ArrowInfo.h"
#include "utils/SpriteUtils.h"

const u32 DIGITS_POSITION_X = 7;
const u32 DIGITS_POSITION_Y = 91;
const u32 NUMBER_WIDTH_3_DIGITS = 26;
const u32 NUMBER_WIDTH_4_DIGITS = 22;
const int OFFSET_3_DIGITS = -NUMBER_WIDTH_3_DIGITS;
const int OFFSET_4_DIGITS = -7;

Combo::Combo(u8 playerId) {
  this->playerId = playerId;

  title = std::unique_ptr<ComboTitle>{new ComboTitle(playerId)};

  if (SAVEFILE_isUsingModernTheme())
    offsetX = -1;

  for (u32 i = 0; i < COMBO_DIGITS; i++) {
    auto digit = std::unique_ptr<Digit>{new Digit(
        DigitSize::BIG,
        (isDouble() ? GAME_POSITION_X[1] : GameState.positionX[playerId]) +
            DIGITS_POSITION_X + offsetX + (i > 0 ? OFFSET_3_DIGITS : 0),
        GameState.scorePositionY + DIGITS_POSITION_Y, i, playerId > 0)};
    digits.push_back(std::move(digit));
  }
}

void Combo::setValue(int value) {
  u32 oldValue = this->value;

  bool isRed = value < 0;
  u32 absValue = min(abs(value), MAX_COMBO);

  this->value = absValue;

  for (int i = COMBO_DIGITS - 1; i >= 0; i--) {
    digits[i]->set(absValue % 10, isRed);
    absValue /= 10;
  }

  bool had4Digits = oldValue > 999;
  bool has4Digits = this->value > 999;
  if (has4Digits != had4Digits)
    relocate();
}

void Combo::show() {
  title->show();

  for (auto& it : digits) {
    if (it->shouldBeVisible())
      it->show();
  }
}

void Combo::hide() {
  SPRITE_hide(title->get());

  for (auto& it : digits)
    SPRITE_hide(it->get());
}

void Combo::relocate() {
  title->relocate();

  bool has4Digits = digits[0]->shouldBeVisible();
  for (u32 i = 0; i < COMBO_DIGITS; i++)
    digits[i]->relocate(
        (isDouble() ? GAME_POSITION_X[1] : GameState.positionX[playerId]) +
            DIGITS_POSITION_X + offsetX +
            (i > 0 && !has4Digits ? OFFSET_3_DIGITS : 0) +
            (has4Digits ? OFFSET_4_DIGITS : 0),
        GameState.scorePositionY + DIGITS_POSITION_Y,
        has4Digits ? NUMBER_WIDTH_4_DIGITS : NUMBER_WIDTH_3_DIGITS);
}

void Combo::disableMosaic() {
  for (u32 i = 0; i < COMBO_DIGITS; i++)
    digits[i]->get()->oam.attr0 =
        digits[i]->get()->oam.attr0 & 0b1110111111111111;
}

void Combo::tick() {
  title->tick();

  for (auto& it : digits) {
    if (it->shouldBeVisible())
      it->tick();
  }
}

Combo::~Combo() {
  digits.clear();
}
