#include "Total.h"

#include <libgba-sprite-engine/gba/tonc_bios.h>

#include "../../gameplay/save/SaveFile.h"
#include "utils/SpriteUtils.h"

const u32 MAX_TOTAL = 9999;
const u32 DIGITS = 4;
const u32 NUMBER_WIDTH_3_DIGITS = 19;
const u32 NUMBER_WIDTH_4_DIGITS = 13;
const int OFFSET_3_DIGITS = -NUMBER_WIDTH_3_DIGITS;
const int OFFSET_4_DIGITS = -1;

Total::Total(u32 x, u32 y, bool has4Digits, bool isFirst) {
  u32 numberWidth4Digits = NUMBER_WIDTH_4_DIGITS;
  u32 offset4Digits = OFFSET_4_DIGITS;
  if (!SAVEFILE_isUsingModernTheme()) {
    numberWidth4Digits++;
    offset4Digits--;
  }

  for (u32 i = 0; i < DIGITS; i++) {
    auto digit = std::unique_ptr<Digit>{new Digit(
        has4Digits ? DigitSize::MINI_NARROW : DigitSize::MINI, x, y, i, false)};

    digit->relocate(x + (i > 0 && !has4Digits ? OFFSET_3_DIGITS : 0) +
                        (has4Digits ? offset4Digits : 0),
                    y, has4Digits ? numberWidth4Digits : NUMBER_WIDTH_3_DIGITS);

    if (i > 0 || has4Digits)
      digit->showAt(0);

    if (i == 0 && !isFirst)
      SPRITE_reuseTiles(digit->get());

    digits.push_back(std::move(digit));
  }
}

void Total::setValue(u32 value) {
  if (value > MAX_TOTAL)
    value = MAX_TOTAL;

  for (int i = DIGITS - 1; i >= 0; i--) {
    digits[i]->set(value % 10, false);
    value /= 10;
  }
}

void Total::render(std::vector<Sprite*>* sprites) {
  for (auto& it : digits)
    sprites->push_back(it->get());
}

Total::~Total() {
  digits.clear();
}
