#include "DanceGradeScene.h"

#include "assets.h"
#include "data/content/_compiled_sprites/palette_grade.h"
#include "data/content/_compiled_sprites/palette_grade_multi.h"
#include "gameplay/Key.h"
#include "gameplay/Sequence.h"
#include "gameplay/multiplayer/Syncer.h"
#include "objects/score/combo/Combo.h"
#include "player/PlaybackState.h"
#include "utils/SceneUtils.h"
#include "utils/StringUtils.h"

extern "C" {
#include "player/player.h"
}

#define SCORE_TITLE "Score:"
#define PLAYER_1_WINS "Player 1 *WINS*"
#define PLAYER_2_WINS "Player 2 *WINS*"
#define GAME_TIED "Game *TIED*"
#define PLAYER_1_ARROW "<<<"
#define PLAYER_2_ARROW ">>>"
#define GAME_TIED_ARROW "(!)"

const u32 ID_MAIN_BACKGROUND = 1;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 16;
const u32 TEXT_COLOR = 0b111111111111101;
const u32 TEXT_ROW = 17;
const u32 SCORE_DIGITS = 8;

const u32 TOTALS_X[] = {11, 160};
const u32 TOTALS_Y[] = {37 - 2, 53 - 2, 69 - 2, 85 - 2, 101 - 2};
const u32 TOTAL_MAX_COMBO_Y = 117 - 2;
const u32 GRADE_X = 88;
const u32 GRADE_Y = 52 - 2;
const u32 MINI_GRADE_X[] = {37, 187};
const u32 MINI_GRADE_Y = 137;

DanceGradeScene::DanceGradeScene(std::shared_ptr<GBAEngine> engine,
                                 const GBFS_FILE* fs,
                                 std::unique_ptr<Evaluation> evaluation,
                                 std::unique_ptr<Evaluation> remoteEvaluation,
                                 std::string songTitle,
                                 std::string songArtist,
                                 std::string songLevel,
                                 bool differentCharts,
                                 bool isLastSong)
    : Scene(engine) {
  this->fs = fs;
  this->evaluation = std::move(evaluation);
  this->remoteEvaluation = std::move(remoteEvaluation);
  this->songTitle = songTitle;
  this->songArtist = songArtist;
  this->songLevel = songLevel;
  this->differentCharts = differentCharts;
  this->isLastSong = isLastSong;
}

std::vector<Background*> DanceGradeScene::backgrounds() {
  return {bg.get()};
}

std::vector<Sprite*> DanceGradeScene::sprites() {
  std::vector<Sprite*> sprites;

  for (auto& it : totals)
    it->render(&sprites);

  if (isVs())
    for (auto& it : remoteTotals)
      it->render(&sprites);

  maxComboTotal->render(&sprites);

  if (isVs())
    remoteMaxComboTotal->render(&sprites);

  if (isVs())
    for (auto& it : miniGrades)
      sprites.push_back(it->get());
  else
    sprites.push_back(grade->get());

  return sprites;
}

void DanceGradeScene::load() {
  if (isMultiplayer())
    syncer->clearTimeout();

  SCENE_init();
  TextStream::instance().setMosaic(true);

  setUpSpritesPalette();
  setUpBackground();
  printScore();

  SCENE_write(songTitle, 1);
  TextStream::instance().setText(
      "- " + songArtist + " -", 2,
      TEXT_MIDDLE_COL - (songArtist.length() + 4) / 2);
  SCENE_write(songLevel, 3);

  bool has4Digits = evaluation->needs4Digits() ||
                    (isVs() && remoteEvaluation->needs4Digits());

  u32 totalsX = TOTALS_X[!isVs() || syncer->getLocalPlayerId() == 1];
  for (u32 i = 0; i < totals.size(); i++)
    totals[i] = std::unique_ptr<Total>{
        new Total(totalsX, TOTALS_Y[i], has4Digits, i == 0)};
  maxComboTotal = std::unique_ptr<Total>{
      new Total(totalsX, TOTAL_MAX_COMBO_Y, has4Digits, false)};

  totals[FeedbackType::PERFECT]->setValue(evaluation->perfects);
  totals[FeedbackType::GREAT]->setValue(evaluation->greats);
  totals[FeedbackType::GOOD]->setValue(evaluation->goods);
  totals[FeedbackType::BAD]->setValue(evaluation->bads);
  totals[FeedbackType::MISS]->setValue(evaluation->misses);
  maxComboTotal->setValue(evaluation->maxCombo);

  if (isVs()) {
    u8 localId = syncer->getLocalPlayerId();
    u8 remoteId = syncer->getRemotePlayerId();

    miniGrades[0] = std::unique_ptr<GradeBadge>{
        new GradeBadge(MINI_GRADE_X[localId], MINI_GRADE_Y, false, true)};
    miniGrades[1] = std::unique_ptr<GradeBadge>{
        new GradeBadge(MINI_GRADE_X[remoteId], MINI_GRADE_Y, true, true)};
    miniGrades[0]->setType(evaluation->getGrade());
    miniGrades[1]->setType(remoteEvaluation->getGrade());
    miniGrades[0]->get()->setDoubleSize(true);
    miniGrades[0]->get()->setAffineId(AFFINE_BASE);
    miniGrades[1]->get()->setDoubleSize(true);
    miniGrades[1]->get()->setAffineId(AFFINE_BASE + 1);

    for (u32 i = 0; i < remoteTotals.size(); i++)
      remoteTotals[i] = std::unique_ptr<Total>{
          new Total(TOTALS_X[remoteId], TOTALS_Y[i], has4Digits, false)};
    remoteMaxComboTotal = std::unique_ptr<Total>{
        new Total(TOTALS_X[remoteId], TOTAL_MAX_COMBO_Y, has4Digits, false)};

    remoteTotals[FeedbackType::PERFECT]->setValue(remoteEvaluation->perfects);
    remoteTotals[FeedbackType::GREAT]->setValue(remoteEvaluation->greats);
    remoteTotals[FeedbackType::GOOD]->setValue(remoteEvaluation->goods);
    remoteTotals[FeedbackType::BAD]->setValue(remoteEvaluation->bads);
    remoteTotals[FeedbackType::MISS]->setValue(remoteEvaluation->misses);
    remoteMaxComboTotal->setValue(remoteEvaluation->maxCombo);
  } else {
    grade = std::unique_ptr<Grade>{
        new Grade(evaluation->getGrade(), GRADE_X, GRADE_Y)};
    grade->get()->setDoubleSize(true);
    grade->get()->setAffineId(AFFINE_BASE);
  }

  updateStats();
}

void DanceGradeScene::tick(u16 keys) {
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

  if (PlaybackState.hasFinished && KEY_ANYKEY(keys)) {
    if (isMultiplayer()) {
      if (syncer->isMaster())
        syncer->send(SYNC_EVENT_CONFIRM_SONG_END, 0);
      else
        return;
    }

    finish();
  }

  if (isVs()) {
    if (miniGrades[0]->getType() == GradeType::S)
      EFFECT_setScale(0, BREATH_SCALE_LUT[animationFrame],
                      BREATH_SCALE_LUT[animationFrame]);
    if (miniGrades[1]->getType() == GradeType::S)
      EFFECT_setScale(1, BREATH_SCALE_LUT[animationFrame],
                      BREATH_SCALE_LUT[animationFrame]);
  } else {
    if (grade->getType() == GradeType::S)
      EFFECT_setScale(0, BREATH_SCALE_LUT[animationFrame],
                      BREATH_SCALE_LUT[animationFrame]);
  }
  animationFrame++;
  if (animationFrame >= BREATH_STEPS)
    animationFrame = 0;
}

void DanceGradeScene::render() {
  if (engine->isTransitioning())
    return;

  if (!hasStarted) {
    BACKGROUND_enable(true, true, false, false);
    SPRITE_enable();
    playSound();
    hasStarted = true;
  }
}

void DanceGradeScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          isVs() ? palette_grade_multiPal : palette_gradePal,
          isVs() ? sizeof(palette_grade_multiPal) : sizeof(palette_gradePal))};
}

void DanceGradeScene::setUpBackground() {
  if (isVs()) {
    if (SAVEFILE_isUsingModernTheme()) {
      backgroundPalette =
          BACKGROUND_loadPaletteFile(fs, BG_GRADE_MULTI_MDRN_PALETTE);
      bg = BACKGROUND_loadBackgroundFiles(fs, BG_GRADE_MULTI_MDRN_TILES,
                                          BG_GRADE_MULTI_MDRN_MAP,
                                          ID_MAIN_BACKGROUND);
    } else {
      backgroundPalette =
          BACKGROUND_loadPaletteFile(fs, BG_GRADE_MULTI_PALETTE);
      bg = BACKGROUND_loadBackgroundFiles(
          fs, BG_GRADE_MULTI_TILES, BG_GRADE_MULTI_MAP, ID_MAIN_BACKGROUND);
    }
  } else {
    if (SAVEFILE_isUsingModernTheme()) {
      backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_GRADE_MDRN_PALETTE);
      bg = BACKGROUND_loadBackgroundFiles(
          fs, BG_GRADE_MDRN_TILES, BG_GRADE_MDRN_MAP, ID_MAIN_BACKGROUND);
    } else {
      backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_GRADE_PALETTE);
      bg = BACKGROUND_loadBackgroundFiles(fs, BG_GRADE_TILES, BG_GRADE_MAP,
                                          ID_MAIN_BACKGROUND);
    }
  }
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void DanceGradeScene::finish() {
  player_stop();
  SEQUENCE_goToWinOrSelection(isLastSong);
}

void DanceGradeScene::printScore() {
  TextStream::instance().setFontColor(TEXT_COLOR);
  TextStream::instance().setFontSubcolor(text_bg_palette_default_subcolor);

  if (isVs()) {
    TextStream::instance().scrollNow(0, -1);

    auto player1Evaluation = syncer->getLocalPlayerId() == 0
                                 ? evaluation.get()
                                 : remoteEvaluation.get();
    auto player2Evaluation = syncer->getLocalPlayerId() == 1
                                 ? evaluation.get()
                                 : remoteEvaluation.get();

    auto player1Points = getMultiplayerPointsOf(player1Evaluation);
    auto player2Points = getMultiplayerPointsOf(player2Evaluation);

    if (player1Points > player2Points) {
      SCENE_write(PLAYER_1_WINS, TEXT_ROW);
      SCENE_write(PLAYER_1_ARROW, TEXT_ROW + 1);
    } else if (player2Points > player1Points) {
      SCENE_write(PLAYER_2_WINS, TEXT_ROW);
      SCENE_write(PLAYER_2_ARROW, TEXT_ROW + 1);
    } else {
      SCENE_write(GAME_TIED, TEXT_ROW);
      SCENE_write(GAME_TIED_ARROW, TEXT_ROW + 1);
    }

    if (differentCharts) {
      auto points1Str = std::to_string(player1Points);
      STRING_padLeft(points1Str, 3, '0');
      auto points2Str = std::to_string(player2Points);
      STRING_padLeft(points2Str, 3, '0');
      STRING_padLeft(points2Str, TEXT_TOTAL_COLS - points1Str.length());
      auto pointsStr = points1Str + points2Str;
      SCENE_write(pointsStr, 0);
    } else {
      auto points1Str = pointsToString(player1Points);
      auto points2Str = pointsToString(player2Points);
      STRING_padLeft(points2Str, TEXT_TOTAL_COLS - points1Str.length());
      auto pointsStr = points1Str + points2Str;
      SCENE_write(pointsStr, 0);
    }
  } else {
    SCENE_write(SCORE_TITLE, TEXT_ROW);
    SCENE_write(pointsToString(evaluation->points), TEXT_ROW + 1);
  }
}

std::string DanceGradeScene::pointsToString(u32 points) {
  auto pointsStr = std::to_string(points);
  STRING_padLeft(pointsStr, SCORE_DIGITS, '0');
  return pointsStr;
}

u32 DanceGradeScene::getMultiplayerPointsOf(Evaluation* evaluation) {
  return differentCharts ? evaluation->percent : evaluation->points;
}

void DanceGradeScene::updateStats() {
  if (!GameState.isStatUpdatingDisabled()) {
    u32 passes = SAVEFILE_read32(SRAM->stats.stagePasses);
    SAVEFILE_write32(SRAM->stats.stagePasses, passes + 1);

    GradeType gradeType = isVs() ? miniGrades[0]->getType() : grade->getType();
    if (gradeType == GradeType::S) {
      u32 sGrades = SAVEFILE_read32(SRAM->stats.sGrades);
      SAVEFILE_write32(SRAM->stats.sGrades, sGrades + 1);
    }
  }

  if (!GameState.isStatUpdatingDisabled() ||
      GameState.mode == GameMode::DEATH_MIX) {
    u32 newCombo = evaluation->maxCombo;
    if (newCombo > MAX_COMBO)
      newCombo = MAX_COMBO;
    u32 maxCombo = SAVEFILE_read32(SRAM->stats.maxCombo);
    if (newCombo > maxCombo) {
      SAVEFILE_write32(SRAM->stats.maxCombo, newCombo);
    }
  }
}

void DanceGradeScene::playSound() {
  auto gradeType = isVs() ? miniGrades[0].get()->getType() : grade->getType();
  auto forceGSM = isMultiplayer() || active_flashcart == EZ_FLASH_OMEGA;

  switch (gradeType) {
    case GradeType::S: {
      player_play(SOUND_RANK_S, forceGSM);
      break;
    }
    case GradeType::A: {
      player_play(SOUND_RANK_A, forceGSM);
      break;
    }
    case GradeType::B: {
      player_play(SOUND_RANK_B, forceGSM);
      break;
    }
    case GradeType::C: {
      player_play(SOUND_RANK_C, forceGSM);
      break;
    }
    case GradeType::D: {
      player_play(SOUND_RANK_D, forceGSM);
      break;
    }
    case GradeType::F: {
      player_play(SOUND_RANK_F, forceGSM);
      break;
    }
    default: {
    }
  }
}

void DanceGradeScene::processMultiplayerUpdates() {
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
