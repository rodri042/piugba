#ifndef INSTRUCTOR_H
#define INSTRUCTOR_H

#include <libgba-sprite-engine/sprites/sprite.h>

enum InstructorType { Girl, AngryGirl2, AngryGirl1 };

class Instructor {
 public:
  Instructor(InstructorType type, u32 x, u32 y);

  inline Sprite* get() { return sprite.get(); }

 private:
  std::unique_ptr<Sprite> sprite;
};

#endif  // INSTRUCTOR_H
