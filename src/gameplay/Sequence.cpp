#include "Sequence.h"

#include <libgba-sprite-engine/gba/tonc_bios.h>

#include <functional>
#include <string>

#include "Key.h"
#include "SequenceMessages.h"
#include "gameplay/Library.h"
#include "gameplay/video/VideoStore.h"
#include "multiplayer/PS2Keyboard.h"
#include "multiplayer/Syncer.h"
#include "scenes/CalibrateScene.h"
#include "scenes/ControlsScene.h"
#include "scenes/DeathMixScene.h"
#include "scenes/MultiplayerLobbyScene.h"
#include "scenes/SelectionScene.h"
#include "scenes/SongScene.h"
#include "scenes/StartScene.h"
#include "scenes/StatsScene.h"
#include "scenes/TalkScene.h"
#include "utils/SceneUtils.h"

static std::shared_ptr<GBAEngine> _engine;
static const GBFS_FILE* _fs;

static void goTo(Scene* scene) {
  _engine->transitionIntoScene(scene, new PixelTransitionEffect());
}

void SEQUENCE_initialize(std::shared_ptr<GBAEngine> engine,
                         const GBFS_FILE* fs) {
  _engine = engine;
  _fs = fs;
}

Scene* SEQUENCE_getInitialScene() {
  u32 fixes = SAVEFILE_initialize(_fs);

  if (!SAVEFILE_isWorking(_fs))
    return SEQUENCE_halt(SRAM_TEST_FAILED);

  if (fixes > 0)
    return SEQUENCE_halt(SAVE_FILE_FIXED_1 + std::to_string(fixes) +
                         SAVE_FILE_FIXED_2);

  bool ewramOverclock = SAVEFILE_read8(SRAM->adminSettings.ewramOverclock);
  if (ewramOverclock)
    SCENE_overclockEWRAM();

  bool ps2Input = SAVEFILE_read8(SRAM->adminSettings.ps2Input);
  if (ps2Input)
    ps2Keyboard->activate();

  if (videoStore->isActivating()) {
    videoStore->disable();
    return SEQUENCE_halt(VIDEO_ACTIVATION_FAILED_CRASH);
  }

  if (videoStore->isEnabled()) {
    auto nextScene = SEQUENCE_activateVideo(false);
    if (nextScene != NULL)
      return nextScene;
  }

  bool isPlaying = SAVEFILE_read8(SRAM->state.isPlaying);
  bool isShuffleMode = ENV_ARCADE || (SAVEFILE_read8(SRAM->isShuffleMode) == 1);
  auto gameMode = SAVEFILE_getGameMode();
  SAVEFILE_write8(SRAM->state.isPlaying, false);

  if (isPlaying && gameMode == GameMode::DEATH_MIX)
    return new DeathMixScene(_engine, _fs, static_cast<MixMode>(isShuffleMode));

  if (isPlaying && !IS_MULTIPLAYER(gameMode))
    return new SelectionScene(_engine, _fs);

  return new ControlsScene(_engine, _fs,
                           []() { goTo(SEQUENCE_getCalibrateOrMainScene()); });
}

Scene* SEQUENCE_getCalibrateOrMainScene() {
  bool isAudioLagCalibrated = SAVEFILE_read8(SRAM->memory.isAudioLagCalibrated);

  if (!isAudioLagCalibrated) {
    auto scene = new TalkScene(_engine, _fs, CALIBRATE_AUDIO_LAG, [](u16 keys) {
      if (KEY_STA(keys)) {
        SAVEFILE_write8(SRAM->memory.isAudioLagCalibrated, 1);
        goTo(new CalibrateScene(_engine, _fs,
                                []() { goTo(SEQUENCE_getMainScene()); }));
      }

      if (KEY_SEL(keys)) {
        SAVEFILE_write8(SRAM->memory.isAudioLagCalibrated, 1);
        goTo(SEQUENCE_getMainScene());
      }
    });
    scene->withButton = false;
    return scene;
  }

  return SEQUENCE_getMainScene();
}

Scene* SEQUENCE_getMainScene() {
  return new StartScene(_engine, _fs);
}

Scene* SEQUENCE_activateVideo(bool showSuccessMessage) {
  auto videoState = videoStore->activate();
  switch (videoState) {
    case VideoStore::NO_SUPPORTED_FLASHCART: {
      return SEQUENCE_halt(VIDEO_ACTIVATION_FAILED_NO_FLASHCART);
      break;
    }
    case VideoStore::MOUNT_ERROR: {
      return SEQUENCE_halt(VIDEO_ACTIVATION_FAILED_MOUNT_FAILED);
      break;
    }
    case VideoStore::ACTIVE:
    default: {
      return showSuccessMessage ? SEQUENCE_halt(VIDEO_ACTIVATION_SUCCESS)
                                : NULL;
    }
  }
}

Scene* SEQUENCE_deactivateVideo() {
  videoStore->disable();
  return SEQUENCE_halt(VIDEO_DEACTIVATION_SUCCESS);
}

Scene* SEQUENCE_activateEWRAMOverclock() {
  SAVEFILE_write8(SRAM->adminSettings.ewramOverclock, true);
  return SEQUENCE_halt(EWRAM_OVERCLOCK_ENABLED);
}

Scene* SEQUENCE_deactivateEWRAMOverclock() {
  SCENE_unoverclockEWRAM();
  SAVEFILE_write8(SRAM->adminSettings.ewramOverclock, false);
  return SEQUENCE_halt(EWRAM_OVERCLOCK_DISABLED);
}

Scene* SEQUENCE_halt(std::string error) {
  auto scene = new TalkScene(_engine, _fs, error, [](u16 keys) {});
  scene->withButton = false;
  return scene;
}

void SEQUENCE_goToGameMode(GameMode gameMode) {
  bool areArcadeModesUnlocked = SAVEFILE_isModeUnlocked(GameMode::ARCADE);
  bool isImpossibleModeUnlocked = SAVEFILE_isModeUnlocked(GameMode::IMPOSSIBLE);

  if (IS_ARCADE(gameMode) && !areArcadeModesUnlocked) {
    goTo(new TalkScene(
        _engine, _fs, ARCADE_MODE_LOCKED,
        [](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new StartScene(_engine, _fs));
        },
        true));
    return;
  }

  if (gameMode == GameMode::IMPOSSIBLE && !isImpossibleModeUnlocked) {
    goTo(new TalkScene(
        _engine, _fs, IMPOSSIBLE_MODE_LOCKED,
        [](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new StartScene(_engine, _fs));
        },
        true));
    return;
  }

  if (IS_MULTIPLAYER(gameMode) && ps2Keyboard->isActive()) {
    goTo(new TalkScene(
        _engine, _fs, MULTIPLAYER_UNAVAILABLE_PS2_ON,
        [](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new StartScene(_engine, _fs));
        },
        true));
    return;
  }

  u16 keys = ~REG_KEYS & KEY_ANY;
  bool isHoldingStart = KEY_STA(keys);
  bool isHoldingL = keys & KEY_L;
  bool isHoldingR = keys & KEY_R;
  auto lastGameMode = SAVEFILE_getGameMode();
  bool wasBonusMode =
      lastGameMode == GameMode::ARCADE && SAVEFILE_read8(SRAM->isBonusMode);
  bool isBonusMode = gameMode == GameMode::ARCADE &&
                     SAVEFILE_bonusCount(_fs) > 0 && isHoldingStart;
  bool isShuffleMode =
      gameMode == GameMode::DEATH_MIX && (ENV_ARCADE || isHoldingStart);

  bool isTransitioningBetweenCampaignAndChallenges =
      (lastGameMode == GameMode::CAMPAIGN && IS_CHALLENGE(gameMode)) ||
      (IS_CHALLENGE(lastGameMode) && gameMode == GameMode::CAMPAIGN);
  bool isTransitioningBetweenArcadeAndNonArcadeModes =
      lastGameMode != gameMode && !isTransitioningBetweenCampaignAndChallenges;
  bool isTransitioningBetweenBonusArcadeAndOtherMode =
      wasBonusMode != isBonusMode;

  if (isTransitioningBetweenArcadeAndNonArcadeModes ||
      isTransitioningBetweenBonusArcadeAndOtherMode) {
    bool arcadeAndCampaignUseTheSameLibrary =
        SAVEFILE_getMaxLibraryType(true) ==
        static_cast<DifficultyLevel>(
            SAVEFILE_read8(SRAM->memory.difficultyLevel));
    bool shouldKeepCursor = !isTransitioningBetweenBonusArcadeAndOtherMode &&
                            arcadeAndCampaignUseTheSameLibrary;

    bool shouldResetCursor = !shouldKeepCursor;
    SAVEFILE_write8(SRAM->memory.numericLevel, 0);
    if (shouldResetCursor) {
      auto songIndex = IS_STORY(gameMode) ? SAVEFILE_getLibrarySize() - 1 : 0;
      SAVEFILE_write8(SRAM->memory.pageIndex, Div(songIndex, PAGE_SIZE));
      SAVEFILE_write8(SRAM->memory.songIndex, DivMod(songIndex, PAGE_SIZE));
    }
  }

  SAVEFILE_write8(SRAM->state.gameMode, gameMode);
  SAVEFILE_write8(SRAM->isBonusMode, false);
  SAVEFILE_write8(SRAM->isShuffleMode, false);
  if (IS_MULTIPLAYER(gameMode)) {
    linkUniversal->setProtocol(isHoldingL ? LinkUniversal::Protocol::CABLE
                               : isHoldingR
                                   ? LinkUniversal::Protocol::WIRELESS_SERVER
                                   : LinkUniversal::Protocol::AUTODETECT);

    SEQUENCE_goToMultiplayerGameMode(gameMode);
  } else if (gameMode == GameMode::DEATH_MIX) {
    SAVEFILE_write8(SRAM->isShuffleMode, isShuffleMode);
    goTo(new DeathMixScene(_engine, _fs, static_cast<MixMode>(isShuffleMode)));
  } else {
    SAVEFILE_write8(SRAM->isBonusMode, isBonusMode);

    auto message = gameMode == GameMode::CAMPAIGN ? MODE_CAMPAIGN
                   : gameMode == GameMode::ARCADE
                       ? (isBonusMode ? MODE_ARCADE_BONUS : MODE_ARCADE)
                       : MODE_IMPOSSIBLE;
    goTo(new TalkScene(
        _engine, _fs, message,
        [](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new SelectionScene(_engine, _fs));
        },
        true));
  }
}

void SEQUENCE_goToMultiplayerGameMode(GameMode gameMode) {
  goTo(new MultiplayerLobbyScene(_engine, _fs,
                                 gameMode == GameMode::MULTI_COOP
                                     ? SyncMode::SYNC_MODE_COOP
                                     : SyncMode::SYNC_MODE_VS));
}

void SEQUENCE_goToMessageOrSong(Song* song, Chart* chart, Chart* remoteChart) {
  STATE_setup(song, chart);

  auto gameMode = SAVEFILE_getGameMode();

  if (!isMultiplayer())
    SAVEFILE_write8(SRAM->state.isPlaying, true);

  if (gameMode == GameMode::CAMPAIGN && song->applyTo[chart->difficulty] &&
      song->hasMessage) {
    goTo(new TalkScene(_engine, _fs, std::string(song->message),
                       [song, chart](u16 keys) {
                         bool isPressed = KEY_CONFIRM(keys);
                         if (isPressed)
                           goTo(new SongScene(_engine, _fs, song, chart));
                       }));
    return;
  }

  if (gameMode == GameMode::CAMPAIGN && song->index == 0) {
    goTo(new TalkScene(
        _engine, _fs, KEYS_HINT,
        [song, chart](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new SongScene(_engine, _fs, song, chart));
        },
        true));
    return;
  }

  if (gameMode == GameMode::ARCADE &&
      GameState.mods.trainingMode == TrainingModeOpts::tON) {
    goTo(new TalkScene(
        _engine, _fs, KEYS_TRAINING_HINT,
        [song, chart](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new SongScene(_engine, _fs, song, chart));
        },
        true));
    return;
  }

  if (gameMode == GameMode::ARCADE && isSinglePlayerDouble() &&
      chart->type == ChartType::DOUBLE_COOP_CHART) {
    goTo(new TalkScene(
        _engine, _fs, COOP_HINT,
        [song, chart](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new SongScene(_engine, _fs, song, chart));
        },
        true));
    return;
  }

  bool ps2Input = SAVEFILE_read8(SRAM->adminSettings.ps2Input);
  if (gameMode == GameMode::ARCADE && isSinglePlayerDouble() && ps2Input) {
    goTo(new TalkScene(
        _engine, _fs, DOUBLE_PS2_INPUT_HINT,
        [song, chart](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(new SongScene(_engine, _fs, song, chart));
        },
        true));
    return;
  }

  goTo(new SongScene(_engine, _fs, song, chart, remoteChart));
}

void SEQUENCE_goToWinOrSelection(bool isLastSong) {
  auto gameMode = SAVEFILE_getGameMode();

  if (gameMode == GameMode::DEATH_MIX && GameState.isShuffleMode &&
      isLastSong) {
    goTo(new DeathMixScene(_engine, _fs, MixMode::SHUFFLE));
    return;
  }

  if ((IS_STORY(gameMode) || gameMode == GameMode::DEATH_MIX) && isLastSong)
    goTo(new TalkScene(
        _engine, _fs,
        gameMode == GameMode::DEATH_MIX
            ? (SAVEFILE_bonusCount(_fs) > 0 ? WIN_DEATHMIX : WIN_IMPOSSIBLE)
        : gameMode == GameMode::CAMPAIGN ? WIN
                                         : WIN_IMPOSSIBLE,
        [](u16 keys) {
          bool isPressed = KEY_CONFIRM(keys);
          if (isPressed)
            goTo(SEQUENCE_getMainScene());
        }));
  else
    goTo(new SelectionScene(_engine, _fs));
}

void SEQUENCE_goToAdminMenuHint() {
  goTo(new TalkScene(_engine, _fs, OPEN_ADMIN_MENU_HINT, [](u16 keys) {
    bool isPressed = KEY_CONFIRM(keys);
    if (isPressed)
      goTo(SEQUENCE_getMainScene());
  }));
}

bool SEQUENCE_isMultiplayerSessionDead() {
  return isMultiplayer() && !syncer->isPlaying();
}
