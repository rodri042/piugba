#include "AdminScene.h"

#include "assets.h"
#include "gameplay/save/SaveFile.h"
#include "scenes/StartScene.h"
#include "utils/SceneUtils.h"

#define TITLE "ADMIN MENU"
#define OPTIONS_COUNT 6
#define OPTION_ARCADE_CHARTS 0
#define OPTION_RUMBLE 1
#define OPTION_IO_BLINK 2
#define OPTION_SRAM_BLINK 3
#define OPTION_RESET_ARCADE_PROGRESS 4
#define OPTION_RESET_ALL_PROGRESS 5

AdminScene::AdminScene(std::shared_ptr<GBAEngine> engine, const GBFS_FILE* fs)
    : MenuScene(engine, fs) {}

u16 AdminScene::getCloseKey() {
  return KEY_START;
}

u32 AdminScene::getOptionsCount() {
  return OPTIONS_COUNT;
}

void AdminScene::loadBackground(u32 id) {
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_LINES_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_LINES_TILES, BG_LINES_MAP, id);
}

void AdminScene::printOptions() {
  SCENE_write(TITLE, 2);

  u8 charts = SAVEFILE_read8(SRAM->adminSettings.arcadeCharts);
  u8 rumble = SAVEFILE_read8(SRAM->adminSettings.rumble);
  u8 ioBlink = SAVEFILE_read8(SRAM->adminSettings.ioBlink);
  u8 sramBlink = SAVEFILE_read8(SRAM->adminSettings.sramBlink);

  printOption(OPTION_ARCADE_CHARTS, "Arcade charts",
              charts == 0 ? "SINGLE" : "DOUBLE", 5);
  printOption(OPTION_RUMBLE, "Rumble",
              rumble == 0 ? "OFF" : rumble == 1 ? "LOW" : "HIGH", 7);
  printOption(OPTION_IO_BLINK, "I/O SD Blink",
              ioBlink == 0 ? "OFF"
                           : ioBlink == 1 ? "ON BEAT"
                                          : ioBlink == 2 ? "ON HIT" : "ON KEY",
              9);
  printOption(
      OPTION_SRAM_BLINK, "SRAM LED Blink",
      sramBlink == 0
          ? "OFF"
          : sramBlink == 1 ? "ON BEAT" : sramBlink == 2 ? "ON HIT" : "ON KEY",
      11);
  printOption(OPTION_RESET_ARCADE_PROGRESS, "[RESET ARCADE PROGRESS]", "", 13);
  printOption(OPTION_RESET_ALL_PROGRESS, "[RESET ALL PROGRESS]", "", 15);
}

bool AdminScene::selectOption(u32 selected) {
  switch (selected) {
    case OPTION_ARCADE_CHARTS: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.arcadeCharts);
      SAVEFILE_write8(SRAM->adminSettings.arcadeCharts, increment(value, 2));
      return true;
    }
    case OPTION_RUMBLE: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.rumble);
      SAVEFILE_write8(SRAM->adminSettings.rumble, increment(value, 3));
      return true;
    }
    case OPTION_IO_BLINK: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.ioBlink);
      SAVEFILE_write8(SRAM->adminSettings.ioBlink, increment(value, 4));
      return true;
    }
    case OPTION_SRAM_BLINK: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.sramBlink);
      SAVEFILE_write8(SRAM->adminSettings.sramBlink, increment(value, 4));
      return true;
    }
    case OPTION_RESET_ARCADE_PROGRESS: {
      // TODO: IMPLEMENT
      return true;
    }
    case OPTION_RESET_ALL_PROGRESS: {
      // TODO: IMPLEMENT
      return true;
    }
  }

  return true;
}

void AdminScene::close() {
  player_stop();
  engine->transitionIntoScene(new StartScene(engine, fs),
                              new PixelTransitionEffect());
}
