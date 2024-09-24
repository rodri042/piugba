#include "StageBreakScene.h"

#include "DeathMixScene.h"
#include "SelectionScene.h"
#include "assets.h"
#include "data/content/_compiled_sprites/palette_break.h"
#include "gameplay/Key.h"
#include "gameplay/Sequence.h"
#include "gameplay/multiplayer/Syncer.h"
#include "player/PlaybackState.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "player/player.h"
}

const u32 ID_MAIN_BACKGROUND = 1;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 16;
const u32 TEXT_COLOR_START = 2686;
const u32 TEXT_COLOR_END = 0x7FFF;
const u32 TEXT_BLEND_ALPHA = 12;
const u32 INSTRUCTOR_X = 152;
const u32 INSTRUCTOR_Y = 48;
const u32 PIXEL_BLINK_LEVEL = 2;

#define WRITE(MSECS, TEXT, ROW, COL, DX, DY, SCALE_X, SCALE_Y)   \
  if (msecs > MSECS && step == totalSteps) {                     \
    TextStream::instance().setText(TEXT, ROW, COL);              \
    instructor->get()->moveTo(instructor->get()->getX() + (DX),  \
                              instructor->get()->getY() + (DY)); \
    EFFECT_setScale(0, SCALE_X, SCALE_Y);                        \
    pixelBlink->blink();                                         \
    step++;                                                      \
  }                                                              \
  totalSteps++;

#define CLEAR(MSECS)                         \
  if (msecs > MSECS && step == totalSteps) { \
    TextStream::instance().clear();          \
    step++;                                  \
  }                                          \
  totalSteps++;

StageBreakScene::StageBreakScene(std::shared_ptr<GBAEngine> engine,
                                 const GBFS_FILE* fs)
    : Scene(engine) {
  this->fs = fs;
}

std::vector<Background*> StageBreakScene::backgrounds() {
  return {bg.get()};
}

std::vector<Sprite*> StageBreakScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(instructor->get());

  return sprites;
}

void StageBreakScene::load() {
  if (isMultiplayer())
    syncer->clearTimeout();

  SCENE_init();

  setUpSpritesPalette();
  setUpBackground();

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};
  EFFECT_setUpBlend(BLD_BG0, BLD_BG1);
  EFFECT_setBlendAlpha(TEXT_BLEND_ALPHA);

  instructor = std::unique_ptr<Instructor>{
      new Instructor(SAVEFILE_isUsingModernTheme() ? InstructorType::AngryGirl2
                                                   : InstructorType::AngryGirl1,
                     INSTRUCTOR_X, INSTRUCTOR_Y)};
  instructor->get()->setDoubleSize(true);
  instructor->get()->setAffineId(AFFINE_BASE);

  updateStats();
}

void StageBreakScene::tick(u16 keys) {
  if (engine->isTransitioning() || !hasStarted)
    return;

  if (SEQUENCE_isMultiplayerSessionDead()) {
    player_stop();
    SEQUENCE_goToMultiplayerGameMode(SAVEFILE_getGameMode());
    return;
  }

  if (isMultiplayer()) {
    processMultiplayerUpdates();
    if (!syncer->isPlaying())
      return;
  }

  pixelBlink->tick();

  if (PlaybackState.hasFinished && KEY_ANYKEY(keys)) {
    if (isMultiplayer()) {
      if (syncer->isMaster())
        syncer->send(SYNC_EVENT_CONFIRM_SONG_END, 0);
      else
        return;
    }

    finish();
  }
}

void StageBreakScene::render() {
  if (engine->isTransitioning())
    return;

  if (!hasStarted) {
    BACKGROUND_enable(true, true, false, false);
    SPRITE_enable();
    player_play(SOUND_STAGE_BREAK,
                isMultiplayer() || active_flashcart == EZ_FLASH_OMEGA);
    hasStarted = true;
  }

  animate();
}

void StageBreakScene::setUpSpritesPalette() {
  foregroundPalette = std::unique_ptr<ForegroundPaletteManager>{
      new ForegroundPaletteManager(palette_breakPal, sizeof(palette_breakPal))};
}

void StageBreakScene::setUpBackground() {
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_WALL_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_WALL_TILES, BG_WALL_MAP,
                                      ID_MAIN_BACKGROUND);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void StageBreakScene::updateStats() {
  if (GameState.isStatUpdatingDisabled())
    return;

  u32 breaks = SAVEFILE_read32(SRAM->stats.stageBreaks);
  SAVEFILE_write32(SRAM->stats.stageBreaks, breaks + 1);
}

void StageBreakScene::animate() {
  u32 msecs = PlaybackState.msecs;
  u32 totalSteps = 0;
  int heyRow = 9;
  int heyCol = 1;

  TextStream::instance().setFontColor(msecs >= 2110 ? TEXT_COLOR_END
                                                    : TEXT_COLOR_START);
  TextStream::instance().setFontSubcolor(text_bg_palette_default_subcolor);

  WRITE(1500, "H", heyRow, heyCol + 0, -1, 2, 228, 228);
  WRITE(1525, "e", heyRow, heyCol + 1, 3, -3, 205, -205);
  WRITE(1550, "e", heyRow, heyCol + 2, 1, 1, -186, 186);
  WRITE(1575, "e", heyRow, heyCol + 3, -3, -4, 171, 171);
  WRITE(1600, "e", heyRow, heyCol + 4, -6, 4, -158, 158);
  WRITE(1625, "e", heyRow, heyCol + 5, -2, -3, 146, 146);
  WRITE(1650, "y", heyRow, heyCol + 6, -2, -1, 137, 137);
  WRITE(1675, "!", heyRow, heyCol + 7, -2, 1, 128, 128);
  CLEAR(2110);

  WRITE(2116, "Why", /*   */ 6, 1, -10, -8, 0x100, 0x100);
  WRITE(2310, "don't", /* */ 7, 4, 2, 10, 0x100, 0x100);
  WRITE(2430, "you", /*   */ 8, 9, -4, -3, -0x100, 0x100);
  WRITE(2480, "just", /*  */ 9, 4, 6, -2, 0x100, 0x100);
  WRITE(2660, "get", /*   */ 10, 1, -3, -6, 0x100, 0x100);
  WRITE(2730, "up", /*    */ 11, 5, 4, 8, -0x100, 0x100);
  WRITE(2830, "and", /*   */ 12, 8, -2, 4, 0x100, 0x100);
  WRITE(2970, "dance,", /**/ 13, 1, -4, -4, 0x100, 0x100);
  WRITE(3280, "man?", /*  */ 14, 7, 2, 4, 0x100, 0x100);
}

void StageBreakScene::finish() {
  player_stop();

  if (SAVEFILE_getGameMode() == GameMode::DEATH_MIX) {
    bool isShuffleMode = ENV_ARCADE || SAVEFILE_read8(SRAM->isShuffleMode) == 1;
    engine->transitionIntoScene(
        new DeathMixScene(engine, fs, static_cast<MixMode>(isShuffleMode)),
        new PixelTransitionEffect());
  } else {
    engine->transitionIntoScene(new SelectionScene(engine, fs),
                                new PixelTransitionEffect());
  }
}

void StageBreakScene::processMultiplayerUpdates() {
  auto remoteId = syncer->getRemotePlayerId();

  while (syncer->isPlaying() && linkUniversal->canRead(remoteId)) {
    u16 message = linkUniversal->read(remoteId);
    u8 event = SYNC_MSG_EVENT(message);

    if (syncer->isMaster()) {
      syncer->registerTimeout();
      continue;
    }

    switch (event) {
      case SYNC_EVENT_CONFIRM_SONG_END: {
        finish();

        syncer->clearTimeout();
        break;
      }
      default: {
        syncer->registerTimeout();
      }
    }
  }
}
