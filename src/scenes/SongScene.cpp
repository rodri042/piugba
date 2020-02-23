#include "SongScene.h"
#include <libgba-sprite-engine/background/text_stream.h>
#include <libgba-sprite-engine/palette/palette_manager.h>
#include <libgba-sprite-engine/sprites/sprite_builder.h>
#include "data/lama.h"

std::vector<Background*> SongScene::backgrounds() {
  return {};
}

std::vector<Sprite*> SongScene::sprites() {
  return {animation.get()};
}

int last_beat = 0;
int count = -3;
int velocity = 10;
bool to_left = false;

void SongScene::load() {
  foregroundPalette = std::unique_ptr<ForegroundPaletteManager>(
      new ForegroundPaletteManager(sharedPal, sizeof(sharedPal)));
  backgroundPalette =
      std::unique_ptr<BackgroundPaletteManager>(new BackgroundPaletteManager());

  SpriteBuilder<Sprite> builder;

  animation = builder.withData(lamaTiles, sizeof(lamaTiles))
                  .withSize(SIZE_32_32)
                  .withAnimated(6, 2)
                  .withLocation(70, 50)
                  .buildPtr();

  TextStream::instance().setText("AHI TA VITEH!", 3, 8);

  engine->getTimer()->start();
}

void SongScene::tick(u16 keys) {
  engine->getTimer()->onvblank();  // TODO: MOVE

  // 60000-----157beats
  // totalmsecs-----x = totalmsecs*157/60000
  int beat = (engine->getTimer()->getTotalMsecs() * 157) / 60000;  // 157 bpm
  if (beat != last_beat) {
    int delta = sgn(velocity);
    count += delta;
    animation->moveTo(animation->getX() + velocity, animation->getY());
    if (abs(count) >= 4) {
      velocity = -velocity;
      to_left = !to_left;
      animation->flipHorizontally(to_left);
    }
  }
  TextStream::instance().setText(std::to_string(count), 10, 1);
  last_beat = beat;

  int is_odd = beat & 1;
  TextStream::instance().setText("             ", !is_odd ? 18 : 15, 1);
  TextStream::instance().setText(engine->getTimer()->to_string(), is_odd ? 18 : 15, 1);

  // if (keys & KEY_LEFT) {
  //   animation->flipHorizontally(true);
  // } else if (keys & KEY_RIGHT) {
  //   animation->flipHorizontally(false);
  // } else if (keys & KEY_UP) {
  //   animation->flipVertically(true);
  // } else if (keys & KEY_DOWN) {
  //   animation->flipVertically(false);
  // }
}
