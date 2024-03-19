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

  if (!IS_STORY(SAVEFILE_getGameMode()))
    sprites.push_back(numericLevelBadge->get());

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
    numericLevelBadge = std::unique_ptr<Button>{
        new Button(ButtonType::LEVEL_METER, NUMERIC_LEVEL_BADGE_X,
                   NUMERIC_LEVEL_BADGE_Y, false)};
    numericLevelBadge->get()->setPriority(ID_HIGHLIGHTER);

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
  if (engine->isTransitioning() || init < 2)
    return;

  if (SEQUENCE_isMultiplayerSessionDead()) {
    player_stop();
    SEQUENCE_goToMultiplayerGameMode(SAVEFILE_getGameMode());
    return;
  }

  processKeys(keys);

  if (pixelBlink->tick() && isCrossingPage)
    stopPageCross();
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

  blendAlpha = max(min(blendAlpha + (confirmed ? 1 : -1), MAX_OPACITY),
                   HIGHLIGHTER_OPACITY);
  EFFECT_setBlendAlpha(blendAlpha);
}

void SelectionScene::render() {
  if (engine->isTransitioning())
    return;

  if (init == 0) {
    init++;
    return;
  } else if (init == 1) {
    if (isDouble()) {
      SCENE_applyColorFilter(foregroundPalette.get(), ColorFilter::ALIEN);
      VBlankIntrWait();
    }

    BACKGROUND_enable(true, true, true, false);
    SPRITE_enable();
    highlighter->initialize(selected);
    init++;
  }

  if (pendingAudio != "") {
    player_play(pendingAudio.c_str());
    pendingAudio = "";
  }

  if (pendingSeek > 0) {
    player_seek(pendingSeek);
    pendingSeek = 0;
  }
}

void SelectionScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          palette_selectionPal, sizeof(palette_selectionPal))};
}

void SelectionScene::setUpBackground() {
  VBlankIntrWait();
  // ---
  auto backgroundFile = library->getPrefix() + std::to_string(getPageStart());
  auto backgroundPaletteFile = backgroundFile + BACKGROUND_PALETTE_EXTENSION;
  auto backgroundTilesFile = backgroundFile + BACKGROUND_TILES_EXTENSION;
  auto backgroundMapFile = backgroundFile + BACKGROUND_MAP_EXTENSION;

  backgroundPalette =
      BACKGROUND_loadPaletteFile(fs, backgroundPaletteFile.c_str());
  backgroundPalette->persist();
  bg = BACKGROUND_loadBackgroundFiles(fs, backgroundTilesFile.c_str(),
                                      backgroundMapFile.c_str(),
                                      ID_MAIN_BACKGROUND);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
  bg->persist();

  TextStream::instance().setFontColor(TEXT_COLOR);

  loadChannels();
  loadProgress();
  // ---
  VBlankIntrWait();
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
  if (numericLevelIndex == getSelectedNumericLevelIndex())
    return;

  SAVEFILE_write8(SRAM->memory.numericLevel, numericLevelIndex);
  updateSelection(true);
  pixelBlink->blink();
}

void SelectionScene::goToSong() {
  if (numericLevels.empty())
    return;

  player_stop();
  confirmed = false;

  bool isStory = IS_STORY(SAVEFILE_getGameMode());
  bool hasRemoteChart = isVs() && syncer->$remoteNumericLevel != -1;
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
          tempSong, (u8)syncer->$remoteNumericLevel, false));
  }
  SONG_free(tempSong);

  Song* song = SONG_parse(fs, getSelectedSong(), chartIndexes);
  Chart* chart =
      isStory ? SONG_findChartByDifficultyLevel(song, difficulty->getValue())
              : SONG_findChartByNumericLevelIndex(
                    song, getSelectedNumericLevelIndex(), isDouble());
  Chart* remoteChart = hasRemoteChart
                           ? SONG_findChartByNumericLevelIndex(
                                 song, (u8)syncer->$remoteNumericLevel, false)
                           : NULL;

  int customOffset = getCustomOffset();
  chart->customOffset = customOffset;
  chart->levelIndex = getSelectedNumericLevelIndex();
  if (remoteChart != NULL) {
    remoteChart->customOffset = customOffset;
    remoteChart->levelIndex = (u8)syncer->$remoteNumericLevel;
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
      player_stop();
      engine->transitionIntoScene(new ModsScene(engine, fs),
                                  new PixelTransitionEffect());
    }
  }

  if (isCustomOffsetAdjustmentEnabled() && multiplier->hasBeenReleasedNow()) {
    if (!multiplier->getHandledFlag()) {
      player_stop();
      engine->transitionIntoScene(new ModsScene(engine, fs),
                                  new PixelTransitionEffect());
    }
    multiplier->setHandledFlag(false);
  }

  if (isCustomOffsetAdjustmentEnabled() && multiplier->getIsPressed())
    return;

  if (settingsMenuInput->hasBeenPressedNow()) {
    player_stop();
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

      pendingAudio = "";
      pendingSeek = 0;
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
    player_stop();
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
        player_stop();
        engine->transitionIntoScene(
            new SelectionScene(engine, fs, InitialLevel::FIRST_LEVEL),
            new PixelTransitionEffect());
        return true;
      } else if (selector == ArrowDirection::UPLEFT && isDouble) {
        SAVEFILE_write8(SRAM->adminSettings.arcadeCharts,
                        ArcadeChartsOpts::SINGLE);
        player_stop();
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

    player_stop();

    if (isOnPageEdge)
      setPage(page + direction, direction);
    else {
      selected += direction;
      updateSelection();
      pixelBlink->blink();
    }

    if (isMultiplayer() && syncer->isMaster()) {
      syncer->send(SYNC_EVENT_SONG_CHANGED, getSelectedSongIndex());
      syncer->$remoteNumericLevel = -1;
    }

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
  setNames(song->title, song->artist);
  Chart* chart = SONG_findChartByNumericLevelIndex(
      song, getSelectedNumericLevelIndex(), isDouble());
  printNumericLevel(chart);
  loadSelectedSongGrade();
  if (!isChangingLevel && initialLevel == InitialLevel::KEEP_LEVEL) {
    pendingAudio = song->audioPath;
    pendingSeek = song->sampleStart;
  }

  SONG_free(song);

  SAVEFILE_write8(SRAM->memory.pageIndex, page);
  SAVEFILE_write8(SRAM->memory.songIndex, selected);
  highlighter->select(selected);

  if (initialLevel != InitialLevel::KEEP_LEVEL) {
    pendingAudio = SOUND_MOD_STR;
    initialLevel = InitialLevel::KEEP_LEVEL;
  }
}

void SelectionScene::updateLevel(Song* song, bool isChangingLevel) {
  bool canUpdateLevel = false;
  u8 currentLevel = 0;

  if (!numericLevels.empty()) {
    canUpdateLevel = true;
    currentLevel = getSelectedNumericLevel();
    numericLevels.clear();
  }

  for (u32 i = 0; i < song->chartCount; i++)
    if (song->charts[i].isDouble == isDouble())
      numericLevels.push_back(song->charts[i].level);

  if (canUpdateLevel && !isChangingLevel)
    setClosestNumericLevel(currentLevel);
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
}

void SelectionScene::confirm() {
  if (isCrossingPage)
    return;

  if (!IS_STORY(SAVEFILE_getGameMode())) {
    if (numericLevels.empty())
      return;

    numericLevelBadge->get()->moveTo(
        NUMERIC_LEVEL_BADGE_X,
        NUMERIC_LEVEL_BADGE_Y + NUMERIC_LEVEL_BADGE_OFFSET_Y);
  }

  playNow(SOUND_STEP);
  confirmed = true;
  arrowSelectors[ArrowDirection::CENTER]->get()->moveTo(CENTER_X, CENTER_Y);
  TextStream::instance().scrollNow(0, TEXT_SCROLL_CONFIRMED);
  TextStream::instance().clear();
  printNumericLevel(NULL, NUMERIC_LEVEL_BADGE_OFFSET_ROW);
  SCENE_write(CONFIRM_MESSAGE, TEXT_ROW);
}

void SelectionScene::unconfirm() {
  if (confirmed) {
    if (!IS_STORY(SAVEFILE_getGameMode()))
      numericLevelBadge->get()->moveTo(NUMERIC_LEVEL_BADGE_X,
                                       NUMERIC_LEVEL_BADGE_Y);

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
  this->isCrossingPage = true;
  this->selected = direction < 0 ? PAGE_SIZE - 1 : 0;
  highlighter->select(selected);
  pixelBlink->blink();
}

void SelectionScene::stopPageCross() {
  setUpBackground();
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
  progress->setValue(getCompletedSongs(), count);

  for (u32 i = 0; i < PAGE_SIZE; i++) {
    auto songIndex = page * PAGE_SIZE + i;

    gradeBadges[i]->setType(
        IS_STORY(SAVEFILE_getGameMode())
            ? SAVEFILE_getStoryGradeOf(songIndex, difficulty->getValue())
            : GradeType::UNPLAYED);
    locks[i]->setVisible(songIndex > getLastUnlockedSongIndex() &&
                         songIndex <= count - 1);
  }
}

void SelectionScene::setNames(std::string title, std::string artist) {
  TextStream::instance().clear();
  SCENE_write(title, TEXT_ROW);
  TextStream::instance().setText("- " + artist + " -", TEXT_ROW + 1,
                                 TEXT_MIDDLE_COL - (artist.length() + 4) / 2);
}

void SelectionScene::printNumericLevel(Chart* chart, s8 offsetY) {
  if (IS_STORY(SAVEFILE_getGameMode()))
    return;

  if (numericLevels.empty()) {
    SCENE_write("--", NUMERIC_LEVEL_ROW + offsetY);
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
      if (chart->variant != '\0')
        SCENE_write(std::string("  ") + chart->variant, NUMERIC_LEVEL_ROW + 2);
    }

    if (chart->difficulty == DifficultyLevel::NORMAL)
      return SCENE_write("NM", NUMERIC_LEVEL_ROW + offsetY);

    if (chart->difficulty == DifficultyLevel::HARD)
      return SCENE_write("HD", NUMERIC_LEVEL_ROW + offsetY);

    if (chart->difficulty == DifficultyLevel::CRAZY)
      return SCENE_write("CZ", NUMERIC_LEVEL_ROW + offsetY);

    if (chart->type == ChartType::DOUBLE_COOP_CHART)
      return SCENE_write(";)", NUMERIC_LEVEL_ROW + offsetY);
  }

  auto numericLevel = getSelectedNumericLevel();
  auto levelText = numericLevel == 99 ? "??" : std::to_string(numericLevel);
  if (levelText.size() == 1)
    levelText = "0" + levelText;
  SCENE_write(levelText, NUMERIC_LEVEL_ROW + offsetY);
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
        if (syncer->$remoteNumericLevel != -1)
          setNumericLevel(syncer->$remoteNumericLevel);
        scrollTo(payload);
        syncer->$remoteNumericLevel = -1;
        pixelBlink->blink();

        syncer->clearTimeout();
        break;
      }
      case SYNC_EVENT_LEVEL_CHANGED: {
        unconfirm();
        playNow(SOUND_STEP);

        if (syncer->isMaster()) {
          setNumericLevel(getSelectedNumericLevelIndex());
          syncer->$remoteNumericLevel = payload;
        } else {
          setNumericLevel(payload);
          syncer->$remoteNumericLevel = payload;
        }

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
  if (syncer->isMaster())
    syncer->$remoteNumericLevel = -1;
  else if (syncer->$remoteNumericLevel == -1)
    syncer->$remoteNumericLevel = getSelectedNumericLevelIndex();
}

void SelectionScene::quit() {
  player_stop();
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
