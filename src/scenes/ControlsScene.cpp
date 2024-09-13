#include "ControlsScene.h"

#include "assets.h"
#include "data/content/_compiled_sprites/palette_controls.h"
#include "gameplay/Key.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "player/player.h"
}

const u32 ID_MAIN_BACKGROUND = 1;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 16;
const u32 RIGHT_CENTER = 5;
const u32 INSTRUCTOR_X = 86;
const u32 INSTRUCTOR_Y = 49;
const u32 START_COMBO_TOTAL = 5;
const u32 BOUNCE_STEPS[] = {0, 1, 2, 4, 5, 8, 7, 5, 3, 0};
const u32 BOUNCE_STEPS_TOTAL = 10;
const int BOUNCE_DIRX[] = {-1, -1, 0, 1, 1};
const int BOUNCE_DIRY[] = {1, -1, 0, -1, 1};
const ArrowDirection START_COMBO[] = {
    ArrowDirection::UPLEFT, ArrowDirection::UPRIGHT, ArrowDirection::DOWNLEFT,
    ArrowDirection::DOWNRIGHT, ArrowDirection::CENTER};
const u32 PIXEL_BLINK_LEVEL = 4;

ControlsScene::ControlsScene(std::shared_ptr<GBAEngine> engine,
                             const GBFS_FILE* fs,
                             std::function<void()> onFinish)
    : Scene(engine) {
  this->fs = fs;
  this->onFinish = onFinish;
}

std::vector<Background*> ControlsScene::backgrounds() {
  return {bg.get()};
}

std::vector<Sprite*> ControlsScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(instructor->get());
  for (auto& it : buttons)
    sprites.push_back(it->get());

  for (u32 i = 0; i < START_COMBO_TOTAL; i++)
    sprites.push_back(comboArrows[i]->get());

  return sprites;
}

void ControlsScene::load() {
  SCENE_init();

  setUpSpritesPalette();
  setUpBackground();

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};

  setUpArrows();
  instructor = std::unique_ptr<Instructor>{
      new Instructor(InstructorType::Girl, INSTRUCTOR_X, INSTRUCTOR_Y)};
}

void ControlsScene::tick(u16 keys) {
  if (engine->isTransitioning() || !hasStarted)
    return;

  __qran_seed += (1 + keys) * REG_VCOUNT;
  processKeys(keys);
  processCombo();

  pixelBlink->tick();
  for (u32 i = 0; i < ARROWS_TOTAL; i++)
    buttons[i]->tick();
  buttons[RIGHT_CENTER]->tick();
  for (u32 i = 0; i < START_COMBO_TOTAL; i++)
    comboArrows[i]->tick();

  bounceStep = max(bounceStep - 1, 0);
  instructor->get()->moveTo(
      INSTRUCTOR_X + BOUNCE_STEPS[bounceStep] * bounceDirectionX,
      INSTRUCTOR_Y + BOUNCE_STEPS[bounceStep] * bounceDirectionY);
}

void ControlsScene::render() {
  if (engine->isTransitioning())
    return;

  if (!hasStarted) {
    BACKGROUND_enable(true, true, false, false);
    SPRITE_enable();
    player_playSfx(SOUND_ENTER);
    hasStarted = true;
  }
}

void ControlsScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          palette_controlsPal, sizeof(palette_controlsPal))};
}

void ControlsScene::setUpBackground() {
  if (SAVEFILE_isUsingModernTheme()) {
    backgroundPalette =
        BACKGROUND_loadPaletteFile(fs, BG_CONTROLS_MDRN_PALETTE);
    bg = BACKGROUND_loadBackgroundFiles(
        fs, BG_CONTROLS_MDRN_TILES, BG_CONTROLS_MDRN_MAP, ID_MAIN_BACKGROUND);
  } else {
    backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_CONTROLS_PALETTE);
    bg = BACKGROUND_loadBackgroundFiles(fs, BG_CONTROLS_TILES, BG_CONTROLS_MAP,
                                        ID_MAIN_BACKGROUND);
  }

  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void ControlsScene::setUpArrows() {
  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    auto direction = static_cast<ArrowDirection>(i);
    buttons.push_back(std::unique_ptr<ArrowSelector>{
        new ArrowSelector(direction, direction > 0, true, false)});
  }

  buttons.push_back(std::unique_ptr<ArrowSelector>{new ArrowSelector(
      static_cast<ArrowDirection>(ArrowDirection::CENTER), true, true, false)});

  buttons[ArrowDirection::DOWNLEFT]->get()->moveTo(22, 67);
  buttons[ArrowDirection::UPLEFT]->get()->moveTo(29, 25);
  buttons[ArrowDirection::CENTER]->get()->moveTo(39, 68);
  buttons[ArrowDirection::UPRIGHT]->get()->moveTo(187, 25);
  buttons[ArrowDirection::DOWNRIGHT]->get()->moveTo(199, 65);
  buttons[RIGHT_CENTER]->get()->moveTo(183, 74);

  for (u32 i = 0; i < START_COMBO_TOTAL; i++)
    comboArrows.push_back(
        std::unique_ptr<ArrowTutorial>{new ArrowTutorial(START_COMBO[i])});

  comboArrows[0]->get()->moveTo(76, 142);
  comboArrows[1]->get()->moveTo(94, 142);
  comboArrows[2]->get()->moveTo(112, 142);
  comboArrows[3]->get()->moveTo(130, 142);
  comboArrows[4]->get()->moveTo(148, 142);
}

void ControlsScene::processKeys(u16 keys) {
  buttons[ArrowDirection::DOWNLEFT]->setIsPressed(KEY_DOWNLEFT(keys));
  buttons[ArrowDirection::UPLEFT]->setIsPressed(KEY_UPLEFT(keys));
  buttons[ArrowDirection::CENTER]->setIsPressed(KEY_CENTER(keys));
  buttons[ArrowDirection::UPRIGHT]->setIsPressed(KEY_UPRIGHT(keys));
  buttons[ArrowDirection::DOWNRIGHT]->setIsPressed(KEY_DOWNRIGHT(keys));
  buttons[RIGHT_CENTER]->setIsPressed(KEY_CENTER(keys));
}

void ControlsScene::processCombo() {
  for (u32 i = 0; i < START_COMBO_TOTAL; i++) {
    auto direction = static_cast<ArrowDirection>(i);

    if (buttons[direction]->hasBeenPressedNow()) {
      bounceDirectionX = BOUNCE_DIRX[direction];
      bounceDirectionY = BOUNCE_DIRY[direction];
      bounceStep = BOUNCE_STEPS_TOTAL - 1;
      if (START_COMBO[comboStep] == direction)
        advanceCombo();
      else
        resetCombo();
    }
  }
}

void ControlsScene::advanceCombo() {
  player_playSfx(SOUND_STEP);
  comboArrows[comboStep]->on();
  comboStep++;

  if (comboStep == START_COMBO_TOTAL) {
    player_stop();
    onFinish();
  }
}

void ControlsScene::resetCombo() {
  if (comboStep == 0)
    return;

  player_playSfx(SOUND_MOD);
  pixelBlink->blink();

  comboStep = 0;
  for (u32 i = 0; i < START_COMBO_TOTAL; i++)
    comboArrows[i]->off();
}

ControlsScene::~ControlsScene() {
  buttons.clear();
  comboArrows.clear();
}
