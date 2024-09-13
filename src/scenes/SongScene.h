#ifndef SONG_SCENE_H
#define SONG_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba/tonc_bios.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include "gameplay/ChartReader.h"
#include "gameplay/DeathMix.h"
#include "gameplay/Judge.h"
#include "gameplay/multiplayer/Syncer.h"
#include "gameplay/video/VideoStore.h"
#include "objects/Arrow.h"
#include "objects/ArrowHolder.h"
#include "objects/LifeBar.h"
#include "objects/base/InputHandler.h"
#include "objects/score/Score.h"
#include "utils/PixelBlink.h"
#include "utils/pool/ObjectPool.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

struct RewindState {
  u32 rewindPoint = 0;
  u32 multiplier = 3;
  u32 rate = 1;
  bool isInitializing = false;
  bool isRewinding = false;
  bool isSavingPoint = false;
};

class SongScene : public Scene {
 public:
  explicit SongScene(std::shared_ptr<GBAEngine> engine,
                     const GBFS_FILE* fs,
                     Song* song,
                     Chart* chart,
                     Chart* remoteChart = NULL,
                     std::unique_ptr<DeathMix> deathMix = NULL,
                     RewindState rewindState = RewindState{});

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;
  void render() override;

  ~SongScene();

 private:
  u32 init = 0;
  std::unique_ptr<Background> bg;
  std::unique_ptr<PixelBlink> pixelBlink;
  const GBFS_FILE* fs;

  Song* song;
  Chart* chart;
  Chart* remoteChart;
  std::unique_ptr<ChartReader> chartReaders[GAME_MAX_PLAYERS];
  std::unique_ptr<LifeBar> lifeBars[GAME_MAX_PLAYERS];
  std::array<std::unique_ptr<Score>, GAME_MAX_PLAYERS> scores;
  std::unique_ptr<Judge> judge;
  std::vector<std::unique_ptr<ArrowHolder>> arrowHolders;
  std::vector<std::unique_ptr<Arrow>> fakeHeads;
  std::unique_ptr<ObjectPool<Arrow>> arrowPool;
  std::unique_ptr<InputHandler> startInput;
  std::unique_ptr<InputHandler> selectInput;
  std::unique_ptr<InputHandler> aInput;
  std::unique_ptr<InputHandler> bInput;
  std::unique_ptr<InputHandler> rateDownPs2Input;
  std::unique_ptr<InputHandler> rateUpPs2Input;
  std::unique_ptr<DeathMix> deathMix;
  bool $isMultiplayer, $isDouble, $isVs, $isSinglePlayerDouble,
      $isVsDifferentLevels, $ps2Input, usesVideo;
  u32 platformCount, playerCount, localBaseIndex, remoteBaseIndex,
      localPlayerId, rumbleTotalFrames, rumblePreRollFrames,
      rumbleIdleCyclePeriod;
  int rate = 0;
  u32 blinkFrame = 0;
  u8 targetMosaic = 0;
  u8 mosaic = 0;
  bool waitMosaic = true;
  int jumpDirection = 1;
  int reduceDirection = 1;
  int bounceDirection = -1;
  int rumbleBeatFrame = -1;
  u32 rumbleIdleFrame = 0;
  u32 autoModDuration = 1;
  u32 autoModCounter = 0;
  u32 lastDownLeftKeys = 0;
  u32 lastUpLeftKeys = 0;
  u32 lastCenterKeys = 0;
  RewindState rewindState;

  inline void setUpGameConfig() {
    $isMultiplayer = isMultiplayer();
    $isDouble = isDouble();
    $isVs = isVs();
    $isSinglePlayerDouble = isSinglePlayerDouble();
    $isVsDifferentLevels = remoteChart->level != chart->level;
    $ps2Input = SAVEFILE_read8(SRAM->adminSettings.ps2Input);

    usesVideo = videoStore->isActive();
    platformCount = isMultiplayer() || isSinglePlayerDouble() ? 2 : 1;
    playerCount = 1 + isVs();
    localBaseIndex = isMultiplayer()
                         ? getBaseIndexFromPlayerId(syncer->getLocalPlayerId())
                         : 0;
    remoteBaseIndex = getBaseIndexFromPlayerId(syncer->getRemotePlayerId());
    localPlayerId = isVs() ? syncer->getLocalPlayerId() : 0;

    u8 rumbleOpts = SAVEFILE_read8(SRAM->adminSettings.rumbleOpts);
    rumbleTotalFrames = SAVEFILE_read8(SRAM->adminSettings.rumbleFrames);
    rumblePreRollFrames = RUMBLE_PREROLL(rumbleOpts);
    rumbleIdleCyclePeriod = RUMBLE_IDLE(rumbleOpts);
  }

  inline bool shouldForceGSM() {
    return $isMultiplayer ||
           GameState.mods.trainingMode != TrainingModeOpts::tOFF;
  }

  inline ArrowDirection getDirectionFromIndex(u32 index) {
    return static_cast<ArrowDirection>($isDouble ? index
                                                 : DivMod(index, ARROWS_TOTAL));
  }

  inline u8 getPlayerIdFromIndex(u32 index) {
    return $isDouble ? 0 : (index >= ARROWS_TOTAL ? 1 : 0);
  }

  inline u8 getBaseIndexFromPlayerId(u8 playerId) {
    return playerId * ARROWS_TOTAL;
  }

  void setUpPalettes();
  void setUpBackground();
  void setUpArrows();
  void initializeBackground();
  bool initializeGame(u16 keys);

  void updateArrowHolders();
  void updateArrows();
  void updateBlink();
  void updateFakeHeads();
  void updateScoresAndLifebars();
  void updateGameX();
  void updateGameY();
  void updateRumble();
  void animateWinnerLifeBar();
  void prepareVideo();
  void drawVideo();
  void throwVideoError();
  void processKeys(u16 keys);

  void onNewBeat(bool isAnyKeyPressed);
  void onStageBreak(u8 playerId);
  void onStagePass();
  void onAbort();
  void breakStage();
  void updateHighestLevel();
  void finishAndGoToEvaluation();
  void continueDeathMix();

  void processModsLoad();
  void processModsTick();
  void processModsBeat();
  u8 processPixelateMod();
  void processTrainingModeMod();
  void processMultiplayerUpdates();
  bool setRate(int rate);
  void startSeek(u32 msecs);
  void endSeek(u32 previousMultiplier);
  bool seek(u32 msecs);

  std::string buildLevelString();

  void unload();

#ifdef SENV_DEVELOPMENT
  u32 profsum = 0;
  u32 profcount = 0;
  void profileStart() {
    REG_TM2CNT_L = 0;
    REG_TM3CNT_L = 0;

    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;

    REG_TM3CNT_H = TM_ENABLE | TM_CASCADE;
    REG_TM2CNT_H = TM_ENABLE | TM_FREQ_1;
  }
  void profileStop() {
    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;

    profsum += (REG_TM2CNT_L | (REG_TM3CNT_L << 16));
    profcount++;
  }
  void profilePrint() {
    if (profsum > 0)
      log("AVG cycles: %d", profsum / profcount);
  }
#endif
};

#endif  // SONG_SCENE_H
