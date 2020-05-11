#include "SongScene.h"

#include <libgba-sprite-engine/effects/fade_out_scene.h>
#include <libgba-sprite-engine/palette/palette_manager.h>

#include "SelectionScene.h"
#include "StageBreakScene.h"
#include "data/content/_compiled_sprites/palette_song.h"
#include "gameplay/Key.h"
#include "player/PlaybackState.h"
#include "ui/Darkener.h"
#include "utils/BackgroundUtils.h"
#include "utils/EffectUtils.h"
#include "utils/SpriteUtils.h"

extern "C" {
#include "player/player.h"
}

const u32 ID_DARKENER = 0;
const u32 ID_MAIN_BACKGROUND = 1;
const u32 ARROW_POOL_SIZE = 50;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 24;

static std::unique_ptr<Darkener> darkener{new Darkener(ID_DARKENER)};

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
  IFTEST { return {}; }
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

  for (u32 i = 0; i < ARROWS_TOTAL; i++)
    sprites.push_back(arrowHolders[i]->get());

  return sprites;
}

void SongScene::load() {
  player_play(song->audioPath.c_str());

  BACKGROUND_enable(false, false, false, false);
  IFTEST { BACKGROUND_enable(true, false, false, false); }

  setUpPalettes();
  IFNOTTEST { setUpBackground(); }
  setUpArrows();

  lifeBar = std::unique_ptr<LifeBar>(new LifeBar());
  score = std::unique_ptr<Score>{new Score(lifeBar.get())};

  judge = std::unique_ptr<Judge>(
      new Judge(arrowPool.get(), &arrowHolders, score.get(), [this]() {
        IFNOTTEST {
          unload();
          engine->transitionIntoScene(new StageBreakScene(engine),
                                      new FadeOutScene(2));
        }
      }));
  chartReader = std::unique_ptr<ChartReader>(
      new ChartReader(chart, arrowPool.get(), judge.get()));
}

void SongScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  if (!hasStarted) {
    IFNOTTEST {
      BACKGROUND_enable(true, true, false, false);
      darkener->initialize();
    }
    IFTEST { BACKGROUND_setColor(0, 127); }
    hasStarted = true;
  }

  u32 songMsecs = PlaybackState.msecs;

  if (PlaybackState.hasFinished || songMsecs >= song->lastMillisecond) {
    unload();
    engine->transitionIntoScene(new SelectionScene(engine),
                                new FadeOutScene(2));
    return;
  }

  bool isNewBeat = chartReader->preUpdate((int)songMsecs);
  if (isNewBeat)
    for (auto& arrowHolder : arrowHolders) {
      lifeBar->blink(foregroundPalette.get());
      if (!KEY_ANY_PRESSED(keys))
        arrowHolder->blink();
    }

  updateArrowHolders();
  processKeys(keys);
  updateFakeHeads();
  updateArrows();
  score->tick();
  lifeBar->tick(foregroundPalette.get());

  chartReader->postUpdate();
}

void SongScene::setUpPalettes() {
  foregroundPalette = std::unique_ptr<ForegroundPaletteManager>(
      new ForegroundPaletteManager(palette_songPal, sizeof(palette_songPal)));

  u32 backgroundPaletteLength;
  auto backgroundPaletteData = (COLOR*)gbfs_get_obj(
      fs, song->backgroundPalettePath.c_str(), &backgroundPaletteLength);

  backgroundPalette =
      std::unique_ptr<BackgroundPaletteManager>(new BackgroundPaletteManager(
          backgroundPaletteData, backgroundPaletteLength));
}

void SongScene::setUpBackground() {
  u32 backgroundTilesLength, backgroundMapLength;
  auto backgroundTilesData = gbfs_get_obj(fs, song->backgroundTilesPath.c_str(),
                                          &backgroundTilesLength);
  auto backgroundMapData =
      gbfs_get_obj(fs, song->backgroundMapPath.c_str(), &backgroundMapLength);

  bg = std::unique_ptr<Background>(new Background(
      ID_MAIN_BACKGROUND, backgroundTilesData, backgroundTilesLength,
      backgroundMapData, backgroundMapLength));
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void SongScene::setUpArrows() {
  arrowPool = std::unique_ptr<ObjectPool<Arrow>>{new ObjectPool<Arrow>(
      ARROW_POOL_SIZE, [](u32 id) -> Arrow* { return new Arrow(id); })};

  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    arrowHolders.push_back(std::unique_ptr<ArrowHolder>{
        new ArrowHolder(static_cast<ArrowDirection>(i))});

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
  arrowPool->forEachActive([this](Arrow* it) {
    ArrowDirection direction = it->direction;
    bool isStopped = chartReader->isStopped();
    bool isOut = false;

    if (!isStopped || it->getIsPressed()) {
      int newY = chartReader->getYFor(it->timestamp);
      bool isPressing = arrowHolders[direction]->getIsPressed();
      ArrowState arrowState = it->tick(chartReader.get(), newY, isPressing);
      if (arrowState == ArrowState::OUT) {
        isOut = true;
        judge->onOut(it);
      }
    }

    bool canBeJudged = false;
    int judgementOffset = 0;
    if (isStopped) {
      bool hasJustStopped = chartReader->hasJustStopped();
      bool isAboutToResume = chartReader->isAboutToResume();

      canBeJudged = it->timestamp >= chartReader->getStopStart() &&
                    (hasJustStopped || isAboutToResume);
      judgementOffset = isAboutToResume ? -chartReader->getStopLength() : 0;
    } else {
      int nextStopStart = 0;
      canBeJudged = chartReader->isAboutToStop(&nextStopStart)
                        ? it->timestamp < nextStopStart
                        : true;
    }

    bool hasBeenPressedNow = arrowHolders[direction]->hasBeenPressedNow();
    if (!isOut && canBeJudged && hasBeenPressedNow)
      judge->onPress(it, chartReader.get(), judgementOffset);
  });
}

void SongScene::updateFakeHeads() {
  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    auto direction = static_cast<ArrowDirection>(i);

    bool isHoldMode = chartReader->isHoldActive(direction);
    bool isPressing = arrowHolders[direction]->getIsPressed();
    bool isVisible = fakeHeads[i]->get()->enabled;

    if (isHoldMode && isPressing && !chartReader->isStopped()) {
      if (!isVisible) {
        fakeHeads[i]->initialize(ArrowType::HOLD_FAKE_HEAD, direction, 0);
        isVisible = true;
      }
    } else if (isVisible) {
      fakeHeads[i]->discard();
      isVisible = false;
    }

    if (isVisible)
      fakeHeads[i]->tick(chartReader.get(), 0, false);
  }
}

void SongScene::processKeys(u16 keys) {
  arrowHolders[0]->setIsPressed(KEY_DOWNLEFT(keys));
  arrowHolders[1]->setIsPressed(KEY_UPLEFT(keys));
  arrowHolders[2]->setIsPressed(KEY_CENTER(keys));
  arrowHolders[3]->setIsPressed(KEY_UPRIGHT(keys));
  arrowHolders[4]->setIsPressed(KEY_DOWNRIGHT(keys));

  IFKEYTEST {
    for (auto& arrowHolder : arrowHolders)
      if (arrowHolder->hasBeenPressedNow())
        arrowPool->create([&arrowHolder, this](Arrow* it) {
          it->initialize(
              ArrowType::UNIQUE, arrowHolder->direction,
              chartReader->getMsecs() + chartReader->getTimeNeeded());
        });
  }
}

void SongScene::unload() {
  player_stop();
}

SongScene::~SongScene() {
  EFFECT_turnOffBlend();
  arrowHolders.clear();
  fakeHeads.clear();
  Song_free(song);
}
