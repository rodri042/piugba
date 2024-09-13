#include "CalibrateScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

#include "SettingsScene.h"
#include "assets.h"
#include "data/content/_compiled_sprites/palette_selection.h"
#include "gameplay/Key.h"
#include "gameplay/save/SaveFile.h"
#include "player/PlaybackState.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "player/player.h"
}

#define TITLE "AUDIO LAG CALIBRATION"
#define SUBTITLE1 "1) Press to start"
#define SUBTITLE2 "2) Wait 4 beats"
#define SUBTITLE3 "3) Press on beat 5"
#define MEASURE_TITLE "- Detected audio lag -"
#define CANCEL_TEXT "BACK"
#define RESET_TEXT "RESET"
#define SAVE_TEXT "SAVE"

const u32 BPM = 120;
const u32 BEATS_TOTAL = 16;
const u32 TARGET_BEAT_MS = 2000;

const u32 ID_MAIN_BACKGROUND = 1;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 16;
const u32 TEXT_COLOR = 0x7FFF;
const u32 TEXT_ROW_TITLE = 3;
const u32 TEXT_ROW_SUBTITLE1 = 5;
const u32 TEXT_ROW_SUBTITLE2 = 6;
const u32 TEXT_ROW_SUBTITLE3 = 7;
const u32 TEXT_ROW_MEASURE_TITLE = 12;
const u32 TEXT_ROW_MEASURE_VALUE = 13;
const u32 TEXT_ROW_BUTTONS = 17;
const u32 TEXT_COL_SUBTITLE = 3;
const u32 TEXT_COL_RESET = 1;
const u32 TEXT_COL_SAVE = 19;
const u32 CALIBRATE_BUTTON_X = 112;
const u32 CALIBRATE_BUTTON_Y = 72;
const u32 BUTTON_MARGIN = 12;
const u32 PIXEL_BLINK_LEVEL = 6;

CalibrateScene::CalibrateScene(std::shared_ptr<GBAEngine> engine,
                               const GBFS_FILE* fs,
                               std::function<void()> onFinish)
    : Scene(engine) {
  this->fs = fs;
  this->onFinish = onFinish;
}

std::vector<Background*> CalibrateScene::backgrounds() {
  return {bg.get()};
}

std::vector<Sprite*> CalibrateScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(calibrateButton->get());
  sprites.push_back(resetButton->get());
  sprites.push_back(saveButton->get());

  return sprites;
}

void CalibrateScene::load() {
  SCENE_init();

  setUpSpritesPalette();
  setUpBackground();

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};

  calibrateButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::CENTER, false, true)};
  resetButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::UPLEFT, true, true)};
  saveButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::UPRIGHT, true, true)};

  calibrateButton->get()->moveTo(CALIBRATE_BUTTON_X, CALIBRATE_BUTTON_Y);
  resetButton->get()->moveTo(BUTTON_MARGIN,
                             GBA_SCREEN_HEIGHT - ARROW_SIZE - BUTTON_MARGIN);
  SPRITE_hide(saveButton->get());

  printTitle();
  TextStream::instance().setText(CANCEL_TEXT, TEXT_ROW_BUTTONS, TEXT_COL_RESET);
}

void CalibrateScene::tick(u16 keys) {
  if (engine->isTransitioning() || !hasStarted)
    return;

  processKeys(keys);

  if (calibrateButton->hasBeenPressedNow())
    calibrate();

  if (resetButton->hasBeenPressedNow() &&
      !SPRITE_isHidden(resetButton->get())) {
    if (hasDoneChanges) {
      measuredLag = 0;
      printTitle();
      finish();
    } else
      goBack();
  }

  if (saveButton->hasBeenPressedNow() && !SPRITE_isHidden(saveButton->get()))
    save();

  pixelBlink->tick();
  calibrateButton->tick();
  resetButton->tick();
  saveButton->tick();

  if (isMeasuring && PlaybackState.hasFinished)
    finish();
}

void CalibrateScene::render() {
  if (engine->isTransitioning())
    return;

  if (!hasStarted) {
    BACKGROUND_enable(true, true, false, false);
    SPRITE_enable();
    hasStarted = true;
  }
}

void CalibrateScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          palette_selectionPal, sizeof(palette_selectionPal))};
}

void CalibrateScene::setUpBackground() {
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_LINES_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_LINES_TILES, BG_LINES_MAP,
                                      ID_MAIN_BACKGROUND);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void CalibrateScene::processKeys(u16 keys) {
  calibrateButton->setIsPressed(KEY_CONFIRM(keys));
  resetButton->setIsPressed(KEY_PREV(keys));
  saveButton->setIsPressed(KEY_NEXT(keys));
}

void CalibrateScene::printTitle() {
  TextStream::instance().setFontColor(TEXT_COLOR);
  TextStream::instance().setFontSubcolor(text_bg_palette_default_subcolor);
  TextStream::instance().clear();

  SCENE_write(TITLE, TEXT_ROW_TITLE);
  TextStream::instance().setText(SUBTITLE1, TEXT_ROW_SUBTITLE1,
                                 TEXT_COL_SUBTITLE);
  TextStream::instance().setText(SUBTITLE2, TEXT_ROW_SUBTITLE2,
                                 TEXT_COL_SUBTITLE);
  TextStream::instance().setText(SUBTITLE3, TEXT_ROW_SUBTITLE3,
                                 TEXT_COL_SUBTITLE);
}

void CalibrateScene::calibrate() {
  pixelBlink->blink();

  if (!isMeasuring) {
    start();
    return;
  }

  u32 msecs = PlaybackState.msecs;
  measuredLag = msecs - TARGET_BEAT_MS;
  finish();
}

void CalibrateScene::start() {
  isMeasuring = true;
  hasDoneChanges = true;
  SPRITE_hide(resetButton->get());
  SPRITE_hide(saveButton->get());
  printTitle();
  player_playSfx(SOUND_CALIBRATE);
}

void CalibrateScene::finish() {
  isMeasuring = false;

  resetButton->get()->moveTo(BUTTON_MARGIN,
                             GBA_SCREEN_HEIGHT - ARROW_SIZE - BUTTON_MARGIN);
  saveButton->get()->moveTo(GBA_SCREEN_WIDTH - ARROW_SIZE - BUTTON_MARGIN,
                            GBA_SCREEN_HEIGHT - ARROW_SIZE - BUTTON_MARGIN);

  TextStream::instance().setText(RESET_TEXT, TEXT_ROW_BUTTONS, TEXT_COL_RESET);
  TextStream::instance().setText(SAVE_TEXT, TEXT_ROW_BUTTONS, TEXT_COL_SAVE);

  SCENE_write(MEASURE_TITLE, TEXT_ROW_MEASURE_TITLE);

  auto value = std::to_string(measuredLag);
  SCENE_write(value, TEXT_ROW_MEASURE_VALUE);
}

void CalibrateScene::save() {
  SAVEFILE_write32(SRAM->settings.audioLag, (u32)measuredLag);
  player_stop();
  goBack();
}

void CalibrateScene::goBack() {
  if (onFinish != NULL)
    onFinish();
  else {
    player_stop();
    engine->transitionIntoScene(new SettingsScene(engine, fs),
                                new PixelTransitionEffect());
  }
}
