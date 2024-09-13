#include "DeathMixScene.h"

#include <string>

#include "SettingsScene.h"
#include "SongScene.h"
#include "assets.h"
#include "data/content/_compiled_sprites/palette_selection.h"
#include "gameplay/DifficultyLevelDeathMix.h"
#include "gameplay/Key.h"
#include "gameplay/NumericLevelDeathMix.h"
#include "gameplay/SequenceMessages.h"
#include "utils/SceneUtils.h"
#include "utils/StringUtils.h"

extern "C" {
#include "player/player.h"
}

const u32 PIXEL_BLINK_LEVEL = 4;
const u32 DIFFICULTY_X = 111;
const u32 DIFFICULTY_Y = 113;
const u32 PROGRESS_X = 94;
const u32 PROGRESS_Y = 130;
const u32 GRADE_X = 143;
const u32 GRADE_Y = 135;
const u32 BACK_X = 89;
const u32 BACK_Y = 112;
const u32 NEXT_X = 198;
const u32 NEXT_Y = 112;
const u32 MULTIPLIER_X = 143;
const u32 MULTIPLIER_Y = 93;
const u32 NUMERIC_LEVEL_X = 134;
const u32 NUMERIC_LEVEL_Y = 112;
const u32 NUMERIC_BUTTON_OFFSET_X = 24;
const u32 NUMERIC_BUTTON_OFFSET_Y = 7;
const u32 TEXT_ROW = 15;
const u32 TEXT_COL = 15;
const u32 MIN_LEVEL = 1;
const u32 MAX_LEVEL = 25;

std::vector<Sprite*> DeathMixScene::sprites() {
  auto sprites = TalkScene::sprites();

  sprites.push_back(backButton->get());
  sprites.push_back(nextButton->get());
  if (mixMode == MixMode::DEATH) {
    difficulty->render(&sprites);
    progress->render(&sprites);
    sprites.push_back(gradeBadge->get());
  } else {
    sprites.push_back(numericLevelBadge->get());
  }
  sprites.push_back(multiplier->get());

  return sprites;
}

void DeathMixScene::load() {
  SAVEFILE_write8(SRAM->state.isPlaying, false);
  TalkScene::load();

  setUpSpritesPalette();

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};
  multiplier = std::unique_ptr<Multiplier>{new Multiplier(
      MULTIPLIER_X, MULTIPLIER_Y, SAVEFILE_read8(SRAM->mods.multiplier))};
  if (mixMode == MixMode::DEATH) {
    difficulty =
        std::unique_ptr<Difficulty>{new Difficulty(DIFFICULTY_X, DIFFICULTY_Y)};
    progress = std::unique_ptr<NumericProgress>{
        new NumericProgress(PROGRESS_X, PROGRESS_Y)};
    gradeBadge = std::unique_ptr<GradeBadge>{
        new GradeBadge(GRADE_X, GRADE_Y, false, false)};
    gradeBadge->setType(GradeType::UNPLAYED);
  } else {
    numericLevelBadge = std::unique_ptr<Button>{new Button(
        ButtonType::LEVEL_METER, NUMERIC_LEVEL_X, NUMERIC_LEVEL_Y, false)};
    numericLevelBadge->get()->setPriority(1);
  }
  backButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::UPLEFT, true, true)};
  nextButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::UPRIGHT, true, true)};
  backButton->get()->moveTo(BACK_X, BACK_Y);
  nextButton->get()->moveTo(NEXT_X, NEXT_Y);
  settingsMenuInput = std::unique_ptr<InputHandler>{new InputHandler()};

  if (mixMode == MixMode::DEATH) {
    auto level = SAVEFILE_read8(SRAM->memory.difficultyLevel);
    difficulty->setValue(static_cast<DifficultyLevel>(level));

    loadProgress();
  } else {
    backButton->get()->moveTo(BACK_X + NUMERIC_BUTTON_OFFSET_X,
                              BACK_Y + NUMERIC_BUTTON_OFFSET_Y);
    nextButton->get()->moveTo(NEXT_X - NUMERIC_BUTTON_OFFSET_X,
                              NEXT_Y + NUMERIC_BUTTON_OFFSET_Y);

    // (force single mode)
    u32 lastNumericLevel = SAVEFILE_read32(SRAM->lastNumericLevel) & 0xff;
    updateNumericLevel(lastNumericLevel);
  }
}

void DeathMixScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  TalkScene::tick(keys);

  __qran_seed += ((1 + keys) * REG_VCOUNT) * SAVEFILE_read32(SRAM->randomSeed);
  processKeys(keys);

  pixelBlink->tick();
  backButton->tick();
  nextButton->tick();
  multiplier->tick();

  processDifficultyChangeEvents();
  processMenuEvents();

  if (mixMode == MixMode::DEATH) {
    if (gradeBadge->getType() == GradeType::S)
      EFFECT_setScale(0, BREATH_SCALE_LUT[animationFrame],
                      BREATH_SCALE_LUT[animationFrame]);
    animationFrame++;
    if (animationFrame >= BREATH_STEPS)
      animationFrame = 0;
  }

  if (hasStarted && mixMode == MixMode::SHUFFLE && !didRenderText) {
    printNumericLevel();
    didRenderText = true;
  }
  wasConfirming = KEY_CONFIRM(keys);
}

void DeathMixScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          palette_selectionPal, sizeof(palette_selectionPal))};
}

void DeathMixScene::processKeys(u16 keys) {
  backButton->setIsPressed(KEY_PREV(keys));
  nextButton->setIsPressed(KEY_NEXT(keys));
  multiplier->setIsPressed(KEY_SEL(keys));
  settingsMenuInput->setIsPressed(KEY_STA(keys));
}

void DeathMixScene::processDifficultyChangeEvents() {
  if (mixMode == MixMode::DEATH) {
    if (onDifficultyLevelChange(
            nextButton.get(),
            static_cast<DifficultyLevel>(
                min((int)difficulty->getValue() + 1, MAX_DIFFICULTY))))
      return;

    onDifficultyLevelChange(
        backButton.get(),
        static_cast<DifficultyLevel>(max((int)difficulty->getValue() - 1, 0)));
  } else {
    if (nextButton->shouldFireEvent()) {
      player_playSfx(SOUND_STEP);
      pixelBlink->blink();
      u32 newNumericLevel =
          (SAVEFILE_read32(SRAM->lastNumericLevel) & 0xff) + 1;
      updateNumericLevel(newNumericLevel);
      printNumericLevel();
      return;
    }

    if (backButton->shouldFireEvent()) {
      player_playSfx(SOUND_STEP);
      pixelBlink->blink();
      u32 newNumericLevel =
          (SAVEFILE_read32(SRAM->lastNumericLevel) & 0xff) - 1;
      updateNumericLevel(newNumericLevel);
      printNumericLevel();
    }
  }
}

void DeathMixScene::updateNumericLevel(u32 newNumericLevel) {
  if (newNumericLevel < MIN_LEVEL)
    newNumericLevel = MIN_LEVEL;
  if (newNumericLevel > MAX_LEVEL)
    newNumericLevel = MAX_LEVEL;
  SAVEFILE_write32(SRAM->lastNumericLevel, newNumericLevel);
}

void DeathMixScene::processMenuEvents() {
  if (multiplier->hasBeenPressedNow()) {
    player_playSfx(SOUND_MOD);
    SAVEFILE_write8(SRAM->mods.multiplier, multiplier->change());
  }

  if (settingsMenuInput->hasBeenPressedNow()) {
    player_stop();
    engine->transitionIntoScene(new SettingsScene(engine, fs),
                                new PixelTransitionEffect());
  }
}

bool DeathMixScene::onDifficultyLevelChange(ArrowSelector* button,
                                            DifficultyLevel newValue) {
  if (button->hasBeenPressedNow()) {
    player_playSfx(SOUND_STEP);

    if (newValue == difficulty->getValue())
      return true;

    SAVEFILE_write8(SRAM->memory.difficultyLevel, newValue);
    difficulty->setValue(newValue);
    loadProgress();
    pixelBlink->blink();

    return true;
  }

  return false;
}

void DeathMixScene::loadProgress() {
  u32 completed = SAVEFILE_read8(
      SRAM->deathMixProgress.completedSongs[difficulty->getValue()]);
  u32 total = SAVEFILE_getLibrarySize();

  progress->setValue(completed, total);

  gradeBadge->get()->setDoubleSize(true);
  gradeBadge->get()->setAffineId(AFFINE_BASE);

  if (completed == total) {
    auto gradeType = static_cast<GradeType>(
        SAVEFILE_read8(SRAM->deathMixProgress.grades[difficulty->getValue()]));

    progress->hide();
    gradeBadge->setType(gradeType);
  } else {
    progress->show();
    gradeBadge->setType(GradeType::UNPLAYED);
  }

  EFFECT_clearAffine();
}

void DeathMixScene::printNumericLevel() {
  u32 lastNumericLevel = SAVEFILE_read32(SRAM->lastNumericLevel) & 0xff;
  std::string level = std::to_string(lastNumericLevel);
  STRING_padLeft(level, 2, '0');
  TextStream::instance().setText(level, TEXT_ROW, TEXT_COL);
}

void DeathMixScene::confirm(u16 keys) {
  bool isPressed = KEY_CONFIRM(keys);
  bool hasBeenPressedNow = isPressed && !wasConfirming;

  if (hasBeenPressedNow) {
    SAVEFILE_write32(SRAM->randomSeed, __qran_seed);

    player_stop();
    auto deathMix =
        mixMode == MixMode::DEATH
            ? std::unique_ptr<DeathMix>{new DifficultyLevelDeathMix(
                  fs, difficulty->getValue())}
            : std::unique_ptr<DeathMix>{new NumericLevelDeathMix(
                  fs, SAVEFILE_read32(SRAM->lastNumericLevel) & 0xff)};
    if (deathMix->isEmpty()) {
      player_playSfx(SOUND_MOD);
      pixelBlink->blink();
      return;
    }
    auto songChart = deathMix->getNextSongChart();

    SAVEFILE_write8(SRAM->state.isPlaying, true);
    STATE_setup(songChart.song, songChart.chart);
    deathMix->multiplier = GameState.mods.multiplier;
    engine->transitionIntoScene(
        new SongScene(engine, fs, songChart.song, songChart.chart, NULL,
                      std::move(deathMix)),
        new PixelTransitionEffect());
  }
}
