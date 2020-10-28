#include "SongScene.h"

#include <libgba-sprite-engine/effects/fade_out_scene.h>
#include <libgba-sprite-engine/gba/tonc_math.h>
#include <libgba-sprite-engine/palette/palette_manager.h>

#include "DanceGradeScene.h"
#include "StageBreakScene.h"
#include "data/content/_compiled_sprites/palette_song.h"
#include "gameplay/Key.h"
#include "gameplay/save/SaveFile.h"
#include "player/PlaybackState.h"
#include "ui/Darkener.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "player/player.h"
}

#define DEBUG_OFFSET_CORRECTION 8

const u32 DARKENER_ID = 0;
const u32 DARKENER_PRIORITY = 2;
const u32 MAIN_BACKGROUND_ID = 1;
const u32 MAIN_BACKGROUND_PRIORITY = 3;
const u32 ARROW_POOL_SIZE = 50;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 24;
const u32 ALPHA_BLINK_TIME = 6;
const u32 ALPHA_BLINK_LEVEL = 10;
const u32 PIXEL_BLINK_LEVEL = 2;

static std::unique_ptr<Darkener> darkener{
    new Darkener(DARKENER_ID, DARKENER_PRIORITY)};

SongScene::SongScene(std::shared_ptr<GBAEngine> engine,
                     const GBFS_FILE* fs,
                     Song* song,
                     Chart* chart)
    : Scene(engine) {
  this->fs = fs;
  this->song = song;
  this->chart = chart;
}

std::vector<Background*> SongScene::backgrounds() {
  if (ENV_DEBUG)
    return {};

  return {bg.get()};
}

std::vector<Sprite*> SongScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(lifeBar->get());
  score->render(&sprites);

  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    fakeHeads[i]->index = sprites.size();
    sprites.push_back(fakeHeads[i]->get());
  }

  arrowPool->forEach([&sprites](Arrow* it) {
    it->index = sprites.size();
    sprites.push_back(it->get());
  });

  for (auto& it : arrowHolders)
    sprites.push_back(it->get());

  return sprites;
}

void SongScene::load() {
  SAVEFILE_write8(SRAM->state.isPlaying, 1);
  player_play(song->audioPath.c_str());

  SCENE_init();

  setUpPalettes();
  if (!ENV_DEBUG)
    setUpBackground();
  setUpArrows();

  pixelBlink = std::unique_ptr<PixelBlink>(new PixelBlink(PIXEL_BLINK_LEVEL));
  lifeBar = std::unique_ptr<LifeBar>(new LifeBar());
  score = std::unique_ptr<Score>{new Score(lifeBar.get())};

  int audioLag = (int)SAVEFILE_read32(SRAM->settings.audioLag);
  u32 multiplier = GameState.mods.multiplier;
  judge = std::unique_ptr<Judge>(
      new Judge(arrowPool.get(), &arrowHolders, score.get(), [this]() {
        if (GameState.mods.stageBreak != StageBreakOpts::sOFF) {
          unload();
          engine->transitionIntoScene(new StageBreakScene(engine, fs),
                                      new FadeOutScene(6));
        }
      }));
  chartReader = std::unique_ptr<ChartReader>(
      new ChartReader(chart, arrowPool.get(), judge.get(), pixelBlink.get(),
                      audioLag, multiplier));

  speedUpInput = std::unique_ptr<InputHandler>(new InputHandler());
  speedDownInput = std::unique_ptr<InputHandler>(new InputHandler());
}

void SongScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  if (init == 0) {
    if (!ENV_DEBUG) {
      auto gamePosition =
          GameState.mods.jump ? 0 : SAVEFILE_read8(SRAM->settings.gamePosition);
      auto type = static_cast<BackgroundType>(
          SAVEFILE_read8(SRAM->settings.backgroundType));
      darkener->initialize(gamePosition, type);
    } else
      BACKGROUND_setColor(0, 127);

    if (GameState.mods.negative) {
      SCENE_invert(backgroundPalette.get());
      SCENE_invert(foregroundPalette.get());
    }

    init++;
    return;
  } else if (init == 1) {
    BACKGROUND_enable(true, !ENV_DEBUG, false, false);
    SPRITE_enable();
    processModsLoad();
    init++;
  }

  u32 songMsecs = PlaybackState.msecs;

  if (PlaybackState.hasFinished || songMsecs >= song->lastMillisecond) {
    finishAndGoToEvaluation();
    return;
  }

  updateArrowHolders();
  processKeys(keys);

  bool isNewBeat = chartReader->update((int)songMsecs);
  if (isNewBeat) {
    blinkFrame += ALPHA_BLINK_TIME;

    for (auto& arrowHolder : arrowHolders) {
      lifeBar->blink(foregroundPalette.get());
      if (!KEY_ANY_PRESSED(keys))
        arrowHolder->blink();
    }

    processModsBeat();
  }

  blinkFrame = max(blinkFrame - 1, 0);
  if (SAVEFILE_read8(SRAM->settings.bgaDarkBlink))
    EFFECT_setBlendAlpha(ALPHA_BLINK_LEVEL - blinkFrame);

  processModsTick();
  u8 minMosaic = processPixelateMod();
  pixelBlink->tick(minMosaic);
  updateFakeHeads();
  updateArrows();
  score->tick();
  lifeBar->tick(foregroundPalette.get());

  if (ENV_DEVELOPMENT && chartReader->offset)
    score->log(chartReader->offset);

  IFTIMINGTEST { chartReader->logDebugInfo<CHART_DEBUG>(); }
}

void SongScene::setUpPalettes() {
  foregroundPalette = std::unique_ptr<ForegroundPaletteManager>(
      new ForegroundPaletteManager(palette_songPal, sizeof(palette_songPal)));

  backgroundPalette =
      BACKGROUND_loadPaletteFile(fs, song->backgroundPalettePath.c_str());
}

void SongScene::setUpBackground() {
  bg = BACKGROUND_loadBackgroundFiles(fs, song->backgroundTilesPath.c_str(),
                                      song->backgroundMapPath.c_str(),
                                      MAIN_BACKGROUND_ID);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->usePriority(MAIN_BACKGROUND_PRIORITY);
  bg->setMosaic(true);
}

void SongScene::setUpArrows() {
  arrowPool = std::unique_ptr<ObjectPool<Arrow>>{new ObjectPool<Arrow>(
      ARROW_POOL_SIZE, [](u32 id) -> Arrow* { return new Arrow(id); })};

  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    arrowHolders.push_back(std::unique_ptr<ArrowHolder>{
        new ArrowHolder(static_cast<ArrowDirection>(i), true)});
    arrowHolders[i]->get()->setPriority(ARROW_LAYER_BACK);

    auto fakeHead =
        std::unique_ptr<Arrow>{new Arrow(ARROW_TILEMAP_LOADING_ID + i)};
    SPRITE_hide(fakeHead->get());
    fakeHeads.push_back(std::move(fakeHead));
  }
}

void SongScene::updateArrowHolders() {
  for (auto& it : arrowHolders)
    it->tick();
}

void SongScene::updateArrows() {
  std::array<Arrow*, ARROWS_TOTAL> nextArrows;
  for (u32 i = 0; i < ARROWS_TOTAL; i++)
    nextArrows[i] = NULL;

  // update sprites
  arrowPool->forEachActive([&nextArrows, this](Arrow* arrow) {
    ArrowDirection direction = arrow->direction;
    bool isStopped = chartReader->isStopped();

    int newY = chartReader->getYFor(arrow);
    bool isPressing = arrowHolders[direction]->getIsPressed() && !isStopped;
    ArrowState arrowState = arrow->tick(newY, isPressing);

    if (arrowState == ArrowState::OUT) {
      judge->onOut(arrow);
      return;
    }

    bool canBeJudged =
        arrow->type == ArrowType::UNIQUE && !arrow->getIsPressed();
    if (canBeJudged && (nextArrows[direction] == NULL ||
                        arrow->timestamp < nextArrows[direction]->timestamp))
      nextArrows[direction] = arrow;
  });

  // judge key press events
  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    auto arrow = nextArrows[i];
    bool isStopped = chartReader->isStopped();
    if (arrow == NULL)
      continue;

    ArrowDirection direction = arrow->direction;
    bool canBeJudged = true;
    int judgementOffset = 0;

    if (isStopped) {
      bool hasJustStopped = chartReader->hasJustStopped();
      bool isAboutToResume = chartReader->isAboutToResume();

      canBeJudged = arrow->timestamp >= chartReader->getStopStart() &&
                    (hasJustStopped || isAboutToResume);
      judgementOffset = isAboutToResume ? -chartReader->getStopLength() : 0;

      if (chartReader->isStopJudgeable()) {
        canBeJudged = true;
        judgementOffset =
            -(chartReader->getMsecs() - chartReader->getStopStart());
      }
    }

    bool hasBeenPressedNow = arrowHolders[direction]->hasBeenPressedNow();
    if (canBeJudged && hasBeenPressedNow)
      judge->onPress(arrow, chartReader.get(), judgementOffset);
  }
}

void SongScene::updateFakeHeads() {
  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    auto direction = static_cast<ArrowDirection>(i);

    bool isHoldMode = chartReader->isHoldActive(direction);
    bool isPressing = arrowHolders[direction]->getIsPressed();
    bool isVisible = fakeHeads[i]->get()->enabled;

    if (isHoldMode && isPressing && !chartReader->isStopped()) {
      if (!isVisible) {
        fakeHeads[i]->initialize(ArrowType::HOLD_FAKE_HEAD, direction, 0,
                                 false);
        isVisible = true;
      }
    } else if (isVisible) {
      fakeHeads[i]->discard();
      isVisible = false;
    }

    if (isVisible)
      fakeHeads[i]->tick(0, false);
  }
}

void SongScene::updateGameX() {
  lifeBar->get()->moveTo(GameState.positionX + LIFEBAR_POSITION_X,
                         lifeBar->get()->getY());
  for (auto& it : arrowHolders)
    it->get()->moveTo(ARROW_CORNER_MARGIN_X() + ARROW_MARGIN * it->direction,
                      it->get()->getY());
  score->relocate();

  auto backgroundType = static_cast<BackgroundType>(
      SAVEFILE_read8(SRAM->settings.backgroundType));
  if (backgroundType == BackgroundType::HALF_BGA_DARK)
    REG_BG_OFS[DARKENER_ID].x = -GameState.positionX;
}

void SongScene::updateGameY() {
  lifeBar->get()->moveTo(lifeBar->get()->getX(),
                         GameState.positionY + LIFEBAR_POSITION_Y);
  for (auto& it : arrowHolders)
    it->get()->moveTo(it->get()->getX(), ARROW_FINAL_Y());
}

void SongScene::processKeys(u16 keys) {
  arrowHolders[0]->setIsPressed(KEY_DOWNLEFT(keys));
  arrowHolders[1]->setIsPressed(KEY_UPLEFT(keys));
  arrowHolders[2]->setIsPressed(KEY_CENTER(keys));
  arrowHolders[3]->setIsPressed(KEY_UPRIGHT(keys));
  arrowHolders[4]->setIsPressed(KEY_DOWNRIGHT(keys));
  speedUpInput->setIsPressed(keys & KEY_START);
  speedDownInput->setIsPressed(keys & KEY_SELECT);

  if (!GameState.mods.randomSpeed) {
    if (speedUpInput->hasBeenPressedNow()) {
      if (ENV_DEVELOPMENT && KEY_CENTER(keys)) {
        chartReader->offset -= DEBUG_OFFSET_CORRECTION;
        if (chartReader->offset == 0)
          score->log(0);
      } else {
        if (player_faster())  // TODO: Parameterize
          pixelBlink->blink();
        // if (chartReader->setMultiplier(chartReader->getMultiplier() + 1))
        //   pixelBlink->blink();
      }
    }

    if (speedDownInput->hasBeenPressedNow()) {
      if (ENV_DEVELOPMENT && KEY_CENTER(keys)) {
        chartReader->offset += DEBUG_OFFSET_CORRECTION;
        if (chartReader->offset == 0)
          score->log(0);
      } else {
        if (player_slower())  // TODO: Parameterize
          pixelBlink->blink();
        // if (chartReader->setMultiplier(chartReader->getMultiplier() - 1))
        //   pixelBlink->blink();
      }
    }
  }

  IFSTRESSTEST {
    for (auto& arrowHolder : arrowHolders)
      if (arrowHolder->hasBeenPressedNow())
        arrowPool->create([&arrowHolder, this](Arrow* it) {
          it->initialize(ArrowType::UNIQUE, arrowHolder->direction,
                         chartReader->getMsecs() + chartReader->getArrowTime(),
                         false);
        });
  }
}

void SongScene::finishAndGoToEvaluation() {
  auto evaluation = score->evaluate();
  bool isLastSong =
      SAVEFILE_setGradeOf(song->id, chart->difficulty, evaluation->getGrade());

  unload();
  engine->transitionIntoScene(
      new DanceGradeScene(engine, fs, std::move(evaluation), isLastSong),
      new FadeOutScene(1));
}

void SongScene::processModsLoad() {
  if (GameState.mods.pixelate == PixelateOpts::pFIXED ||
      GameState.mods.pixelate == PixelateOpts::pBLINK_OUT)
    targetMosaic = 4;
  else if (GameState.mods.pixelate == PixelateOpts::pBLINK_IN)
    targetMosaic = 0;
}

void SongScene::processModsBeat() {
  if (GameState.mods.pixelate == PixelateOpts::pBLINK_IN)
    mosaic = 6;
  else if (GameState.mods.pixelate == PixelateOpts::pBLINK_OUT)
    mosaic = 0;
  else if (GameState.mods.pixelate == PixelateOpts::pRANDOM) {
    auto previousTargetMosaic = targetMosaic;
    targetMosaic = qran_range(1, 5);
    if (previousTargetMosaic == targetMosaic)
      mosaic = 1;
  }

  if (GameState.mods.jump == JumpOpts::jRANDOM) {
    int random = qran_range(0, GAME_POSITION_X[GamePosition::RIGHT] + 1);
    GameState.positionX = random;
    updateGameX();
    pixelBlink->blink();
  }

  if (GameState.mods.reduce == ReduceOpts::rRANDOM) {
    int random = qran_range(0, REDUCE_MOD_POSITION_Y);
    GameState.positionY = REDUCE_MOD_POSITION_Y - random;
    updateGameY();
    pixelBlink->blink();
  }

  if (GameState.mods.randomSpeed)
    chartReader->setMultiplier(qran_range(3, 6));
}

void SongScene::processModsTick() {
  if (GameState.mods.jump == JumpOpts::jLINEAR) {
    GameState.positionX += jumpDirection;
    if (GameState.positionX >= (int)GAME_POSITION_X[GamePosition::RIGHT] ||
        GameState.positionX <= 0)
      jumpDirection *= -1;
    updateGameX();
  }

  if (GameState.mods.reduce == ReduceOpts::rLINEAR) {
    GameState.positionY += reduceDirection;
    if (GameState.positionY >= REDUCE_MOD_POSITION_Y ||
        GameState.positionY <= 0)
      reduceDirection *= -1;
    updateGameY();
  }
}

u8 SongScene::processPixelateMod() {
  u8 minMosaic = 0;

  switch (GameState.mods.pixelate) {
    case PixelateOpts::pOFF:
      return 0;
    case PixelateOpts::pLIFE:
      minMosaic = lifeBar->getMosaicValue();
      break;
    case PixelateOpts::pFIXED:
    case PixelateOpts::pBLINK_IN:
    case PixelateOpts::pBLINK_OUT:
    case PixelateOpts::pRANDOM:
      waitMosaic = !waitMosaic;

      if (!waitMosaic) {
        if (targetMosaic > mosaic)
          mosaic++;
        else if (mosaic > targetMosaic)
          mosaic--;
      }

      minMosaic = mosaic;
      break;
  }

  EFFECT_setMosaic(minMosaic);
  return minMosaic;
}

void SongScene::unload() {
  SAVEFILE_write8(SRAM->state.isPlaying, 0);
  player_stopAll();
}

SongScene::~SongScene() {
  arrowHolders.clear();
  fakeHeads.clear();
  SONG_free(song);
}
