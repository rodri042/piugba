#include "AdminScene.h"

#include "assets.h"
#include "gameplay/Sequence.h"
#include "gameplay/multiplayer/PS2Keyboard.h"
#include "gameplay/save/SaveFile.h"
#include "player/PlaybackState.h"
#include "scenes/StartScene.h"
#include "utils/SceneUtils.h"

#define TITLE "ADMIN MENU (v1.11.3)"
#define SUBMENU_RUMBLE 0
#define SUBMENU_OFFSETS 1
#define SUBMENU_RESET 2
#define SUBMENU_SURE_OFFSETS 3
#define SUBMENU_SURE_ARCADE 4
#define SUBMENU_SURE_ALL 5
#define OPTION_COUNT_DEFAULT 10
#define OPTION_COUNT_RUMBLE 5
#define OPTION_COUNT_OFFSETS 4
#define OPTION_COUNT_RESET 3
#define OPTION_COUNT_ARE_YOU_SURE 2

#define OPTION_NAVIGATION_STYLE 0
#define OPTION_RUMBLE 1
#define OPTION_IO_BLINK 2
#define OPTION_SRAM_BLINK 3
#define OPTION_HQ_MODE 4
#define OPTION_EWRAM_OVERCLOCK 5
#define OPTION_PS2_INPUT 6
#define OPTION_RUMBLE_OPTS 7
#define OPTION_CUSTOM_OFFSETS 8
#define OPTION_RESET_SAVE_FILE 9

AdminScene::AdminScene(std::shared_ptr<GBAEngine> engine,
                       const GBFS_FILE* fs,
                       bool withSound)
    : MenuScene(engine, fs) {
  this->withSound = withSound;
}

u32 AdminScene::getOptionCount() {
  return submenu >= SUBMENU_SURE_OFFSETS ? OPTION_COUNT_ARE_YOU_SURE
         : submenu == SUBMENU_RESET      ? OPTION_COUNT_RESET
         : submenu == SUBMENU_RUMBLE     ? OPTION_COUNT_RUMBLE
         : submenu == SUBMENU_OFFSETS    ? OPTION_COUNT_OFFSETS
                                         : OPTION_COUNT_DEFAULT;
}

void AdminScene::loadBackground(u32 id) {
  totalOffsets = OFFSET_getCount();
  VBlankIntrWait();
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_LINES_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_LINES_TILES, BG_LINES_MAP, id);
}

void AdminScene::printOptions() {
#define PLAY_AND_END()          \
  if (withSound) {              \
    player_playSfx(SOUND_STEP); \
    withSound = false;          \
  }                             \
  return;

  TextStream::instance().scrollNow(0, -2);

  if (submenu == SUBMENU_RUMBLE) {
    SCENE_write("RUMBLE OPTIONS", 1);

    u8 rumbleFrames = SAVEFILE_read8(SRAM->adminSettings.rumbleFrames);
    u8 rumbleOpts = SAVEFILE_read8(SRAM->adminSettings.rumbleOpts);
    u8 rumblePreRollFrames = RUMBLE_PREROLL(rumbleOpts);
    u8 rumbleIdleCyclePeriod = RUMBLE_IDLE(rumbleOpts);

    printOption(0, "Total frames", std::to_string(rumbleFrames), 4);
    printOption(1, "Pre-roll frames", std::to_string(rumblePreRollFrames), 6);
    printOption(2, "Idle cycle period", std::to_string(rumbleIdleCyclePeriod),
                8);

    printOption(3, "[RESET RUMBLE]", "", 13);
    printOption(4, "[BACK]", "", 15);

    PLAY_AND_END();
  } else if (submenu == SUBMENU_OFFSETS) {
    SCENE_write("CUSTOM OFFSETS", 1);

    int globalOffset = (int)SAVEFILE_read32(SRAM->globalOffset);

    bool offsetEditingEnabled =
        SAVEFILE_read8(SRAM->adminSettings.offsetEditingEnabled);
    printOption(0, "Offset editing", offsetEditingEnabled ? "ON" : "OFF", 4);
    printOption(1, "Global offset",
                (globalOffset > 0 ? "+" : "") + std::to_string(globalOffset),
                5);

    SCENE_write("When editing is enabled,", 7);
    SCENE_write("press SELECT + L/R", 8);
    SCENE_write("to add -/+ 8ms offsets.", 9);
    SCENE_write("Total offsets: " + std::to_string(totalOffsets), 11);

    printOption(2, "[RESET ALL OFFSETS]", "", 13);
    printOption(3, "[BACK]", "", 15);

    PLAY_AND_END();
  } else if (submenu == SUBMENU_RESET) {
    SCENE_write("DELETE SAVE FILE", 1);

    printOption(0, "[RESET ARCADE PROGRESS]", "", 11);
    printOption(1, "[DELETE ALL SAVED DATA]", "", 13);
    printOption(2, "[BACK]", "", 15);

    PLAY_AND_END();
  } else if (submenu >= SUBMENU_SURE_OFFSETS) {
    SCENE_write("ARE YOU SURE?!", 1);

    printOption(0, "[NO]", "", 13);
    printOption(1, "[YES]", "", 15);

    PLAY_AND_END();
  }

  SCENE_write(TITLE, 1);
  SCENE_write("[SUPERCARD SD]", 2);

  u8 navigationStyle = SAVEFILE_read8(SRAM->adminSettings.navigationStyle);
  u8 rumble = SAVEFILE_read8(SRAM->adminSettings.rumble);
  u8 ioBlink = SAVEFILE_read8(SRAM->adminSettings.ioBlink);
  u8 sramBlink = SAVEFILE_read8(SRAM->adminSettings.sramBlink);
  u8 hqMode = SAVEFILE_read8(SRAM->adminSettings.hqMode);
  u8 ewramOverclock = SAVEFILE_read8(SRAM->adminSettings.ewramOverclock);
  u8 ps2Input = SAVEFILE_read8(SRAM->adminSettings.ps2Input);

  printOption(OPTION_NAVIGATION_STYLE, "Navigation style",
              navigationStyle != 1 ? "PIU" : "GBA", 4);

  printOption(OPTION_RUMBLE, "Rumble",
              rumble == 0   ? "OFF"
              : rumble == 2 ? "I/O SC"
                            : "CARTRIDGE",
              6);
  printOption(OPTION_IO_BLINK, "I/O SD blink",
              ioBlink == 1   ? "ON BEAT"
              : ioBlink == 2 ? "ON KEY"
                             : "OFF",
              7);
  printOption(OPTION_SRAM_BLINK, "SRAM LED blink",
              sramBlink == 1   ? "ON BEAT"
              : sramBlink == 2 ? "ON HIT"
                               : "OFF",
              8);
  printOption(OPTION_HQ_MODE, "HQ (audio / video)",
              ps2Input > 0 ? "---"
                           : (hqMode == 3   ? "VIDEO"
                              : hqMode == 4 ? "AUDIO"
                              : hqMode > 0  ? "ALL"
                                            : "OFF"),
              9);
  printOption(OPTION_EWRAM_OVERCLOCK, "EWRAM overclock",
              ewramOverclock == 1 ? "ON" : "OFF", 10);
  printOption(OPTION_PS2_INPUT, "PS/2 input",
              hqMode > 0 ? "---" : (ps2Input > 0 ? "ON" : "OFF"), 11);

  printOption(OPTION_RUMBLE_OPTS, "[RUMBLE OPTIONS]", "", 13);
  printOption(OPTION_CUSTOM_OFFSETS, "[CUSTOM OFFSETS]", "", 14);
  printOption(OPTION_RESET_SAVE_FILE, "[DELETE SAVE FILE]", "", 15);

  PLAY_AND_END();
}

bool AdminScene::selectOption(u32 selected, int direction) {
  if (submenu == SUBMENU_RUMBLE) {
    u8 rumbleFrames = SAVEFILE_read8(SRAM->adminSettings.rumbleFrames);
    u8 rumbleOpts = SAVEFILE_read8(SRAM->adminSettings.rumbleOpts);
    u8 rumblePreRollFrames = RUMBLE_PREROLL(rumbleOpts);
    u8 rumbleIdleCyclePeriod = RUMBLE_IDLE(rumbleOpts);

    switch (selected) {
      case 0: {
        SAVEFILE_write8(SRAM->adminSettings.rumbleFrames,
                        1 + change(rumbleFrames - 1, 12, direction));
        return true;
      }
      case 1: {
        SAVEFILE_write8(SRAM->adminSettings.rumbleOpts,
                        RUMBLE_OPTS_BUILD(
                            1 + change(rumblePreRollFrames - 1, 12, direction),
                            rumbleIdleCyclePeriod));
        return true;
      }
      case 2: {
        SAVEFILE_write8(
            SRAM->adminSettings.rumbleOpts,
            RUMBLE_OPTS_BUILD(rumblePreRollFrames,
                              change(rumbleIdleCyclePeriod, 7, direction)));
        return true;
      }
      case 3: {
        if (direction != 0)
          return true;

        SAVEFILE_resetRumble();
        return true;
      }
      case 4: {
        if (direction != 0)
          return true;
        submenu = -1;
        this->selected = 0;
        return true;
      }
    }

    return true;
  }

  if (submenu == SUBMENU_OFFSETS) {
    switch (selected) {
      case 0: {
        u8 value = SAVEFILE_read8(SRAM->adminSettings.offsetEditingEnabled);
        SAVEFILE_write8(SRAM->adminSettings.offsetEditingEnabled,
                        change(value, 2, direction));
        return true;
      }
      case 1: {
        int value = (int)SAVEFILE_read32(SRAM->globalOffset) +
                    8 * (direction >= 0 ? 1 : -1);
        if (value > 3000)
          value = 3000;
        else if (value < -3000)
          value = -3000;

        SAVEFILE_write32(SRAM->globalOffset, value);
        return true;
      }
      case 2: {
        if (direction != 0)
          return true;
        submenu = SUBMENU_SURE_OFFSETS;
        this->selected = 0;
        return true;
      }
      case 3: {
        if (direction != 0)
          return true;
        submenu = -1;
        this->selected = 0;
        return true;
      }
    }

    return true;
  }

  if (submenu == SUBMENU_RESET) {
    switch (selected) {
      case 0: {
        if (direction != 0)
          return true;
        submenu = SUBMENU_SURE_ARCADE;
        this->selected = 0;
        return true;
      }
      case 1: {
        if (direction != 0)
          return true;
        submenu = SUBMENU_SURE_ALL;
        this->selected = 0;
        return true;
      }
      case 2: {
        if (direction != 0)
          return true;
        submenu = -1;
        this->selected = 0;
        return true;
      }
    }

    return true;
  }

  if (submenu >= SUBMENU_SURE_OFFSETS) {
    if (direction != 0)
      return true;

    if (selected) {
      switch (submenu) {
        case SUBMENU_SURE_OFFSETS: {
          SAVEFILE_resetOffsets();
          SCENE_softReset();
          return true;
        }
        case SUBMENU_SURE_ARCADE: {
          SAVEFILE_resetArcade();
          SCENE_softReset();
          return true;
        }
        case SUBMENU_SURE_ALL: {
          SAVEFILE_reset();
          SCENE_softReset();
          return true;
        }
      }
    } else {
      submenu = -1;
      this->selected = 0;
    }

    return true;
  }

  switch (selected) {
    case OPTION_NAVIGATION_STYLE: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.navigationStyle);
      SAVEFILE_write8(SRAM->adminSettings.navigationStyle,
                      change(value, 2, direction));
      player_stop();
      engine->transitionIntoScene(new AdminScene(engine, fs, true),
                                  new PixelTransitionEffect());
      return false;
    }
    case OPTION_RUMBLE: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.rumble);
      SAVEFILE_write8(SRAM->adminSettings.rumble, change(value, 3, direction));
      return true;
    }
    case OPTION_IO_BLINK: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.ioBlink);
      SAVEFILE_write8(SRAM->adminSettings.ioBlink, change(value, 3, direction));
      return true;
    }
    case OPTION_SRAM_BLINK: {
      u8 value = SAVEFILE_read8(SRAM->adminSettings.sramBlink);
      SAVEFILE_write8(SRAM->adminSettings.sramBlink,
                      change(value, 3, direction));
      return true;
    }
    case OPTION_HQ_MODE: {
      u8 ps2Input = SAVEFILE_read8(SRAM->adminSettings.ps2Input);
      if (ps2Input > 0)
        return true;

      u8 hqMode = SAVEFILE_read8(SRAM->adminSettings.hqMode);
      u8 updatedHQMode = change(hqMode, 5, direction);
      bool wasOn = hqMode > 1;
      bool isOn = updatedHQMode > 1;

      if (wasOn && isOn) {
        SAVEFILE_write8(SRAM->adminSettings.hqMode, updatedHQMode);
        PlaybackState.isPCMDisabled =
            static_cast<HQModeOpts>(updatedHQMode) == HQModeOpts::dVIDEO_ONLY;
        return true;
      } else {
        player_stop();
        engine->transitionIntoScene(hqMode > 0 ? SEQUENCE_deactivateVideo()
                                               : SEQUENCE_activateVideo(true),
                                    new PixelTransitionEffect());

        return false;
      }
    }
    case OPTION_EWRAM_OVERCLOCK: {
      u8 ewramOverclock = SAVEFILE_read8(SRAM->adminSettings.ewramOverclock);

      if (ewramOverclock > 0) {
        player_stop();
        engine->transitionIntoScene(SEQUENCE_deactivateEWRAMOverclock(),
                                    new PixelTransitionEffect());

        return false;
      } else {
        player_stop();
        engine->transitionIntoScene(SEQUENCE_activateEWRAMOverclock(),
                                    new PixelTransitionEffect());

        return false;
      }
    }
    case OPTION_PS2_INPUT: {
      u8 hqMode = SAVEFILE_read8(SRAM->adminSettings.hqMode);
      if (hqMode > 0)
        return true;

      u8 value = SAVEFILE_read8(SRAM->adminSettings.ps2Input);
      u8 newValue = change(value, 2, direction);
      SAVEFILE_write8(SRAM->adminSettings.ps2Input, newValue);

      if (newValue)
        ps2Keyboard->activate();
      else
        ps2Keyboard->deactivate();

      return true;
    }
    case OPTION_RUMBLE_OPTS: {
      if (direction != 0)
        return true;

      submenu = SUBMENU_RUMBLE;
      this->selected = 0;
      return true;
    }
    case OPTION_CUSTOM_OFFSETS: {
      if (direction != 0)
        return true;

      submenu = SUBMENU_OFFSETS;
      this->selected = 0;
      return true;
    }
    case OPTION_RESET_SAVE_FILE: {
      if (direction != 0)
        return true;

      submenu = SUBMENU_RESET;
      this->selected = 0;
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
