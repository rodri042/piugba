#ifndef COMBO_H
#define COMBO_H

#include <libgba-sprite-engine/sprites/sprite.h>

#include <vector>

#include "ComboTitle.h"
#include "objects/Digit.h"

#define MAX_COMBO 9999
#define COMBO_DIGITS 4

class Combo {
 public:
  Combo(u8 playerId);

  void setValue(int value);
  inline u32 getValue() { return value; }
  void show();
  void hide();
  void relocate();
  void disableMosaic();

  void tick();

  inline ComboTitle* getTitle() { return title.get(); }
  inline std::vector<std::unique_ptr<Digit>>* getDigits() { return &digits; }

  ~Combo();

 private:
  u32 value = 0;
  std::unique_ptr<ComboTitle> title;
  std::vector<std::unique_ptr<Digit>> digits;
  u8 playerId;
  int offsetX = 0;
};

#endif  // COMBO_H
