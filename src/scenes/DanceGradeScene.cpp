#include "DanceGradeScene.h"

#include <libgba-sprite-engine/effects/fade_out_scene.h>

#include "data/content/_compiled_sprites/palette_grade.h"
#include "gameplay/models/Song.h"
#include "scenes/SelectionScene.h"
#include "utils/BackgroundUtils.h"

DanceGradeScene::DanceGradeScene(std::shared_ptr<GBAEngine> engine)
    : Scene(engine) {}

std::vector<Background*> DanceGradeScene::backgrounds() {
  return {};
}

std::vector<Sprite*> DanceGradeScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(grade->get());

  return sprites;
}

void DanceGradeScene::load() {
  BACKGROUND_enable(false, false, false, false);
  grade = std::unique_ptr<Grade>{new Grade(GradeType::S, 50, 50)};

  setUpSpritesPalette();
}

void DanceGradeScene::tick(u16 keys) {
  if (keys & KEY_ANY && !engine->isTransitioning()) {
    engine->transitionIntoScene(new SelectionScene(engine),
                                new FadeOutScene(2));
  }
}

void DanceGradeScene::setUpSpritesPalette() {
  foregroundPalette = std::unique_ptr<ForegroundPaletteManager>(
      new ForegroundPaletteManager(palette_gradePal, sizeof(palette_gradePal)));
}
