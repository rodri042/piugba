#include "SettingsScene.h"

#include "AdminScene.h"
#include "CalibrateScene.h"
#include "DeathMixScene.h"
#include "SelectionScene.h"
#include "StartScene.h"
#include "assets.h"
#include "gameplay/Sequence.h"
#include "gameplay/multiplayer/Syncer.h"
#include "gameplay/save/SaveFile.h"
#include "utils/SceneUtils.h"

#define TITLE "SETTINGS"
#define OPTION_COUNT 7
#define OPTION_AUDIO_LAG 0
#define OPTION_THEME 1
#define OPTION_GAME_POSITION 2
#define OPTION_BACKGROUND_TYPE 3
#define OPTION_BGA_DARK_BLINK 4
#define OPTION_RESET 5
#define OPTION_QUIT 6

SettingsScene::SettingsScene(std::shared_ptr<GBAEngine> engine,
                             const GBFS_FILE* fs,
                             u32 initialOption)
    : MenuScene(engine, fs) {
  this->initialOption = initialOption;
  if (initialOption)
    selected = initialOption;
}

u32 SettingsScene::getOptionCount() {
  return OPTION_COUNT;
}

void SettingsScene::loadBackground(u32 id) {
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_LINES_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_LINES_TILES, BG_LINES_MAP, id);
}

void SettingsScene::printOptions() {
  SCENE_write(TITLE, 1);

  int audioLag = (int)SAVEFILE_read32(SRAM->settings.audioLag);
  u8 gamePosition = SAVEFILE_read8(SRAM->settings.gamePosition);
  u8 backgroundType = SAVEFILE_read8(SRAM->settings.backgroundType);
  u8 bgaDarkBlink = SAVEFILE_read8(SRAM->settings.bgaDarkBlink);
  u8 theme = SAVEFILE_read8(SRAM->settings.theme);

  printOption(OPTION_AUDIO_LAG, "Audio lag", std::to_string(audioLag), 3);
  printOption(OPTION_THEME, "Theme", theme == 1 ? "MODERN" : "CLASSIC", 5);

  if (isSinglePlayerDouble()) {
    printOption(OPTION_GAME_POSITION, "Game position", "---", 7);
    printOption(OPTION_BACKGROUND_TYPE, "Background type", "---", 9);
    printOption(OPTION_BGA_DARK_BLINK, "Background blink",
                bgaDarkBlink == 1 ? "ON" : "OFF", 11);
  } else {
    printOption(OPTION_GAME_POSITION, "Game position",
                gamePosition == 1   ? "CENTER"
                : gamePosition == 2 ? "RIGHT"
                                    : "LEFT",
                7);
    printOption(OPTION_BACKGROUND_TYPE, "Background type",
                backgroundType == 0   ? "RAW"
                : backgroundType == 1 ? "HALF DARK"
                                      : "FULL DARK",
                9);
    if (backgroundType > 0)
      printOption(OPTION_BGA_DARK_BLINK, "Background blink",
                  bgaDarkBlink == 1 ? "ON" : "OFF", 11);
    else
      printOption(OPTION_BGA_DARK_BLINK, "Background blink", "---", 11);
  }

  printOption(OPTION_RESET, "[RESET OPTIONS]", "", 14);
  printOption(
      OPTION_QUIT,
      quitToAdminMenu ? "[QUIT TO <ADMIN MENU>]" : "[QUIT TO <MAIN MENU>]", "",
      16);

  if (initialOption > 0) {
    player_playSfx(SOUND_STEP);
    initialOption = false;
  }
}

bool SettingsScene::selectOption(u32 selected, int direction) {
  switch (selected) {
    case OPTION_AUDIO_LAG: {
      if (direction != 0)
        return true;

      player_stop();
      engine->transitionIntoScene(new CalibrateScene(engine, fs),
                                  new PixelTransitionEffect());
      return false;
    }
    case OPTION_THEME: {
      u8 theme = SAVEFILE_read8(SRAM->settings.theme);
      SAVEFILE_write8(SRAM->settings.theme, change(theme, 2, direction));
      player_stop();
      engine->transitionIntoScene(new SettingsScene(engine, fs, OPTION_THEME),
                                  new PixelTransitionEffect());
      return false;
    }
    case OPTION_GAME_POSITION: {
      if (isSinglePlayerDouble())
        return true;

      u8 gamePosition = SAVEFILE_read8(SRAM->settings.gamePosition);
      SAVEFILE_write8(SRAM->settings.gamePosition,
                      change(gamePosition, 3, direction));
      return true;
    }
    case OPTION_BACKGROUND_TYPE: {
      if (isSinglePlayerDouble())
        return true;

      u8 backgroundType = SAVEFILE_read8(SRAM->settings.backgroundType);
      SAVEFILE_write8(SRAM->settings.backgroundType,
                      change(backgroundType, 3, direction));
      return true;
    }
    case OPTION_BGA_DARK_BLINK: {
      u8 backgroundType = SAVEFILE_read8(SRAM->settings.backgroundType);
      if (backgroundType == 0)
        return true;

      u8 bgaDarkBlink = SAVEFILE_read8(SRAM->settings.bgaDarkBlink);
      SAVEFILE_write8(SRAM->settings.bgaDarkBlink,
                      change(bgaDarkBlink, 2, direction));
      return true;
    }
    case OPTION_RESET: {
      if (direction != 0)
        return true;

      u8 theme = SAVEFILE_read8(SRAM->settings.theme);
      SAVEFILE_resetSettings();

      if (theme > 0) {
        player_stop();
        engine->transitionIntoScene(new SettingsScene(engine, fs, OPTION_RESET),
                                    new PixelTransitionEffect());
        return false;
      } else {
        return true;
      }
    }
    case OPTION_QUIT: {
      if (direction != 0) {
        quitToAdminMenu = !quitToAdminMenu;
        return true;
      }

      player_stop();
      if (quitToAdminMenu)
        SEQUENCE_goToAdminMenuHint();
      else
        engine->transitionIntoScene(new StartScene(engine, fs),
                                    new PixelTransitionEffect());
      return false;
    }
  }

  return true;
}

void SettingsScene::close() {
  player_stop();

  if (SAVEFILE_getGameMode() == GameMode::DEATH_MIX) {
    bool isShuffleMode =
        ENV_ARCADE || (SAVEFILE_read8(SRAM->isShuffleMode) == 1);
    engine->transitionIntoScene(
        new DeathMixScene(engine, fs, static_cast<MixMode>(isShuffleMode)),
        new PixelTransitionEffect());
  } else {
    engine->transitionIntoScene(new SelectionScene(engine, fs),
                                new PixelTransitionEffect());
  }
}
