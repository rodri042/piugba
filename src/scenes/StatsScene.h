#ifndef STATS_SCENE_H
#define STATS_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include <string>

#include "objects/base/InputHandler.h"
#include "objects/ui/ArrowSelector.h"
#include "utils/PixelBlink.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

class StatsScene : public Scene {
 public:
  StatsScene(std::shared_ptr<GBAEngine> engine, const GBFS_FILE* fs);

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;
  void render() override;

 private:
  struct ArcadePercentages {
    std::string singleProgress;
    std::string doubleProgress;
    u32 completedSingle;
    u32 totalSingle;
    u32 completedDouble;
    u32 totalDouble;
  };

  bool hasStarted = false;
  std::unique_ptr<PixelBlink> pixelBlink;
  std::unique_ptr<Background> bg;
  const GBFS_FILE* fs;
  std::unique_ptr<ArrowSelector> selectButton;
  bool didPrintText = false;

  void setUpSpritesPalette();
  void setUpBackground();

  void processKeys(u16 keys);
  void printStats();
  void loadBackground(u32 id);

  std::string getPlayTime();
  std::string getHighestLevel();
  std::string getCampaignProgress(GameMode gameMode);
  std::string getCampaignSClearProgress();
  ArcadePercentages getArcadeProgress();
  std::string getDeathMixProgress();
  std::string getPercentage(u32 current, u32 total);

  void printFixedLine(std::string name,
                      std::string value,
                      u32 row,
                      u32 extraSpace = 0);
};

#endif  // STATS_SCENE_H
