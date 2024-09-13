#include "State.h"

#include "Savefile.h"
#include "gameplay/multiplayer/Syncer.h"

DATA_EWRAM RAMState GameState;

void STATE_setup(Song* song, Chart* chart) {
  auto gameMode = SAVEFILE_getGameMode();
  if (song == NULL)
    gameMode = GameMode::ARCADE;

  GameState.mode = gameMode;
  GameState.isShuffleMode =
      gameMode == DEATH_MIX &&
      (ENV_ARCADE || SAVEFILE_read8(SRAM->isShuffleMode) == 1);
  GameState.settings.audioLag = SAVEFILE_read32(SRAM->settings.audioLag);
  GameState.settings.gamePosition =
      static_cast<GamePosition>(SAVEFILE_read8(SRAM->settings.gamePosition));
  GameState.settings.backgroundType =
      isMultiplayer() || isSinglePlayerDouble()
          ? BackgroundType::FULL_BGA_DARK
          : static_cast<BackgroundType>(
                SAVEFILE_read8(SRAM->settings.backgroundType));
  GameState.settings.bgaDarkBlink = SAVEFILE_read8(SRAM->settings.bgaDarkBlink);
  GameState.adminSettings.rumble =
      static_cast<RumbleOpts>(SAVEFILE_read8(SRAM->adminSettings.rumble));
  GameState.adminSettings.ioBlink =
      static_cast<IOBlinkOpts>(SAVEFILE_read8(SRAM->adminSettings.ioBlink));
  GameState.adminSettings.sramBlink =
      static_cast<SRAMBlinkOpts>(SAVEFILE_read8(SRAM->adminSettings.sramBlink));

  GameState.mods.multiplier =
      isMultiplayer() ? 3 : SAVEFILE_read8(SRAM->mods.multiplier);

  switch (gameMode) {
    case GameMode::CAMPAIGN: {
      GameState.mods.stageBreak =
          !ENV_DEVELOPMENT || ((~REG_KEYS & KEY_ANY) & KEY_SELECT)
              ? StageBreakOpts::sON
              : StageBreakOpts::sOFF;
      GameState.mods.pixelate = PixelateOpts::pOFF;
      GameState.mods.jump = JumpOpts::jOFF;
      GameState.mods.reduce = ReduceOpts::rOFF;
      GameState.mods.bounce = BounceOpts::bOFF;
      GameState.mods.colorFilter = ColorFilter::NO_FILTER;
      GameState.mods.speedHack = SpeedHackOpts::hOFF;
      GameState.mods.mirrorSteps = false;
      GameState.mods.randomSteps = false;
      GameState.mods.autoMod = AutoModOpts::aOFF;
      GameState.mods.trainingMode = TrainingModeOpts::tOFF;

      if (song->applyTo[chart->difficulty]) {
        GameState.mods.pixelate = static_cast<PixelateOpts>(song->pixelate);
        GameState.mods.jump = static_cast<JumpOpts>(song->jump);
        GameState.mods.reduce = static_cast<ReduceOpts>(song->reduce);
        GameState.mods.bounce = static_cast<BounceOpts>(song->bounce);
        GameState.mods.colorFilter =
            static_cast<ColorFilter>(song->colorFilter);
        GameState.mods.speedHack = static_cast<SpeedHackOpts>(song->speedHack);
      }

      break;
    }
    case GameMode::ARCADE: {
      auto autoMod =
          static_cast<AutoModOpts>(SAVEFILE_read8(SRAM->mods.autoMod));

      GameState.mods.stageBreak =
          static_cast<StageBreakOpts>(SAVEFILE_read8(SRAM->mods.stageBreak));
      GameState.mods.pixelate =
          autoMod
              ? PixelateOpts::pOFF
              : static_cast<PixelateOpts>(SAVEFILE_read8(SRAM->mods.pixelate));
      GameState.mods.jump =
          autoMod || isSinglePlayerDouble()
              ? JumpOpts::jOFF
              : static_cast<JumpOpts>(SAVEFILE_read8(SRAM->mods.jump));
      GameState.mods.reduce =
          autoMod ? ReduceOpts::rOFF
                  : static_cast<ReduceOpts>(SAVEFILE_read8(SRAM->mods.reduce));
      GameState.mods.bounce =
          autoMod ? BounceOpts::bOFF
                  : static_cast<BounceOpts>(SAVEFILE_read8(SRAM->mods.bounce));
      GameState.mods.colorFilter =
          autoMod ? ColorFilter::NO_FILTER
                  : static_cast<ColorFilter>(
                        SAVEFILE_read8(SRAM->mods.colorFilter));
      GameState.mods.speedHack =
          static_cast<SpeedHackOpts>(SRAM->mods.speedHack);
      GameState.mods.mirrorSteps = SAVEFILE_read8(SRAM->mods.mirrorSteps);
      GameState.mods.randomSteps =
          !isSinglePlayerDouble() && SAVEFILE_read8(SRAM->mods.randomSteps);
      GameState.mods.autoMod = autoMod;
      GameState.mods.trainingMode = static_cast<TrainingModeOpts>(
          SAVEFILE_read8(SRAM->mods.trainingMode));
      break;
    }
    case GameMode::IMPOSSIBLE: {
      GameState.mods.stageBreak =
          !ENV_DEVELOPMENT || ((~REG_KEYS & KEY_ANY) & KEY_SELECT)
              ? StageBreakOpts::sON
              : StageBreakOpts::sOFF;
      GameState.mods.pixelate = PixelateOpts::pOFF;
      GameState.mods.jump = JumpOpts::jOFF;
      GameState.mods.reduce = ReduceOpts::rOFF;
      GameState.mods.bounce = BounceOpts::bOFF;
      GameState.mods.colorFilter = ColorFilter::NO_FILTER;
      GameState.mods.speedHack = SpeedHackOpts::hOFF;
      GameState.mods.mirrorSteps = true;
      GameState.mods.randomSteps = false;
      GameState.mods.autoMod = AutoModOpts::aINSANE;
      GameState.mods.trainingMode = TrainingModeOpts::tOFF;
      break;
    }
    default: {
      GameState.mods.stageBreak = StageBreakOpts::sON;
      GameState.mods.pixelate = PixelateOpts::pOFF;
      GameState.mods.jump = JumpOpts::jOFF;
      GameState.mods.reduce = ReduceOpts::rOFF;
      GameState.mods.bounce = BounceOpts::bOFF;
      GameState.mods.colorFilter = ColorFilter::NO_FILTER;
      GameState.mods.speedHack = SpeedHackOpts::hOFF;
      GameState.mods.mirrorSteps = false;
      GameState.mods.randomSteps = false;
      GameState.mods.autoMod = AutoModOpts::aOFF;
      GameState.mods.trainingMode = TrainingModeOpts::tOFF;
    }
  }

  if (gameMode == GameMode::DEATH_MIX) {
#ifdef SENV_DEVELOPMENT
    GameState.mods.stageBreak = ((~REG_KEYS & KEY_ANY) & KEY_SELECT)
                                    ? StageBreakOpts::sON
                                    : StageBreakOpts::sOFF;
#endif

    GameState.mods.speedHack = SpeedHackOpts::hFIXED_VELOCITY;
  }

  GameState.positionX[0] =
      isMultiplayer() ? (isVs() ? GAME_POSITION_X[0] : GAME_COOP_POSITION_X)
      : isSinglePlayerDouble()
          ? GAME_COOP_POSITION_X
          : GAME_POSITION_X[SAVEFILE_read8(SRAM->settings.gamePosition) % 3];
  GameState.positionX[1] = isVs() ? GAME_POSITION_X[2] : 0;
  GameState.positionY = 0;
  GameState.scorePositionY = 0;

  if (GameState.mods.reduce != ReduceOpts::rOFF) {
    GameState.positionY = GameState.mods.reduce == ReduceOpts::rLINEAR ||
                                  GameState.mods.reduce == ReduceOpts::rMICRO
                              ? 0
                              : REDUCE_MOD_POSITION_Y;
    if (GameState.mods.reduce != ReduceOpts::rMICRO)
      GameState.scorePositionY = REDUCE_MOD_SCORE_POSITION_Y;
  }
}
