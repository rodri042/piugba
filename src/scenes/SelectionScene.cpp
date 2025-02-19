#include "SelectionScene.h"

#include <libgba-sprite-engine/background/text_stream.h>
#include <libgba-sprite-engine/gba/tonc_bios.h>
#include <tonc_input.h>

#include "ModsScene.h"
#include "MultiplayerLobbyScene.h"
#include "SettingsScene.h"
#include "assets.h"
#include "data/content/_compiled_sprites/palette_selection.h"
#include "gameplay/Key.h"
#include "gameplay/Sequence.h"
#include "gameplay/models/Song.h"
#include "utils/SceneUtils.h"

#define CONFIRM_MESSAGE "Press    to start!"

const u32 ID_HIGHLIGHTER = 1;
const u32 ID_MAIN_BACKGROUND = 2;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 16;
const u32 SELECTOR_MARGIN = 3;
const u32 CENTER_X = 96;
const u32 CENTER_Y = 110;
const u32 TEXT_COLOR = 0x7FFF;
const u32 TEXT_ROW = 13;
const int TEXT_SCROLL_NORMAL = -6;
const int TEXT_SCROLL_CONFIRMED = -10;
const u32 PIXEL_BLINK_LEVEL = 4;
const u32 DIFFICULTY_X = 79;
const u32 DIFFICULTY_Y = 16;
const u32 MULTIPLIER_X = 111;
const u32 MULTIPLIER_Y = 34;
const u32 PROGRESS_X = 63;
const u32 PROGRESS_Y = 131;
const u32 CHANNEL_BADGE_X[] = {22, 82, 142, 202};
const u32 CHANNEL_BADGE_Y = 49;
const u32 GRADE_BADGE_X[] = {43, 103, 163, 222};
const u32 GRADE_BADGE_Y = 84;
const u32 LOCK_X[] = {22, 82, 142, 202};
const u32 LOCK_Y = 71;
const u32 NUMERIC_LEVEL_BADGE_X = 104;
const u32 NUMERIC_LEVEL_BADGE_Y = 19;
const u32 NUMERIC_LEVEL_ROW = 3;
const u32 NUMERIC_LEVEL_BADGE_OFFSET_Y = 43;
const u32 NUMERIC_LEVEL_BADGE_OFFSET_ROW = 5;
const u32 LOADING_INDICATORS_X[] = {
    GBA_SCREEN_WIDTH - 16 - 4,
    GBA_SCREEN_WIDTH - 16 - 4,
    31,
    193,
};
const u32 LOADING_INDICATORS_Y[] = {GBA_SCREEN_HEIGHT - 16 - 4, 18};
const std::string SOUND_MOD_STR = SOUND_MOD;

static std::unique_ptr<Highlighter> highlighter{
    new Highlighter(ID_HIGHLIGHTER)};

SelectionScene::SelectionScene(std::shared_ptr<GBAEngine> engine,
                               const GBFS_FILE* fs,
                               InitialLevel initialLevel)
    : Scene(engine) {
  this->fs = fs;
  library = std::unique_ptr<Library>{new Library(fs)};
  this->initialLevel = initialLevel;
}

std::vector<Background*> SelectionScene::backgrounds() {
  return {};
}

std::vector<Sprite*> SelectionScene::sprites() {
  std::vector<Sprite*> sprites;

  for (auto& it : arrowSelectors)
    sprites.push_back(it->get());

  if (isMultiplayer()) {
    sprites.push_back(loadingIndicator1->get());
    sprites.push_back(loadingIndicator2->get());
  }

  for (auto& it : channelBadges)
    sprites.push_back(it->get());

  for (auto& it : gradeBadges)
    sprites.push_back(it->get());

  for (auto& it : locks)
    sprites.push_back(it->get());

  difficulty->render(&sprites);
  sprites.push_back(multiplier->get());
  progress->render(&sprites);

  if (!IS_STORY(SAVEFILE_getGameMode())) {
    sprites.push_back(numericLevelBadge->get());
    if (isVs())
      sprites.push_back(remoteNumericLevelBadge->get());
  }

  return sprites;
}

void SelectionScene::load() {
  if (ENV_ARCADE && IS_STORY(SAVEFILE_getGameMode()))
    BSOD("*Error* (Check your save file)       There's no campaign mode!");
  if (isBonusMode() && SAVEFILE_bonusCount(fs) == 0)
    BSOD("*Error* (Check your save file)          There's no bonus data!");

  if (isMultiplayer()) {
    syncer->clearTimeout();
    syncer->$resetFlag = false;
  }

  SAVEFILE_write8(SRAM->state.isPlaying, false);
  SCENE_init();

  TextStream::instance().scrollNow(0, TEXT_SCROLL_NORMAL);
  TextStream::instance().setMosaic(true);

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};
  difficulty =
      std::unique_ptr<Difficulty>{new Difficulty(DIFFICULTY_X, DIFFICULTY_Y)};
  multiplier = std::unique_ptr<Multiplier>{new Multiplier(
      MULTIPLIER_X, MULTIPLIER_Y, SAVEFILE_read8(SRAM->mods.multiplier))};
  progress = std::unique_ptr<NumericProgress>{
      new NumericProgress(PROGRESS_X, PROGRESS_Y)};
  settingsMenuInput = std::unique_ptr<InputHandler>{new InputHandler()};

  if (IS_STORY(SAVEFILE_getGameMode())) {
    auto level = SAVEFILE_read8(SRAM->memory.difficultyLevel);
    difficulty->setValue(static_cast<DifficultyLevel>(level));
  } else {
    numericLevelBadge = std::unique_ptr<Button>{new Button(
        ButtonType::LEVEL_METER,
        NUMERIC_LEVEL_BADGE_X - (isVs() ? NUMERIC_LEVEL_BADGE_MARGIN : 0),
        NUMERIC_LEVEL_BADGE_Y, false)};
    numericLevelBadge->get()->setPriority(ID_HIGHLIGHTER);
    if (isVs()) {
      remoteNumericLevelBadge = std::unique_ptr<Button>{
          new Button(ButtonType::LEVEL_METER,
                     NUMERIC_LEVEL_BADGE_X + NUMERIC_LEVEL_BADGE_MARGIN,
                     NUMERIC_LEVEL_BADGE_Y, true)};
      remoteNumericLevelBadge->get()->setPriority(ID_HIGHLIGHTER);
    }

    difficulty->setValue(DifficultyLevel::NUMERIC);
    SPRITE_hide(multiplier->get());
  }

  setUpSpritesPalette();
  setUpArrows();
  setUpChannelBadges();
  setUpGradeBadges();
  setUpLocks();
  setUpPager();
}

void SelectionScene::tick(u16 keys) {
  if (engine->isTransitioning() || init < 3)
    return;

  if (SEQUENCE_isMultiplayerSessionDead()) {
    stop();
    SEQUENCE_goToMultiplayerGameMode(SAVEFILE_getGameMode());
    return;
  }

  processKeys(keys);

  if (isCrossingPage == 2)
    stopPageCross2();
  if (pixelBlink->tick() && isCrossingPage == 1)
    stopPageCross1();
  for (auto& it : arrowSelectors)
    it->tick();
  multiplier->tick();

  if (isMultiplayer()) {
    loadingIndicator1->tick();
    loadingIndicator2->tick();
  }

  if (isMultiplayer()) {
    processMultiplayerUpdates();
    if (!syncer->isPlaying())
      return;
  }

  processDifficultyChangeEvents();
  processSelectionChangeEvents();
  processConfirmEvents();
  processMenuEvents();

  blendCount++;
  if (blendCount == 2) {
    blendAlpha = max(min(blendAlpha + (confirmed ? 1 : -1), MAX_OPACITY),
                     HIGHLIGHTER_OPACITY);
    blendCount = 0;
  }
  EFFECT_setBlendAlpha(blendAlpha);

  for (u32 i = 0; i < PAGE_SIZE; i++) {
    if (gradeBadges[i]->getType() == GradeType::S)
      EFFECT_setScale(i, BREATH_SCALE_LUT[animationFrame],
                      BREATH_SCALE_LUT[animationFrame]);
  }
  animationFrame++;
  if (animationFrame >= BREATH_STEPS)
    animationFrame = 0;

  for (auto& channelBadge : channelBadges)
    channelBadge->tick();
}

void SelectionScene::render() {
  if (engine->isTransitioning())
    return;

  if (init == 0) {
    init++;
    return;
  } else if (init == 1) {
    if (isDouble())
      SCENE_applyColorFilter(pal_obj_bank, ColorFilter::DOUBLE_FILTER);
    highlighter->initialize(selected);
    init++;
    return;
  } else if (init == 2) {
    EFFECT_setBlendAlpha(blendAlpha);
    EFFECT_render();
    BACKGROUND_enable(true, true, true, false);
    SPRITE_enable();
    init++;
  }
}

void SelectionScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          palette_selectionPal, sizeof(palette_selectionPal))};
}

void SelectionScene::setUpBackground() {
  auto backgroundFile = library->getPrefix() + std::to_string(getPageStart());
  auto backgroundPaletteFile = backgroundFile + BACKGROUND_PALETTE_EXTENSION;
  auto backgroundTilesFile = backgroundFile + BACKGROUND_TILES_EXTENSION;
  auto backgroundMapFile = backgroundFile + BACKGROUND_MAP_EXTENSION;

  backgroundPalette =
      BACKGROUND_loadPaletteFile(fs, backgroundPaletteFile.c_str());
  backgroundPalette->persist();
  bg = BACKGROUND_loadBackgroundFiles(fs, backgroundTilesFile.c_str(),
                                      backgroundMapFile.c_str(),
                                      ID_MAIN_BACKGROUND, false);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
  bg->persist();

  TextStream::instance().setFontColor(TEXT_COLOR);
  TextStream::instance().setFontSubcolor(text_bg_palette_default_subcolor);

  loadChannels();
  loadProgress();
}

void SelectionScene::setUpArrows() {
  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    auto direction = static_cast<ArrowDirection>(i);
    arrowSelectors.push_back(std::unique_ptr<ArrowSelector>{
        new ArrowSelector(static_cast<ArrowDirection>(direction), direction > 0,
                          direction != ArrowDirection::CENTER)});
  }

  arrowSelectors[ArrowDirection::DOWNLEFT]->get()->moveTo(
      SELECTOR_MARGIN, GBA_SCREEN_HEIGHT - ARROW_SIZE - SELECTOR_MARGIN);
  arrowSelectors[ArrowDirection::UPLEFT]->get()->moveTo(SELECTOR_MARGIN,
                                                        SELECTOR_MARGIN);
  SPRITE_hide(arrowSelectors[ArrowDirection::CENTER]->get());
  arrowSelectors[ArrowDirection::UPRIGHT]->get()->moveTo(
      GBA_SCREEN_WIDTH - ARROW_SIZE - SELECTOR_MARGIN, SELECTOR_MARGIN);
  arrowSelectors[ArrowDirection::DOWNRIGHT]->get()->moveTo(
      GBA_SCREEN_WIDTH - ARROW_SIZE - SELECTOR_MARGIN,
      GBA_SCREEN_HEIGHT - ARROW_SIZE - SELECTOR_MARGIN);

  if (isMultiplayer() && !syncer->isMaster()) {
    SPRITE_hide(arrowSelectors[ArrowDirection::DOWNLEFT]->get());
    SPRITE_hide(arrowSelectors[ArrowDirection::DOWNRIGHT]->get());

    if (isCoop()) {
      SPRITE_hide(arrowSelectors[ArrowDirection::UPLEFT]->get());
      SPRITE_hide(arrowSelectors[ArrowDirection::UPRIGHT]->get());
    }
  }

  if (isMultiplayer()) {
    loadingIndicator1 = std::unique_ptr<Explosion>{
        new Explosion(LOADING_INDICATORS_X[syncer->isMaster() * 2],
                      LOADING_INDICATORS_Y[syncer->isMaster()], true)};
    loadingIndicator2 = std::unique_ptr<Explosion>{
        new Explosion(LOADING_INDICATORS_X[syncer->isMaster() * 2 + 1],
                      LOADING_INDICATORS_Y[syncer->isMaster()], true)};
  }
}

void SelectionScene::setUpChannelBadges() {
  for (u32 i = 0; i < PAGE_SIZE; i++)
    channelBadges.push_back(std::unique_ptr<ChannelBadge>{
        new ChannelBadge(CHANNEL_BADGE_X[i], CHANNEL_BADGE_Y, i > 0)});
}

void SelectionScene::setUpGradeBadges() {
  for (u32 i = 0; i < PAGE_SIZE; i++) {
    gradeBadges.push_back(std::unique_ptr<GradeBadge>{
        new GradeBadge(GRADE_BADGE_X[i], GRADE_BADGE_Y, i > 0, false)});
    gradeBadges[i]->get()->setPriority(ID_MAIN_BACKGROUND);
    gradeBadges[i]->get()->setDoubleSize(true);
    gradeBadges[i]->get()->setAffineId(AFFINE_BASE + i);
  }
}

void SelectionScene::setUpLocks() {
  for (u32 i = 0; i < PAGE_SIZE; i++)
    locks.push_back(std::unique_ptr<Lock>{new Lock(LOCK_X[i], LOCK_Y, i > 0)});
}

void SelectionScene::setUpPager() {
  count = isBonusMode() ? SAVEFILE_bonusCount(fs) : SAVEFILE_getLibrarySize();

  scrollTo(SAVEFILE_read8(SRAM->memory.pageIndex),
           SAVEFILE_read8(SRAM->memory.songIndex));
}

void SelectionScene::scrollTo(u32 songIndex) {
  scrollTo(Div(songIndex, PAGE_SIZE), DivMod(songIndex, PAGE_SIZE));
}

void SelectionScene::scrollTo(u32 page, u32 selected) {
  this->page = page;
  this->selected = selected;

  if (getSelectedSongIndex() > getLastUnlockedSongIndex()) {
    scrollTo(getLastUnlockedSongIndex());
    return;
  }

  setPage(page, 0);
  updateSelection();
}

void SelectionScene::setNumericLevel(u8 numericLevelIndex) {
  if (numericLevelIndex == getSelectedNumericLevelIndex()) {
    updateLastNumericLevel();
    return;
  }

  SAVEFILE_write8(SRAM->memory.numericLevel, numericLevelIndex);
  updateLastNumericLevel();
  updateSelection(true);
  pixelBlink->blink();
}

void SelectionScene::setRemoteNumericLevel(u8 remoteNumericLevelIndex) {
  syncer->setRemoteNumericLevel(remoteNumericLevelIndex, -1);
  updateSelection(true);
  pixelBlink->blink();
}

void SelectionScene::goToSong() {
  if (numericLevels.empty())
    return;

  stop();
  confirmed = false;

  bool isStory = IS_STORY(SAVEFILE_getGameMode());
  bool hasRemoteChart = isVs() && syncer->$remoteNumericLevelIndex != -1;
  std::vector<u8> chartIndexes;

  Song* tempSong = SONG_parse(fs, getSelectedSong());
  if (isStory) {
    chartIndexes.push_back(
        SONG_findChartIndexByDifficultyLevel(tempSong, difficulty->getValue()));
  } else {
    chartIndexes.push_back(SONG_findChartIndexByNumericLevelIndex(
        tempSong, getSelectedNumericLevelIndex(), isDouble()));
    if (hasRemoteChart)
      chartIndexes.push_back(SONG_findChartIndexByNumericLevelIndex(
          tempSong, (u8)syncer->$remoteNumericLevelIndex, false));
  }
  SONG_free(tempSong);

  Song* song = SONG_parse(fs, getSelectedSong(), chartIndexes);
  Chart* chart =
      isStory ? SONG_findChartByDifficultyLevel(song, difficulty->getValue())
              : SONG_findChartByNumericLevelIndex(
                    song, getSelectedNumericLevelIndex(), isDouble());
  Chart* remoteChart =
      hasRemoteChart ? SONG_findChartByNumericLevelIndex(
                           song, (u8)syncer->$remoteNumericLevelIndex, false)
                     : NULL;

  int customOffset = getCustomOffset();
  chart->customOffset = customOffset;
  chart->levelIndex = getSelectedNumericLevelIndex();
  if (remoteChart != NULL) {
    remoteChart->customOffset = customOffset;
    remoteChart->levelIndex = (u8)syncer->$remoteNumericLevelIndex;
  }

  SEQUENCE_goToMessageOrSong(song, chart, remoteChart);
}

void SelectionScene::processKeys(u16 keys) {
  arrowSelectors[ArrowDirection::DOWNLEFT]->setIsPressed(KEY_GOLEFT(keys));
  arrowSelectors[ArrowDirection::UPLEFT]->setIsPressed(KEY_PREV(keys));
  arrowSelectors[ArrowDirection::CENTER]->setIsPressed(KEY_CONFIRM(keys));
  arrowSelectors[ArrowDirection::UPRIGHT]->setIsPressed(KEY_NEXT(keys));
  arrowSelectors[ArrowDirection::DOWNRIGHT]->setIsPressed(KEY_GORIGHT(keys));
  multiplier->setIsPressed(KEY_SEL(keys));
  settingsMenuInput->setIsPressed(KEY_STA(keys));
}

void SelectionScene::processDifficultyChangeEvents() {
  if (isCoop() && !syncer->isMaster())
    return;

  if (IS_STORY(SAVEFILE_getGameMode())) {
    if (onDifficultyLevelChange(
            ArrowDirection::UPRIGHT,
            static_cast<DifficultyLevel>(
                min((int)difficulty->getValue() + 1, MAX_DIFFICULTY))))
      return;

    onDifficultyLevelChange(
        ArrowDirection::UPLEFT,
        static_cast<DifficultyLevel>(max((int)difficulty->getValue() - 1, 0)));
  } else {
    auto currentIndex = getSelectedNumericLevelIndex();
    auto previousIndex = max(currentIndex - 1, 0);
    auto nextIndex = min(currentIndex + 1, max(numericLevels.size() - 1, 0));

    bool didChangeOffset1 = onCustomOffsetChange(ArrowDirection::UPRIGHT, 8);
    bool didChangeOffset2 = onCustomOffsetChange(ArrowDirection::UPLEFT, -8);
    if (didChangeOffset1 || didChangeOffset2)
      return;

    if (onNumericLevelChange(ArrowDirection::UPRIGHT, nextIndex))
      return;

    onNumericLevelChange(ArrowDirection::UPLEFT, previousIndex);
  }
}

void SelectionScene::processSelectionChangeEvents() {
  if (isMultiplayer() && !syncer->isMaster())
    return;

  bool isOnListEdge = getSelectedSongIndex() == getLastUnlockedSongIndex();

#ifdef SENV_DEVELOPMENT
  isOnListEdge = getSelectedSongIndex() == count - 1;
#endif

  if (isCustomOffsetAdjustmentEnabled() && multiplier->getIsPressed())
    return;

  if (onSelectionChange(ArrowDirection::DOWNRIGHT, isOnListEdge,
                        selected == PAGE_SIZE - 1, 1))
    return;

  onSelectionChange(ArrowDirection::DOWNLEFT, page == 0 && selected == 0,
                    selected == 0, -1);
}

void SelectionScene::processConfirmEvents() {
  if (isMultiplayer() && !syncer->isMaster())
    return;

  if (arrowSelectors[ArrowDirection::CENTER]->hasBeenPressedNow()) {
    if (isMultiplayer() && syncer->isMaster())
      syncer->send(SYNC_EVENT_START_SONG, confirmed);

    if (isCustomOffsetAdjustmentEnabled() && multiplier->getIsPressed())
      return;

    onConfirmOrStart(confirmed);
  }
}

void SelectionScene::processMenuEvents() {
  if (isMultiplayer()) {
    if (multiplier->hasBeenPressedNow()) {
      syncer->initialize(SyncMode::SYNC_MODE_OFFLINE);
      quit();
    }

    return;
  }

  if (multiplier->hasBeenPressedNow()) {
    if (IS_STORY(SAVEFILE_getGameMode())) {
      playNow(SOUND_MOD);
      SAVEFILE_write8(SRAM->mods.multiplier, multiplier->change());
    } else if (!isCustomOffsetAdjustmentEnabled()) {
      stop();
      engine->transitionIntoScene(new ModsScene(engine, fs),
                                  new PixelTransitionEffect());
    }
  }

  if (isCustomOffsetAdjustmentEnabled() && multiplier->hasBeenReleasedNow()) {
    if (!multiplier->getHandledFlag()) {
      stop();
      engine->transitionIntoScene(new ModsScene(engine, fs),
                                  new PixelTransitionEffect());
    }
    multiplier->setHandledFlag(false);
  }

  if (isCustomOffsetAdjustmentEnabled() && multiplier->getIsPressed())
    return;

  if (settingsMenuInput->hasBeenPressedNow()) {
    stop();
    engine->transitionIntoScene(new SettingsScene(engine, fs),
                                new PixelTransitionEffect());
  }
}

bool SelectionScene::onCustomOffsetChange(ArrowDirection selector, int offset) {
  if (isCustomOffsetAdjustmentEnabled() && multiplier->getIsPressed()) {
    if (arrowSelectors[selector]->hasBeenPressedNow()) {
      arrowSelectors[selector]->setIsPressed(true);
      multiplier->setHandledFlag(true);

      updateCustomOffset(offset);
      unconfirm();
      updateSelection();

      playNow(SOUND_MOD);
    }

    return true;
  }

  return false;
}

bool SelectionScene::onDifficultyLevelChange(ArrowDirection selector,
                                             DifficultyLevel newValue) {
  if (isCrossingPage)
    return true;

  if (arrowSelectors[selector]->hasBeenPressedNow()) {
    unconfirm();
    playNow(SOUND_STEP);

    if (newValue == difficulty->getValue())
      return true;

    SAVEFILE_write8(SRAM->memory.difficultyLevel, newValue);
    difficulty->setValue(newValue);
    pixelBlink->blink();

    u32 lastUnlockedSongIndex = getLastUnlockedSongIndex();
    stop();
    scrollTo(lastUnlockedSongIndex);

    return true;
  }

  return false;
}

bool SelectionScene::onNumericLevelChange(ArrowDirection selector,
                                          u8 newValue) {
  if (isCrossingPage)
    return true;

  if (arrowSelectors[selector]->hasBeenPressedNow()) {
    if (!isMultiplayer() && newValue == getSelectedNumericLevelIndex()) {
      auto arcadeCharts = static_cast<ArcadeChartsOpts>(
          SAVEFILE_read8(SRAM->adminSettings.arcadeCharts));
      bool isDouble = arcadeCharts == ArcadeChartsOpts::DOUBLE;

      if (selector == ArrowDirection::UPRIGHT && !isDouble) {
        SAVEFILE_write8(SRAM->adminSettings.arcadeCharts,
                        ArcadeChartsOpts::DOUBLE);
        stop();
        engine->transitionIntoScene(
            new SelectionScene(engine, fs, InitialLevel::FIRST_LEVEL),
            new PixelTransitionEffect());
        return true;
      } else if (selector == ArrowDirection::UPLEFT && isDouble) {
        SAVEFILE_write8(SRAM->adminSettings.arcadeCharts,
                        ArcadeChartsOpts::SINGLE);
        stop();
        engine->transitionIntoScene(
            new SelectionScene(engine, fs, InitialLevel::LAST_LEVEL),
            new PixelTransitionEffect());
        return true;
      }
    }

    unconfirm();
    playNow(SOUND_STEP);

    syncNumericLevelChanged(newValue);
    setNumericLevel(newValue);

    return true;
  }

  return false;
}

bool SelectionScene::onSelectionChange(ArrowDirection selector,
                                       bool isOnListEdge,
                                       bool isOnPageEdge,
                                       int direction) {
  if (arrowSelectors[selector]->shouldFireEvent()) {
    unconfirm();

    if (isOnListEdge) {
      bool withSound = arrowSelectors[selector]->hasBeenPressedNow();

      if (withSound)
        playNow(SOUND_STEP);

      if (isMultiplayer() && syncer->isMaster())
        syncer->send(SYNC_EVENT_SONG_CORNER, withSound);

      return true;
    }

    stop();

    if (isOnPageEdge)
      setPage(page + direction, direction);
    else {
      selected += direction;
      updateSelection();
      pixelBlink->blink();
    }

    if (isMultiplayer() && syncer->isMaster())
      syncer->send(SYNC_EVENT_SONG_CHANGED, getSelectedSongIndex());

    return true;
  }

  return false;
}

void SelectionScene::onConfirmOrStart(bool isConfirmed) {
  if (isConfirmed)
    goToSong();
  else
    confirm();
}

void SelectionScene::updateSelection(bool isChangingLevel) {
  Song* song = SONG_parse(fs, getSelectedSong());
  selectedSongId = song->id;

  updateLevel(song, isChangingLevel);
  progress->setValue(getSelectedSongIndex() + 1, count);
  setNames(song->title, song->artist);
  Chart* chart = SONG_findChartByNumericLevelIndex(
      song, getSelectedNumericLevelIndex(), isDouble());
  Chart* remoteChart = isVs() && syncer->$remoteNumericLevelIndex != -1
                           ? SONG_findChartByNumericLevelIndex(
                                 song, syncer->$remoteNumericLevelIndex, false)
                           : NULL;
  if (isVs() && syncer->$remoteNumericLevelIndex != -1 &&
      syncer->$remoteNumericLevel == -1) {
    syncer->$remoteLastNumericLevel = remoteChart->level;
    syncer->setRemoteNumericLevel(syncer->$remoteNumericLevelIndex,
                                  remoteChart->level);
  }
  printNumericLevel(chart, remoteChart);
  loadSelectedSongGrade();
  if (!isChangingLevel && initialLevel == InitialLevel::KEEP_LEVEL) {
    syncer->pendingAudio = song->audioPath;
    syncer->pendingSeek = song->sampleStart;
  }

  SONG_free(song);

  SAVEFILE_write8(SRAM->memory.pageIndex, page);
  SAVEFILE_write8(SRAM->memory.songIndex, selected);
  highlighter->select(selected);

  if (initialLevel != InitialLevel::KEEP_LEVEL) {
    syncer->pendingAudio = SOUND_MOD_STR;
    syncer->pendingSeek = 0;
    initialLevel = InitialLevel::KEEP_LEVEL;
    updateLastNumericLevel();
  }
}

void SelectionScene::updateLevel(Song* song, bool isChangingLevel) {
  // (!isChangingLevel = changing song)

  if (!numericLevels.empty())
    numericLevels.clear();

  for (u32 i = 0; i < song->chartCount; i++)
    if (song->charts[i].isDouble == isDouble())
      numericLevels.push_back((song->charts[i].type << 16) |
                              song->charts[i].level);

  if (!isChangingLevel)
    setClosestNumericLevel(getLastNumericLevel());
  if (getSelectedNumericLevelIndex() > numericLevels.size() - 1)
    setClosestNumericLevel(0);

  if (difficulty->getValue() != DifficultyLevel::NUMERIC)
    setClosestNumericLevel(
        SONG_findChartByDifficultyLevel(song, difficulty->getValue())->level);

  if (initialLevel == InitialLevel::FIRST_LEVEL) {
    SAVEFILE_write8(SRAM->memory.numericLevel, 0);
  } else if (initialLevel == InitialLevel::LAST_LEVEL) {
    if (numericLevels.empty()) {
      SAVEFILE_write8(SRAM->memory.numericLevel, 0);
    } else {
      SAVEFILE_write8(SRAM->memory.numericLevel, numericLevels.size() - 1);
    }
  }

  if (isVs()) {
    if (!isChangingLevel) {
      int closest = getClosestLevelIndexTo(syncer->$remoteLastNumericLevel,
                                           syncer->$remoteNumericLevel,
                                           syncer->$remoteNumericLevelIndex);
      if (closest > -1)
        syncer->setRemoteNumericLevel(closest, numericLevels[closest] & 0xff);
    }
  }
}

void SelectionScene::confirm() {
  if (isCrossingPage)
    return;

  if (!IS_STORY(SAVEFILE_getGameMode())) {
    if (numericLevels.empty())
      return;

    numericLevelBadge->get()->moveTo(
        numericLevelBadge->get()->getX(),
        NUMERIC_LEVEL_BADGE_Y + NUMERIC_LEVEL_BADGE_OFFSET_Y);
    if (isVs()) {
      remoteNumericLevelBadge->get()->moveTo(
          remoteNumericLevelBadge->get()->getX(),
          NUMERIC_LEVEL_BADGE_Y + NUMERIC_LEVEL_BADGE_OFFSET_Y);
    }
  }

  playNow(SOUND_STEP);
  confirmed = true;
  arrowSelectors[ArrowDirection::CENTER]->get()->moveTo(CENTER_X, CENTER_Y);
  TextStream::instance().scrollNow(0, TEXT_SCROLL_CONFIRMED);
  TextStream::instance().clear();
  printNumericLevel(NULL, NULL, NUMERIC_LEVEL_BADGE_OFFSET_ROW);
  SCENE_write(CONFIRM_MESSAGE, TEXT_ROW);
}

void SelectionScene::unconfirm() {
  if (confirmed) {
    if (!IS_STORY(SAVEFILE_getGameMode())) {
      numericLevelBadge->get()->moveTo(numericLevelBadge->get()->getX(),
                                       NUMERIC_LEVEL_BADGE_Y);
      if (isVs())
        remoteNumericLevelBadge->get()->moveTo(
            remoteNumericLevelBadge->get()->getX(), NUMERIC_LEVEL_BADGE_Y);
    }

    SPRITE_hide(arrowSelectors[ArrowDirection::CENTER]->get());
    TextStream::instance().scrollNow(0, TEXT_SCROLL_NORMAL);
    updateSelection();
  }
  confirmed = false;
}

void SelectionScene::setPage(u32 page, int direction) {
  this->page = page;

  songs.clear();
  library->loadSongs(songs, getLibraryType(), getPageStart());

  if (direction == 0)
    setUpBackground();
  else
    startPageCross(direction);
}

void SelectionScene::startPageCross(int direction) {
  syncer->pendingAudio = "";
  syncer->pendingSeek = 0;
  this->isCrossingPage = 1;
  this->selected = direction < 0 ? PAGE_SIZE - 1 : 0;
  pixelBlink->blink();
}

void SelectionScene::stopPageCross1() {
  setUpBackground();
  this->isCrossingPage = 2;
}

void SelectionScene::stopPageCross2() {
  updateSelection();
  this->isCrossingPage = false;
}

void SelectionScene::loadChannels() {
  for (u32 i = 0; i < songs.size(); i++) {
    auto channel = SONG_getChannel(fs, SAVEFILE_getGameMode(), songs[i].get(),
                                   getLibraryType());
    channelBadges[i]->setType(channel);
  }

  for (u32 i = songs.size(); i < PAGE_SIZE; i++)
    channelBadges[i]->hide();
}

void SelectionScene::loadProgress() {
  progress->setValue(getSelectedSongIndex() + 1, count);

  for (u32 i = 0; i < PAGE_SIZE; i++) {
    auto songIndex = page * PAGE_SIZE + i;

    gradeBadges[i]->setType(
        IS_STORY(SAVEFILE_getGameMode())
            ? SAVEFILE_getStoryGradeOf(songIndex, difficulty->getValue())
            : GradeType::UNPLAYED);
    locks[i]->setVisible(songIndex > getLastUnlockedSongIndex() &&
                         songIndex <= count - 1);
  }

  EFFECT_clearAffine();
}

void SelectionScene::setNames(std::string title, std::string artist) {
  TextStream::instance().clear();
  SCENE_write(title, TEXT_ROW);
  TextStream::instance().setText("- " + artist + " -", TEXT_ROW + 1,
                                 TEXT_MIDDLE_COL - (artist.length() + 4) / 2);
}

void SelectionScene::printNumericLevel(Chart* chart,
                                       Chart* remoteChart,
                                       s8 offsetY) {
  if (IS_STORY(SAVEFILE_getGameMode()))
    return;

  if (numericLevels.empty()) {
    SCENE_write(isVs() ? combineLevels("--", "--") : "--",
                NUMERIC_LEVEL_ROW + offsetY);
    return;
  }

  if (chart != NULL) {
    if (isCustomOffsetAdjustmentEnabled()) {
      int customOffset = getCustomOffset();
      TextStream::instance().setText(
          (customOffset >= 0 ? "[+" : "[") + std::to_string(customOffset) + "]",
          TEXT_ROW - 1, -3);
      SCENE_write(std::string("  ") + chart->offsetLabel,
                  NUMERIC_LEVEL_ROW + 2);
    } else {
      std::string variant = "";
      if (chart->variant != '\0')
        variant = std::string("  ") + chart->variant;
      if (remoteChart != NULL) {
        std::string remoteVariant = "";
        if (remoteChart->variant != '\0')
          remoteVariant = std::string("  ") + remoteChart->variant;
        variant = combineLevels(variant.empty() ? "   " : variant,
                                remoteVariant.empty() ? "   " : remoteVariant,
                                NUMERIC_LEVEL_BADGE_SEPARATOR_MINUS_ONE_SPACE);
      }
      if (!variant.empty())
        SCENE_write(variant, NUMERIC_LEVEL_ROW + 2 - isVs());
    }
  }

  std::string arcadeLevelString = "";
  std::string remoteArcadeLevelString = "";

  if (chart != NULL)
    arcadeLevelString = chart->getArcadeLevelString();
  if (remoteChart != NULL)
    remoteArcadeLevelString = remoteChart->getArcadeLevelString();

  if (arcadeLevelString.empty())
    arcadeLevelString = formatNumericLevel(getSelectedNumericLevel());
  if (isVs() && syncer->$remoteNumericLevelIndex != -1) {
    if (remoteArcadeLevelString.empty())
      remoteArcadeLevelString = formatNumericLevel(syncer->$remoteNumericLevel);
    arcadeLevelString =
        combineLevels(arcadeLevelString, remoteArcadeLevelString);
  }

  SCENE_write(arcadeLevelString, NUMERIC_LEVEL_ROW + offsetY);
}

std::string SelectionScene::combineLevels(std::string localLevel,
                                          std::string remoteLevel,
                                          std::string separator) {
  return syncer->getLocalPlayerId() == 1 ? remoteLevel + separator + localLevel
                                         : localLevel + separator + remoteLevel;
}

std::string SelectionScene::formatNumericLevel(int numericLevel) {
  std::string levelText =
      numericLevel == 99 ? "??" : std::to_string(numericLevel);
  if (levelText.size() == 1)
    levelText = "0" + levelText;
  return levelText;
}

void SelectionScene::loadSelectedSongGrade() {
  if (IS_STORY(SAVEFILE_getGameMode()))
    return;

  for (u32 i = 0; i < PAGE_SIZE; i++) {
    gradeBadges[i]->setType(
        i == selected ? SAVEFILE_getArcadeGradeOf(
                            selectedSongId, getSelectedNumericLevelIndex())
                      : GradeType::UNPLAYED);
  }

  EFFECT_clearAffine();
}

void SelectionScene::processMultiplayerUpdates() {
  auto remoteId = syncer->getRemotePlayerId();

  while (syncer->isPlaying() && linkUniversal->canRead(remoteId)) {
    u16 message = linkUniversal->read(remoteId);
    u8 event = SYNC_MSG_EVENT(message);
    u16 payload = SYNC_MSG_PAYLOAD(message);

    if (syncer->isMaster() && (isCoop() || event != SYNC_EVENT_LEVEL_CHANGED)) {
      syncer->registerTimeout();
      continue;
    }

    switch (event) {
      case SYNC_EVENT_SONG_CHANGED: {
        if (payload > getLastUnlockedSongIndex() || isCrossingPage) {
          syncer->registerTimeout();
          continue;
        }

        unconfirm();
        scrollTo(payload);
        pixelBlink->blink();

        syncer->clearTimeout();
        break;
      }
      case SYNC_EVENT_LEVEL_CHANGED: {
        unconfirm();
        playNow(SOUND_STEP);

        if (isCoop())
          setNumericLevel(payload);
        else
          setRemoteNumericLevel(payload);

        syncer->clearTimeout();
        break;
      }
      case SYNC_EVENT_SONG_CORNER: {
        unconfirm();
        if (payload)
          playNow(SOUND_STEP);

        syncer->clearTimeout();
        break;
      }
      case SYNC_EVENT_START_SONG: {
        onConfirmOrStart(payload);

        syncer->clearTimeout();
        break;
      }
      default: {
        syncer->registerTimeout();
      }
    }
  }
}

void SelectionScene::syncNumericLevelChanged(u8 newValue) {
  if (!isMultiplayer())
    return;

  syncer->send(SYNC_EVENT_LEVEL_CHANGED, newValue);
}

void SelectionScene::quit() {
  stop();
  engine->transitionIntoScene(SEQUENCE_getMainScene(),
                              new PixelTransitionEffect());
}

SelectionScene::~SelectionScene() {
  songs.clear();
  arrowSelectors.clear();
  channelBadges.clear();
  gradeBadges.clear();
  locks.clear();
}
