#include "MenuScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

#include "data/content/_compiled_sprites/palette_selection.h"
#include "gameplay/Key.h"
#include "player/PlaybackState.h"
#include "scenes/SelectionScene.h"
#include "utils/SceneUtils.h"
#include "utils/StringUtils.h"

extern "C" {
#include "player/player.h"
}

const u32 ID_MAIN_BACKGROUND = 1;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 16;
const u32 TEXT_COLOR = 0x7FFF;
const int TEXT_COL_UNSELECTED = -2;
const int TEXT_COL_SELECTED = -3;
const int TEXT_COL_VALUE_MIDDLE = 20;
const u32 BUTTON_MARGIN = 3;
const u32 PIXEL_BLINK_LEVEL = 4;
const u32 SELECT_BUTTON_X = 112;

MenuScene::MenuScene(std::shared_ptr<GBAEngine> engine, const GBFS_FILE* fs)
    : Scene(engine) {
  this->fs = fs;
}

std::vector<Background*> MenuScene::backgrounds() {
  return {bg.get()};
}

std::vector<Sprite*> MenuScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(selectButton->get());
  sprites.push_back(backButton->get());
  sprites.push_back(nextButton->get());
  sprites.push_back(changeLeftButton->get());
  sprites.push_back(changeRightButton->get());

  return sprites;
}

void MenuScene::load() {
  SCENE_init();

  setUpSpritesPalette();
  setUpBackground();

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};

  selectButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::CENTER, false, true, true, true)};
  backButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::DOWNLEFT, true, true, true, true)};
  nextButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::DOWNRIGHT, true, true, true, true)};
  changeLeftButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::UPLEFT, true, true, true, true)};
  changeRightButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::UPRIGHT, true, true, true, true)};
  closeInput = std::unique_ptr<InputHandler>{new InputHandler()};

  selectButton->get()->moveTo(SELECT_BUTTON_X,
                              GBA_SCREEN_HEIGHT - ARROW_SIZE - BUTTON_MARGIN);
  backButton->get()->moveTo(BUTTON_MARGIN,
                            GBA_SCREEN_HEIGHT - ARROW_SIZE - BUTTON_MARGIN);
  nextButton->get()->moveTo(GBA_SCREEN_WIDTH - ARROW_SIZE - BUTTON_MARGIN,
                            GBA_SCREEN_HEIGHT - ARROW_SIZE - BUTTON_MARGIN);
  changeLeftButton->get()->moveTo(BUTTON_MARGIN, BUTTON_MARGIN);
  changeRightButton->get()->moveTo(
      GBA_SCREEN_WIDTH - ARROW_SIZE - BUTTON_MARGIN, BUTTON_MARGIN);

  printMenu();
}

void MenuScene::tick(u16 keys) {
  if (engine->isTransitioning() || !hasStarted)
    return;

  processKeys(keys);
  processSelection();

  pixelBlink->tick();
  selectButton->tick();
  backButton->tick();
  nextButton->tick();
  changeLeftButton->tick();
  changeRightButton->tick();
}

void MenuScene::render() {
  if (engine->isTransitioning())
    return;

  if (!hasStarted) {
    BACKGROUND_enable(true, true, false, false);
    SPRITE_enable();
    hasStarted = true;
  }
}

void MenuScene::printOption(u32 id,
                            std::string name,
                            std::string value,
                            u32 row,
                            bool highlightChange,
                            std::string defaultValue) {
  bool isActive = selected == id;
  TextStream::instance().setText(
      std::string(isActive ? ">" : "") +
          (highlightChange && value != defaultValue ? "* " : "") + name,
      row, isActive ? TEXT_COL_SELECTED : TEXT_COL_UNSELECTED);

  if (value.length() > 0) {
    auto valueString = "<" + value + ">";
    TextStream::instance().setText(
        valueString, row, TEXT_COL_VALUE_MIDDLE - valueString.length() / 2);
  }
}

u8 MenuScene::change(u8 value, u8 optionsCount, int direction) {
  return direction >= 0 ? increment(value, optionsCount)
                        : decrement(value, optionsCount);
}

u8 MenuScene::increment(u8 value, u8 optionsCount) {
  return value >= optionsCount - 1 ? 0 : value + 1;
}

u8 MenuScene::decrement(u8 value, u8 optionsCount) {
  return value == 0 ? optionsCount - 1 : value - 1;
}

void MenuScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          palette_selectionPal, sizeof(palette_selectionPal))};
}

void MenuScene::setUpBackground() {
  loadBackground(ID_MAIN_BACKGROUND);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void MenuScene::processKeys(u16 keys) {
  if (SAVEFILE_isUsingGBAStyle() != isUsingGBAStyle)
    blockButtons = true;

  if (blockButtons && !(keys & KEY_ANY))
    blockButtons = false;

  isUsingGBAStyle = SAVEFILE_isUsingGBAStyle();
  selectButton->setIsPressed(!blockButtons && KEY_CONFIRM(keys));
  backButton->setIsPressed(!blockButtons && KEY_GOUP(keys));
  nextButton->setIsPressed(!blockButtons && KEY_GODOWN(keys));
  changeLeftButton->setIsPressed(!blockButtons && KEY_PREVLEFT(keys));
  changeRightButton->setIsPressed(!blockButtons && KEY_NEXTRIGHT(keys));
  closeInput->setIsPressed(!blockButtons && KEY_STASEL(keys));

  if (closeInput->hasBeenPressedNow())
    close();
}

void MenuScene::processSelection() {
  if (selectButton->hasBeenPressedNow())
    select(0);

  if (engine->isTransitioning())
    return;

  if (changeRightButton->hasBeenPressedNow())
    select(1);

  if (changeLeftButton->hasBeenPressedNow())
    select(-1);

  if (nextButton->hasBeenPressedNow())
    move(1);

  if (backButton->hasBeenPressedNow())
    move(-1);
}

void MenuScene::printMenu() {
  TextStream::instance().setFontColor(TEXT_COLOR);
  TextStream::instance().setFontSubcolor(text_bg_palette_default_subcolor);
  TextStream::instance().clear();

  printOptions();
}

void MenuScene::move(int direction) {
  u32 maxOption = getOptionCount() - 1;
  if (selected == maxOption && direction > 0)
    selected = 0;
  else if (selected == 0 && direction < 0)
    selected = maxOption;
  else
    selected += direction;

  pixelBlink->blink();
  printMenu();
  player_playSfx(SOUND_STEP);
}

void MenuScene::select(int direction) {
  pixelBlink->blink();

  if (selectOption(selected, direction)) {
    printMenu();
    player_playSfx(SOUND_STEP);
  } else
    player_stop();
}

void MenuScene::close() {
  player_stop();
  engine->transitionIntoScene(new SelectionScene(engine, fs),
                              new PixelTransitionEffect());
}
