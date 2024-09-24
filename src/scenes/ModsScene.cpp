#include "ModsScene.h"

#include "assets.h"
#include "gameplay/multiplayer/Syncer.h"
#include "gameplay/save/SaveFile.h"
#include "utils/SceneUtils.h"

#define TITLE "MODS"
#define OPTION_COUNT 13
#define OPTION_MULTIPLIER 0
#define OPTION_STAGE_BREAK 1
#define OPTION_PIXELATE 2
#define OPTION_JUMP 3
#define OPTION_REDUCE 4
#define OPTION_BOUNCE 5
#define OPTION_COLOR_FILTER 6
#define OPTION_SPEED_HACK 7
#define OPTION_MIRROR_STEPS 8
#define OPTION_RANDOM_STEPS 9
#define OPTION_AUTOMOD 10
#define OPTION_TRAINING_MODE 11
#define OPTION_RESET 12

const u32 TEXT_BLEND_ALPHA = 12;

const char* COLOR_FILTERS[] = {
    "OFF",      "VIBRANT", "CONTRAST",  "POSTERIZE", "WARM",  "COLD",
    "ETHEREAL", "WATER",   "GOLDEN",    "DREAMY",    "RETRO", "ALIEN",
    "SPACE",    "SEPIA",   "GRAYSCALE", "MONO",      "INVERT"};

ModsScene::ModsScene(std::shared_ptr<GBAEngine> engine, const GBFS_FILE* fs)
    : MenuScene(engine, fs) {}

u32 ModsScene::getOptionCount() {
  return OPTION_COUNT;
}

void ModsScene::loadBackground(u32 id) {
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_WALL_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_WALL_TILES, BG_WALL_MAP, id);

  EFFECT_setUpBlend(BLD_BG0, BLD_BG1);
  EFFECT_setBlendAlpha(TEXT_BLEND_ALPHA);
}

void ModsScene::printOptions() {
  TextStream::instance().scrollNow(0, 2);
  SCENE_write(TITLE, 1);

  u8 multiplier = SAVEFILE_read8(SRAM->mods.multiplier);
  u8 stageBreak = SAVEFILE_read8(SRAM->mods.stageBreak);
  u8 pixelate = SAVEFILE_read8(SRAM->mods.pixelate);
  u8 jump = SAVEFILE_read8(SRAM->mods.jump);
  u8 reduce = SAVEFILE_read8(SRAM->mods.reduce);
  u8 bounce = SAVEFILE_read8(SRAM->mods.bounce);
  u8 colorFilter = SAVEFILE_read8(SRAM->mods.colorFilter);
  u8 speedHack = SAVEFILE_read8(SRAM->mods.speedHack);
  u8 mirrorSteps = SAVEFILE_read8(SRAM->mods.mirrorSteps);
  u8 randomSteps = SAVEFILE_read8(SRAM->mods.randomSteps);
  u8 autoMod = SAVEFILE_read8(SRAM->mods.autoMod);
  u8 trainingMode = SAVEFILE_read8(SRAM->mods.trainingMode);

  if (speedHack == 3)
    printOption(OPTION_MULTIPLIER, "Multiplier", "---", 3, true, "---");
  else if (speedHack == 2)
    printOption(OPTION_MULTIPLIER, "FixedVelocity",
                "FV" + std::to_string(AUTOVELOCITY_VALUES[multiplier - 1]), 3,
                true, "AV700");
  else if (speedHack == 1)
    printOption(OPTION_MULTIPLIER, "AutoVelocity",
                "AV" + std::to_string(AUTOVELOCITY_VALUES[multiplier - 1]), 3,
                true, "AV700");
  else
    printOption(OPTION_MULTIPLIER, "Multiplier",
                std::to_string(multiplier) + "x", 3, true, "3x");
  printOption(OPTION_STAGE_BREAK, "Stage break",
              stageBreak == 1   ? "OFF"
              : stageBreak == 2 ? "DEATH"
                                : "ON",
              4, true, "ON");
  if (autoMod) {
    printOption(OPTION_PIXELATE, "Pixelate", "---", 5, true, "---");
  } else {
    printOption(OPTION_PIXELATE, "Pixelate",
                pixelate == 1   ? "LIFE"
                : pixelate == 2 ? "FIXED"
                : pixelate == 3 ? "BLINK IN"
                : pixelate == 4 ? "BLINK OUT"
                : pixelate == 5 ? "RANDOM"
                                : "OFF",
                5, true, "OFF");
  }
  if (autoMod || isSinglePlayerDouble()) {
    printOption(OPTION_JUMP, "Jump", "---", 6, true, "---");
  } else {
    printOption(OPTION_JUMP, "Jump",
                jump == 1   ? "LINEAR"
                : jump == 2 ? "RANDOM"
                            : "OFF",
                6, true, "OFF");
  }
  if (autoMod) {
    printOption(OPTION_REDUCE, "Reduce", "---", 7, true, "---");
  } else {
    printOption(OPTION_REDUCE, "Reduce",
                reduce == 1   ? "FIXED"
                : reduce == 2 ? "LINEAR"
                : reduce == 3 ? "MICRO"
                : reduce == 4 ? "RANDOM"
                              : "OFF",
                7, true, "OFF");
  }
  if (autoMod) {
    printOption(OPTION_BOUNCE, "Bounce", "---", 8, true, "---");
  } else {
    printOption(OPTION_BOUNCE, "Bounce",
                bounce == 0   ? "OFF"
                : bounce == 2 ? "ALL"
                              : "ARROWS",
                8, true, "OFF");
  }
  if (autoMod) {
    printOption(OPTION_COLOR_FILTER, "Color filter", "---", 9, true, "---");
  } else {
    printOption(
        OPTION_COLOR_FILTER, "Color filter",
        colorFilter < TOTAL_COLOR_FILTERS ? COLOR_FILTERS[colorFilter] : "OFF",
        9, true, "OFF");
  }
  printOption(OPTION_SPEED_HACK, "Speed hack",
              speedHack == 1   ? "AV"
              : speedHack == 2 ? "FV"
              : speedHack == 3 ? "RANDOM"
                               : "OFF",
              10, true, "OFF");
  printOption(OPTION_MIRROR_STEPS, "Mirror steps", mirrorSteps ? "ON" : "OFF",
              11, true, "OFF");
  if (isSinglePlayerDouble())
    printOption(OPTION_RANDOM_STEPS, "Random steps", "---", 12, true, "---");
  else
    printOption(OPTION_RANDOM_STEPS, "Random steps", randomSteps ? "ON" : "OFF",
                12, true, "OFF");
  printOption(OPTION_AUTOMOD, "AutoMod",
              autoMod == 0   ? "OFF"
              : autoMod == 2 ? "INSANE"
                             : "FUN",
              13, true, "OFF");
  printOption(OPTION_TRAINING_MODE, "Training mode",
              trainingMode == 0   ? "OFF"
              : trainingMode == 1 ? "ON"
                                  : "SILENT",
              14, true, "OFF");

  if (stageBreak == 1 || speedHack == 2 || randomSteps || trainingMode > 0)
    SCENE_write("! Grade saving OFF !", 15);

  printOption(OPTION_RESET, "        [RESET ALL]", "", 16);
}

bool ModsScene::selectOption(u32 selected, int direction) {
  bool autoMod = SAVEFILE_read8(SRAM->mods.autoMod);
  u8 speedHack = SAVEFILE_read8(SRAM->mods.speedHack);

  switch (selected) {
    case OPTION_MULTIPLIER: {
      if (speedHack == 3)
        return true;

      u8 multiplier = SAVEFILE_read8(SRAM->mods.multiplier);
      multiplier = 1 + change(multiplier - 1, ARROW_MAX_MULTIPLIER, direction);

      SAVEFILE_write8(SRAM->mods.multiplier, multiplier);
      return true;
    }
    case OPTION_STAGE_BREAK: {
      u8 stageBreak = SAVEFILE_read8(SRAM->mods.stageBreak);
      SAVEFILE_write8(SRAM->mods.stageBreak, change(stageBreak, 3, direction));
      return true;
    }
    case OPTION_PIXELATE: {
      if (autoMod)
        return true;

      u8 pixelate = SAVEFILE_read8(SRAM->mods.pixelate);
      SAVEFILE_write8(SRAM->mods.pixelate, change(pixelate, 6, direction));
      return true;
    }
    case OPTION_JUMP: {
      if (autoMod || isSinglePlayerDouble())
        return true;

      u8 jump = SAVEFILE_read8(SRAM->mods.jump);
      SAVEFILE_write8(SRAM->mods.jump, change(jump, 3, direction));
      return true;
    }
    case OPTION_BOUNCE: {
      if (autoMod)
        return true;

      u8 bounce = SAVEFILE_read8(SRAM->mods.bounce);
      SAVEFILE_write8(SRAM->mods.bounce, change(bounce, 3, direction));
      return true;
    }
    case OPTION_REDUCE: {
      if (autoMod)
        return true;

      u8 reduce = SAVEFILE_read8(SRAM->mods.reduce);
      SAVEFILE_write8(SRAM->mods.reduce, change(reduce, 5, direction));
      return true;
    }
    case OPTION_COLOR_FILTER: {
      if (autoMod)
        return true;

      u8 colorFilter = SAVEFILE_read8(SRAM->mods.colorFilter);
      SAVEFILE_write8(SRAM->mods.colorFilter,
                      change(colorFilter, TOTAL_COLOR_FILTERS, direction));
      return true;
    }
    case OPTION_SPEED_HACK: {
      u8 speedHack = SAVEFILE_read8(SRAM->mods.speedHack);
      SAVEFILE_write8(SRAM->mods.speedHack, change(speedHack, 4, direction));
      return true;
    }
    case OPTION_MIRROR_STEPS: {
      bool mirrorSteps = SAVEFILE_read8(SRAM->mods.mirrorSteps);
      SAVEFILE_write8(SRAM->mods.mirrorSteps, !mirrorSteps);
      return true;
    }
    case OPTION_RANDOM_STEPS: {
      if (isSinglePlayerDouble())
        return true;

      bool randomSteps = SAVEFILE_read8(SRAM->mods.randomSteps);
      SAVEFILE_write8(SRAM->mods.randomSteps, !randomSteps);
      return true;
    }
    case OPTION_AUTOMOD: {
      u8 autoMod = SAVEFILE_read8(SRAM->mods.autoMod);
      SAVEFILE_write8(SRAM->mods.autoMod, change(autoMod, 3, direction));
      return true;
    }
    case OPTION_TRAINING_MODE: {
      u8 trainingMode = SAVEFILE_read8(SRAM->mods.trainingMode);
      SAVEFILE_write8(SRAM->mods.trainingMode,
                      change(trainingMode, 3, direction));
      return true;
    }
    case OPTION_RESET: {
      if (direction != 0)
        return true;

      SAVEFILE_resetMods();
      return true;
    }
  }

  return true;
}
