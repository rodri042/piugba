#ifndef STAGE_BREAK_SCENE_H
#define STAGE_BREAK_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include "objects/ui/Instructor.h"
#include "utils/PixelBlink.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

class StageBreakScene : public Scene {
 public:
  StageBreakScene(std::shared_ptr<GBAEngine> engine, const GBFS_FILE* fs);

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;
  void render() override;

 private:
  bool hasStarted = false;
  std::unique_ptr<Background> bg;
  std::unique_ptr<PixelBlink> pixelBlink;
  const GBFS_FILE* fs;

  std::unique_ptr<Instructor> instructor;
  u32 step = 0;

  void setUpSpritesPalette();
  void setUpBackground();

  void updateStats();

  void animate();
  void finish();
  void processMultiplayerUpdates();
};

#endif  // STAGE_BREAK_SCENE_H
