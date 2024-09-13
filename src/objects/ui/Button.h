#ifndef BUTTON_H
#define BUTTON_H

#include <libgba-sprite-engine/sprites/sprite.h>

enum ButtonType {
  BLUE,
  GRAY,
  ORANGE,
  LEVEL_METER,
  SUB_BUTTON_BLUE,
  SUB_BUTTON_GRAY,
  SUB_BUTTON_ORANGE
};

class Button {
 public:
  Button(ButtonType, u32 x, u32 y, bool reuseTiles);

  void setSelected(bool isSelected);
  void show();
  void hide();

  inline Sprite* get() { return sprite.get(); }

 private:
  std::unique_ptr<Sprite> sprite;
  ButtonType type;
  u32 x;
  u32 y;
  bool isSelected = false;
};

#endif  // BUTTON_H
