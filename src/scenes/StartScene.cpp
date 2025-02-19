#include "StartScene.h"

#include <libgba-sprite-engine/background/text_stream.h>
#include <libgba-sprite-engine/gba/tonc_bios.h>
#include <libgba-sprite-engine/gba/tonc_math.h>

#include <string>

#include "assets.h"
#include "data/content/_compiled_sprites/palette_start.h"
#include "gameplay/Key.h"
#include "gameplay/Sequence.h"
#include "gameplay/TimingProvider.h"
#include "gameplay/save/SaveFile.h"
#include "player/PlaybackState.h"
#include "scenes/AdminScene.h"
#include "scenes/StatsScene.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "player/player.h"
}

const std::string TITLES[] = {
    "Play",   "Campaign",   "Progress",   "Arcade",     "Multi VS",
    "Single", "Multi COOP", "Challenges", "Impossible", "DeathMix"};

#define BUTTON_PLAY 0
#define SUBBUTTON_CAMPAIGN 1
#define SUBBUTTON_STATS 2
#define BUTTON_ARCADE 3
#define SUBBUTTON_MULTI_VS 4
#define SUBBUTTON_SINGLE 5
#define SUBBUTTON_MULTI_COOP 6
#define BUTTON_CHALLENGES 7
#define SUBBUTTON_IMPOSSIBLE 8
#define SUBBUTTON_DEATHMIX 9

const u32 BPM = 145;
const u32 ARROW_POOL_SIZE = 10;
const u32 DEMO_ARROW_INITIAL_Y = 78;
const u32 BUTTONS_TOTAL = 10;

const u32 ID_DARKENER = 1;
const u32 ID_MAIN_BACKGROUND = 2;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 20;
const u32 DARKENER_COLOR_INDEX = 253;
const u32 TEXT_COLOR = 0x6DEF;
const u32 TEXT_ROW = 12;
const int TEXT_OFFSET_Y = -4;
const u32 PIXEL_BLINK_LEVEL = 4;
const u32 ALPHA_BLINK_LEVEL = 8;
const u32 BUTTONS_X[] = {141, 141, 154, 175, 170, 183, 196, 209, 209, 222};
const u32 BUTTONS_Y[] = {128, 136, 136, 128, 136, 136, 136, 128, 136, 136};
const u32 BOUNCE_STEPS[] = {0, 6, 10, 13, 11, 9, 6, 3, 1, 0};
const u32 INPUT_LEFT = 0;
const u32 INPUT_RIGHT = 1;
const u32 INPUT_SELECT = 2;
const u32 INPUT_SELECT_ALT = 3;
const u32 GAME_X = 72;
const u32 GAME_Y = 13;

StartScene::StartScene(std::shared_ptr<GBAEngine> engine, const GBFS_FILE* fs)
    : Scene(engine) {
  this->fs = fs;

  darkener = std::unique_ptr<Darkener>{new Darkener(ID_DARKENER, ID_DARKENER)};
  darkenerOpacity = ALPHA_BLINK_LEVEL;
}

std::vector<Background*> StartScene::backgrounds() {
  return {bg.get()};
}

std::vector<Sprite*> StartScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(buttons[BUTTON_PLAY]->get());
  sprites.push_back(buttons[BUTTON_ARCADE]->get());
  sprites.push_back(buttons[BUTTON_CHALLENGES]->get());
  sprites.push_back(buttons[SUBBUTTON_CAMPAIGN]->get());
  sprites.push_back(buttons[SUBBUTTON_STATS]->get());
  sprites.push_back(buttons[SUBBUTTON_MULTI_VS]->get());
  sprites.push_back(buttons[SUBBUTTON_SINGLE]->get());
  sprites.push_back(buttons[SUBBUTTON_MULTI_COOP]->get());
  sprites.push_back(buttons[SUBBUTTON_IMPOSSIBLE]->get());
  sprites.push_back(buttons[SUBBUTTON_DEATHMIX]->get());

  sprites.push_back(inputs[INPUT_LEFT]->get());
  sprites.push_back(inputs[INPUT_RIGHT]->get());
  sprites.push_back(inputs[INPUT_SELECT]->get());
  sprites.push_back(inputs[INPUT_SELECT_ALT]->get());

  arrowPool->forEach([&sprites](Arrow* it) {
    it->index = sprites.size();
    sprites.push_back(it->get());
  });

  for (auto& it : arrowHolders)
    sprites.push_back(it->get());

  return sprites;
}

void StartScene::load() {
  SAVEFILE_write8(SRAM->state.isPlaying, false);
  SCENE_init();

  setUpSpritesPalette();
  setUpBackground();

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};

  setUpInputs();
  setUpButtons();
  setUpGameAnimation();

  TextStream::instance().scrollNow(0, TEXT_OFFSET_Y);
}

void StartScene::tick(u16 keys) {
  if (engine->isTransitioning() || init < 3)
    return;

  __qran_seed += (1 + keys) * REG_VCOUNT;
  processKeys(keys);

  animateBpm();
  int bounceOffset = bounceDirection * BOUNCE_STEPS[darkenerOpacity] *
                     !!didWinImpossibleMode();
  darkenerOpacity = min(darkenerOpacity + 1, ALPHA_BLINK_LEVEL);
  EFFECT_setBlendAlpha(darkenerOpacity);

  animateInputs(bounceOffset);
  for (auto& it : inputs)
    it->tick();
  animateArrows(bounceOffset);

  processSelectionChange();
  navigateToAdminMenuIfNeeded(keys);

  pixelBlink->tick();
}

void StartScene::render() {
  if (engine->isTransitioning())
    return;

  if (init == 0) {
    init++;
    return;
  } else if (init == 1) {
    darkener->initialize(BackgroundType::FULL_BGA_DARK, DARKENER_COLOR_INDEX);
    init++;
    return;
  } else if (init == 2) {
    printTitle();
    BACKGROUND_enable(true, true, true, false);
    SPRITE_enable();
    player_playSfx(SOUND_LOOP);
    player_enableLoop();
    init++;
  }
}

void StartScene::setUpSpritesPalette() {
  foregroundPalette = std::unique_ptr<ForegroundPaletteManager>{
      new ForegroundPaletteManager(palette_startPal, sizeof(palette_startPal))};
}

void StartScene::setUpBackground() {
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_START_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_START_TILES, BG_START_MAP,
                                      ID_MAIN_BACKGROUND);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void StartScene::setUpInputs() {
  inputs.push_back(std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::DOWNLEFT, false, true)});
  inputs.push_back(std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::DOWNRIGHT, true, true)});
  inputs.push_back(std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::CENTER, true, true)});
  inputs.push_back(std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::CENTER, true, true)});
  inputs.push_back(std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::CENTER, true, true)});

  animateInputs(0);
}

void StartScene::setUpButtons() {
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::BLUE, BUTTONS_X[BUTTON_PLAY],
                 BUTTONS_Y[BUTTON_PLAY], false)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::SUB_BUTTON_BLUE, BUTTONS_X[SUBBUTTON_CAMPAIGN],
                 BUTTONS_Y[SUBBUTTON_CAMPAIGN], false)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::SUB_BUTTON_BLUE, BUTTONS_X[SUBBUTTON_STATS],
                 BUTTONS_Y[SUBBUTTON_STATS], false)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::GRAY, BUTTONS_X[BUTTON_ARCADE],
                 BUTTONS_Y[BUTTON_ARCADE], true)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::SUB_BUTTON_GRAY, BUTTONS_X[SUBBUTTON_MULTI_VS],
                 BUTTONS_Y[SUBBUTTON_MULTI_VS], false)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::SUB_BUTTON_GRAY, BUTTONS_X[SUBBUTTON_SINGLE],
                 BUTTONS_Y[SUBBUTTON_SINGLE], true)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::SUB_BUTTON_GRAY, BUTTONS_X[SUBBUTTON_MULTI_COOP],
                 BUTTONS_Y[SUBBUTTON_MULTI_COOP], true)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::ORANGE, BUTTONS_X[BUTTON_CHALLENGES],
                 BUTTONS_Y[BUTTON_CHALLENGES], true)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::SUB_BUTTON_ORANGE, BUTTONS_X[SUBBUTTON_IMPOSSIBLE],
                 BUTTONS_Y[SUBBUTTON_IMPOSSIBLE], true)});
  buttons.push_back(std::unique_ptr<Button>{
      new Button(ButtonType::SUB_BUTTON_ORANGE, BUTTONS_X[SUBBUTTON_DEATHMIX],
                 BUTTONS_Y[SUBBUTTON_DEATHMIX], true)});

  buttons[SUBBUTTON_CAMPAIGN]->hide();
  buttons[SUBBUTTON_STATS]->hide();
  buttons[SUBBUTTON_MULTI_VS]->hide();
  buttons[SUBBUTTON_SINGLE]->hide();
  buttons[SUBBUTTON_MULTI_COOP]->hide();
  buttons[SUBBUTTON_IMPOSSIBLE]->hide();
  buttons[SUBBUTTON_DEATHMIX]->hide();

  if (ENV_ARCADE) {
    isPlayExpanded = true;
    isArcadeExpanded = true;
    isChallengesExpanded = true;
    buttons[BUTTON_PLAY]->hide();
    buttons[SUBBUTTON_CAMPAIGN]->hide();
    buttons[SUBBUTTON_STATS]->show();
    buttons[BUTTON_ARCADE]->hide();
    buttons[SUBBUTTON_MULTI_VS]->show();
    buttons[SUBBUTTON_SINGLE]->show();
    buttons[SUBBUTTON_MULTI_COOP]->show();
    buttons[BUTTON_CHALLENGES]->hide();
    buttons[SUBBUTTON_IMPOSSIBLE]->hide();
    buttons[SUBBUTTON_DEATHMIX]->show();
    buttons[SUBBUTTON_STATS]->get()->moveTo(
        buttons[SUBBUTTON_STATS]->get()->getX() + 3 + 26 - 13,
        buttons[SUBBUTTON_STATS]->get()->getY() + 2);
    for (u32 i = SUBBUTTON_MULTI_VS; i <= SUBBUTTON_MULTI_COOP; i++)
      buttons[i]->get()->moveTo(buttons[i]->get()->getX() + 26 - 13,
                                buttons[i]->get()->getY() + 2);
    buttons[SUBBUTTON_DEATHMIX]->get()->moveTo(
        buttons[SUBBUTTON_DEATHMIX]->get()->getX(),
        buttons[SUBBUTTON_DEATHMIX]->get()->getY() + 2);
    selectedMode = SUBBUTTON_SINGLE;
    buttons[SUBBUTTON_SINGLE]->setSelected(true);
  } else
    buttons[BUTTON_PLAY]->setSelected(true);
}

void StartScene::setUpGameAnimation() {
  STATE_setup();
  GameState.positionX[0] = GAME_X;
  GameState.positionY = GAME_Y;

  arrowPool = std::unique_ptr<ObjectPool<Arrow>>{
      new ObjectPool<Arrow>(ARROW_POOL_SIZE, [](u32 id) -> Arrow* {
        return new Arrow(ARROW_TILEMAP_LOADING_ID + id);
      })};

  for (u32 i = 0; i < ARROWS_TOTAL; i++) {
    arrowHolders.push_back(std::unique_ptr<ArrowHolder>{
        new ArrowHolder(static_cast<ArrowDirection>(i), 0, true)});
    arrowHolders[i]->setIsPressed(false);
  }
}

void StartScene::animateBpm() {
  int audioLag = (int)GameState.settings.audioLag;
  int msecs = PlaybackState.msecs - audioLag;
  int beat = MATH_fracumul(msecs * BPM, FRACUMUL_DIV_BY_MINUTE);
  int tick =
      MATH_fracumul(msecs * BPM * getTickCount(), FRACUMUL_DIV_BY_MINUTE);

  if (beat != lastBeat && beat != 0) {
    lastBeat = beat;
    darkenerOpacity = 0;
    bounceDirection *= -1;

    for (auto& it : arrowHolders)
      it->blink();
  }

  if (tick != lastTick && beat != 0) {
    lastTick = tick;

    arrowPool->create([this](Arrow* it) {
      it->initialize(ArrowType::UNIQUE,
                     static_cast<ArrowDirection>(qran_range(0, ARROWS_TOTAL)),
                     0, 0, didWinImpossibleMode() && qran_range(1, 101) > 50);
      it->get()->moveTo(it->get()->getX(), DEMO_ARROW_INITIAL_Y);
      it->press();
    });
  }
}

void StartScene::animateArrows(int bounceOffset) {
  for (auto& it : arrowHolders)
    it->tick();

  u32 arrowSpeed = getArrowSpeed();
  arrowPool->forEachActive([this, arrowSpeed, bounceOffset](Arrow* arrow) {
    int newY = arrow->get()->getY() - arrowSpeed;

    arrow->tick(newY, false, bounceOffset);
    if (arrow->needsDiscard())
      arrowPool->discard(arrow->id - ARROW_TILEMAP_LOADING_ID);
  });
}

void StartScene::animateInputs(int bounceOffset) {
  if (SAVEFILE_isUsingGBAStyle()) {
    inputs[INPUT_LEFT]->get()->moveTo(23 + bounceOffset, 48);
    inputs[INPUT_RIGHT]->get()->moveTo(42 + bounceOffset, 48);
    inputs[INPUT_SELECT]->get()->moveTo(202 + bounceOffset, 46);
    SPRITE_hide(inputs[INPUT_SELECT_ALT]->get());
  } else {
    inputs[INPUT_LEFT]->get()->moveTo(24 + bounceOffset, 47);
    inputs[INPUT_RIGHT]->get()->moveTo(201 + bounceOffset, 44);
    inputs[INPUT_SELECT]->get()->moveTo(42 + bounceOffset, 47);
    inputs[INPUT_SELECT_ALT]->get()->moveTo(185 + bounceOffset, 53);
  }
}

void StartScene::updateExpandedOrCollapsedButtons() {
  if (ENV_ARCADE)
    return;

  isPlayExpanded =
      selectedMode >= SUBBUTTON_CAMPAIGN && selectedMode <= SUBBUTTON_STATS;
  isArcadeExpanded = selectedMode >= SUBBUTTON_MULTI_VS &&
                     selectedMode <= SUBBUTTON_MULTI_COOP;
  isChallengesExpanded = selectedMode >= SUBBUTTON_IMPOSSIBLE;

  if (isPlayExpanded) {
    buttons[BUTTON_PLAY]->hide();
    buttons[SUBBUTTON_CAMPAIGN]->show();
    buttons[SUBBUTTON_STATS]->show();
  } else {
    buttons[BUTTON_PLAY]->show();
    buttons[SUBBUTTON_CAMPAIGN]->hide();
    buttons[SUBBUTTON_STATS]->hide();
  }

  if (isArcadeExpanded) {
    buttons[BUTTON_ARCADE]->hide();
    buttons[SUBBUTTON_MULTI_VS]->show();
    buttons[SUBBUTTON_SINGLE]->show();
    buttons[SUBBUTTON_MULTI_COOP]->show();
  } else {
    buttons[BUTTON_ARCADE]->show();
    buttons[SUBBUTTON_MULTI_VS]->hide();
    buttons[SUBBUTTON_SINGLE]->hide();
    buttons[SUBBUTTON_MULTI_COOP]->hide();
  }

  if (isChallengesExpanded) {
    buttons[BUTTON_CHALLENGES]->hide();
    buttons[SUBBUTTON_IMPOSSIBLE]->show();
    buttons[SUBBUTTON_DEATHMIX]->show();
  } else {
    buttons[BUTTON_CHALLENGES]->show();
    buttons[SUBBUTTON_IMPOSSIBLE]->hide();
    buttons[SUBBUTTON_DEATHMIX]->hide();
  }
}

void StartScene::printTitle() {
  TextStream::instance().setFontColor(TEXT_COLOR);
  TextStream::instance().setFontSubcolor(text_bg_palette_default_subcolor);
  TextStream::instance().clear();

  SCENE_write(std::string((char*)gbfs_get_obj(fs, ROM_NAME_FILE, NULL)), 0);
  SCENE_write(ENV_ARCADE && selectedMode == SUBBUTTON_DEATHMIX
                  ? "Shuffle!"
                  : TITLES[selectedMode],
              TEXT_ROW);
}

void StartScene::processKeys(u16 keys) {
  inputs[INPUT_LEFT]->setIsPressed(KEY_GOLEFT(keys));
  inputs[INPUT_RIGHT]->setIsPressed(KEY_GORIGHT(keys));
  inputs[INPUT_SELECT]->setIsPressed(KEY_CONFIRM(keys));
  inputs[INPUT_SELECT_ALT]->setIsPressed(KEY_CONFIRM(keys));
}

void StartScene::processSelectionChange() {
  bool canGoLeft =
      (!ENV_ARCADE && ((isPlayExpanded && selectedMode > SUBBUTTON_CAMPAIGN) ||
                       (!isPlayExpanded && selectedMode > BUTTON_PLAY))) ||
      (ENV_ARCADE && selectedMode >= SUBBUTTON_MULTI_VS);

  if (inputs[INPUT_LEFT]->hasBeenPressedNow() && canGoLeft) {
    selectedMode--;

    if (ENV_ARCADE && selectedMode == SUBBUTTON_IMPOSSIBLE)
      selectedMode = SUBBUTTON_MULTI_COOP;

    if (selectedMode == BUTTON_CHALLENGES && isChallengesExpanded)
      selectedMode--;
    if (selectedMode == SUBBUTTON_MULTI_COOP && !isArcadeExpanded)
      selectedMode -= 3;
    if (selectedMode == BUTTON_ARCADE && isArcadeExpanded)
      selectedMode--;
    if (selectedMode == SUBBUTTON_STATS && !isPlayExpanded)
      selectedMode -= 2;

    updateExpandedOrCollapsedButtons();
    pixelBlink->blink();
    printTitle();
  }

  bool canGoRight =
      (!ENV_ARCADE &&
       ((isChallengesExpanded && selectedMode < SUBBUTTON_DEATHMIX) ||
        (!isChallengesExpanded && selectedMode < BUTTON_CHALLENGES))) ||
      (ENV_ARCADE && selectedMode <= SUBBUTTON_MULTI_COOP);
  if (inputs[INPUT_RIGHT]->hasBeenPressedNow() && canGoRight) {
    selectedMode++;

    if (ENV_ARCADE && selectedMode == BUTTON_CHALLENGES)
      selectedMode = SUBBUTTON_DEATHMIX;

    if (selectedMode == SUBBUTTON_CAMPAIGN && !isPlayExpanded)
      selectedMode += 2;
    if (selectedMode == BUTTON_ARCADE && isArcadeExpanded)
      selectedMode++;
    if (selectedMode == SUBBUTTON_MULTI_VS && !isArcadeExpanded)
      selectedMode += 3;
    if (selectedMode == BUTTON_CHALLENGES && isChallengesExpanded)
      selectedMode++;

    updateExpandedOrCollapsedButtons();
    pixelBlink->blink();
    printTitle();
  }

  if (inputs[INPUT_SELECT]->hasBeenPressedNow())
    goToGame();

  for (u32 i = 0; i < BUTTONS_TOTAL; i++)
    buttons[i]->setSelected(selectedMode == i);
}

void StartScene::navigateToAdminMenuIfNeeded(u16 keys) {
  if (!isPressingAdminCombo(keys))
    wasNotPressingAdminCombo = true;

  if (wasNotPressingAdminCombo && isPressingAdminCombo(keys)) {
    player_stop();
    engine->transitionIntoScene(new AdminScene(engine, fs),
                                new PixelTransitionEffect());
  }
}

bool StartScene::isPressingAdminCombo(u16 keys) {
  return (keys & KEY_L) && (keys & KEY_R) && (keys & KEY_START) &&
         (keys & KEY_SELECT);
}

void StartScene::goToGame() {
  if (selectedMode == BUTTON_PLAY) {
    selectedMode = SUBBUTTON_CAMPAIGN;
    updateExpandedOrCollapsedButtons();
    pixelBlink->blink();
    printTitle();
    return;
  }
  if (selectedMode == BUTTON_ARCADE) {
    selectedMode = SUBBUTTON_SINGLE;
    updateExpandedOrCollapsedButtons();
    pixelBlink->blink();
    printTitle();
    return;
  }
  if (selectedMode == BUTTON_CHALLENGES) {
    bool isImpossibleModeUnlocked =
        SAVEFILE_isModeUnlocked(GameMode::IMPOSSIBLE);
    if (isImpossibleModeUnlocked) {
      isChallengesExpanded = true;
      selectedMode = SUBBUTTON_IMPOSSIBLE;
      updateExpandedOrCollapsedButtons();
      pixelBlink->blink();
      printTitle();
      return;
    }
  }
  if (selectedMode == SUBBUTTON_STATS) {
    player_stop();
    engine->transitionIntoScene(new StatsScene(engine, fs),
                                new PixelTransitionEffect());
    return;
  }

  GameMode gameMode;
  switch (selectedMode) {
    case SUBBUTTON_MULTI_VS: {
      gameMode = GameMode::MULTI_VS;
      break;
    }
    case SUBBUTTON_SINGLE: {
      gameMode = GameMode::ARCADE;
      break;
    }
    case SUBBUTTON_MULTI_COOP: {
      gameMode = GameMode::MULTI_COOP;
      break;
    }
    case BUTTON_CHALLENGES:
    case SUBBUTTON_IMPOSSIBLE: {
      gameMode = GameMode::IMPOSSIBLE;
      break;
    }
    case SUBBUTTON_DEATHMIX: {
      gameMode = GameMode::DEATH_MIX;
      break;
    }
    default: {
      gameMode = GameMode::CAMPAIGN;
    }
  }

  player_stop();
  SEQUENCE_goToGameMode(gameMode);
}

StartScene::~StartScene() {
  buttons.clear();
  inputs.clear();
  arrowHolders.clear();
}
