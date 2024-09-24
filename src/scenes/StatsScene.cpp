#include "StatsScene.h"

#include <libgba-sprite-engine/background/text_stream.h>

#include "data/content/_compiled_sprites/palette_selection.h"
#include "gameplay/Key.h"
#include "gameplay/Library.h"
#include "gameplay/save/SaveFile.h"
#include "scenes/StartScene.h"
#include "utils/SceneUtils.h"
#include "utils/StringUtils.h"

#define TITLE "PROGRESS"

const u32 ID_MAIN_BACKGROUND = 1;
const u32 BANK_BACKGROUND_TILES = 0;
const u32 BANK_BACKGROUND_MAP = 16;
const u32 TEXT_COLOR = 0x7FFF;
const u32 BUTTON_MARGIN = 3;
const u32 PIXEL_BLINK_LEVEL = 4;
const u32 SELECT_BUTTON_X = 112;
const int TEXT_COL_UNSELECTED = -2;
const int TEXT_COL_VALUE_MIDDLE = 20;

StatsScene::StatsScene(std::shared_ptr<GBAEngine> engine, const GBFS_FILE* fs)
    : Scene(engine) {
  this->fs = fs;
}

std::vector<Background*> StatsScene::backgrounds() {
  return {bg.get()};
}

std::vector<Sprite*> StatsScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(selectButton->get());

  return sprites;
}

void StatsScene::load() {
  SCENE_init();

  setUpSpritesPalette();
  setUpBackground();

  pixelBlink = std::unique_ptr<PixelBlink>{new PixelBlink(PIXEL_BLINK_LEVEL)};

  selectButton = std::unique_ptr<ArrowSelector>{
      new ArrowSelector(ArrowDirection::CENTER, false, true, true, true)};

  selectButton->get()->moveTo(SELECT_BUTTON_X,
                              GBA_SCREEN_HEIGHT - ARROW_SIZE - BUTTON_MARGIN);

  TextStream::instance().setFontColor(TEXT_COLOR);
  TextStream::instance().setFontSubcolor(text_bg_palette_default_subcolor);
  TextStream::instance().scrollNow(0, 2);
  TextStream::instance().setMosaic(true);
  SCENE_write(TITLE, 1);
  printFixedLine("Loading...", "", 3);
}

void StatsScene::tick(u16 keys) {
  if (engine->isTransitioning() || !hasStarted)
    return;

  processKeys(keys);

  selectButton->tick();
  pixelBlink->tick();
}

void StatsScene::render() {
  if (engine->isTransitioning())
    return;

  if (!hasStarted) {
    BACKGROUND_enable(true, true, false, false);
    SPRITE_enable();
    hasStarted = true;
    return;
  }

  if (!didPrintText) {
    printStats();
    didPrintText = true;
  }
}

void StatsScene::setUpSpritesPalette() {
  foregroundPalette =
      std::unique_ptr<ForegroundPaletteManager>{new ForegroundPaletteManager(
          palette_selectionPal, sizeof(palette_selectionPal))};
}

void StatsScene::setUpBackground() {
  loadBackground(ID_MAIN_BACKGROUND);
  bg->useCharBlock(BANK_BACKGROUND_TILES);
  bg->useMapScreenBlock(BANK_BACKGROUND_MAP);
  bg->setMosaic(true);
}

void StatsScene::loadBackground(u32 id) {
  backgroundPalette = BACKGROUND_loadPaletteFile(fs, BG_WALL_PALETTE);
  bg = BACKGROUND_loadBackgroundFiles(fs, BG_WALL_TILES, BG_WALL_MAP, id);
}

void StatsScene::processKeys(u16 keys) {
  selectButton->setIsPressed(KEY_CONFIRM(keys));

  if (selectButton->hasBeenPressedNow()) {
    player_stop();
    engine->transitionIntoScene(new StartScene(engine, fs),
                                new PixelTransitionEffect());
  }
}

void StatsScene::printStats() {
  bool hasImpossible =
      !ENV_ARCADE && SAVEFILE_isModeUnlocked(GameMode::IMPOSSIBLE);
  bool hasDeathMix =
      !ENV_ARCADE && SAVEFILE_isModeUnlocked(GameMode::DEATH_MIX);

  std::string playTime = getPlayTime();
  u32 stagePasses = SAVEFILE_read32(SRAM->stats.stagePasses);
  u32 stageBreaks = SAVEFILE_read32(SRAM->stats.stageBreaks);
  u32 sGrades = SAVEFILE_read32(SRAM->stats.sGrades);
  u32 maxCombo = SAVEFILE_read32(SRAM->stats.maxCombo);
  std::string highestLevel = getHighestLevel();
  std::string campaignProgress =
      !ENV_ARCADE ? getCampaignProgress(GameMode::CAMPAIGN) : "";
  std::string campaignSClearProgress =
      !ENV_ARCADE ? getCampaignSClearProgress() : "";
  auto arcadeProgress = getArcadeProgress();
  std::string impossibleProgress =
      hasImpossible ? getCampaignProgress(GameMode::IMPOSSIBLE) : "";
  std::string deathMixProgress = hasDeathMix ? getDeathMixProgress() : "";

  VBlankIntrWait();
  pixelBlink->blink();
  printFixedLine("Play time", playTime, 3);
  printFixedLine("Songs played", std::to_string(stagePasses + stageBreaks), 4);
  printFixedLine("Stage passes", std::to_string(stagePasses), 5);
  printFixedLine("Stage breaks", std::to_string(stageBreaks), 6);
  printFixedLine("S grades", std::to_string(sGrades), 7);
  printFixedLine("Max combo", std::to_string(maxCombo), 8);
  printFixedLine("Highest level", highestLevel, 9);

  if (!ENV_ARCADE) {
    printFixedLine("Campaign\\", campaignProgress, 11);
    printFixedLine("Campaign\\ (S)", campaignSClearProgress, 12);
  }
  if (ENV_ARCADE) {
    printFixedLine("Arcade\\ (s)",
                   std::to_string(arcadeProgress.completedSingle) + "/" +
                       std::to_string(arcadeProgress.totalSingle) + " (" +
                       arcadeProgress.singleProgress + ")",
                   11, 2);
    printFixedLine("Arcade\\ (d)",
                   std::to_string(arcadeProgress.completedDouble) + "/" +
                       std::to_string(arcadeProgress.totalDouble) + " (" +
                       arcadeProgress.doubleProgress + ")",
                   12, 2);
  } else {
    printFixedLine(
        "Arcade\\",
        arcadeProgress.singleProgress + "/" + arcadeProgress.doubleProgress,
        13);
  }
  if (hasImpossible)
    printFixedLine("Impossible\\", impossibleProgress, 14);
  if (hasDeathMix)
    printFixedLine("DeathMix\\", deathMixProgress, 15);
  printFixedLine("Calories burned", "0 kcal", ENV_ARCADE ? 13 : 16, 1);
}

std::string StatsScene::getPlayTime() {
  u32 playTimeSeconds = SAVEFILE_read32(SRAM->stats.playTimeSeconds);
  std::string playTime = "";

  u32 days, hours, minutes, seconds;
  days = Div(playTimeSeconds, 86400);
  playTimeSeconds = DivMod(playTimeSeconds, 86400);
  hours = Div(playTimeSeconds, 3600);
  playTimeSeconds = DivMod(playTimeSeconds, 3600);
  minutes = Div(playTimeSeconds, 60);
  seconds = DivMod(playTimeSeconds, 60);
  if (days > 0)
    playTime += std::to_string(days) + "d ";
  if (hours > 0 || days > 0)
    playTime += std::to_string(hours) + "h ";
  if (minutes > 0 || hours > 0 || days > 0)
    playTime += std::to_string(minutes) + "m ";
  playTime += std::to_string(seconds) + "s";

  return playTime;
}

std::string StatsScene::getHighestLevel() {
  u32 rawHighestLevel = SAVEFILE_read32(SRAM->stats.highestLevel);
  if (rawHighestLevel == 0)
    return "-";
  u32 highestLevel = rawHighestLevel & 0xff;
  u32 highestType = (rawHighestLevel >> 16) & 0xff;

  return (highestType == 0   ? "s"
          : highestType == 1 ? "d"
                             : "m") +
         std::to_string(highestLevel);
}

std::string StatsScene::getCampaignProgress(GameMode gameMode) {
  auto librarySize = SAVEFILE_getLibrarySize();

  auto completedNormal =
      SAVEFILE_getCompletedSongsOf(gameMode, DifficultyLevel::NORMAL);
  auto completedHard =
      SAVEFILE_getCompletedSongsOf(gameMode, DifficultyLevel::HARD);
  auto completedCrazy =
      SAVEFILE_getCompletedSongsOf(gameMode, DifficultyLevel::CRAZY);

  return getPercentage(completedNormal, librarySize) + "/" +
         getPercentage(completedHard, librarySize) + "/" +
         getPercentage(completedCrazy, librarySize);
}

std::string StatsScene::getCampaignSClearProgress() {
  auto librarySize = SAVEFILE_getLibrarySize();

  u32 completedNormal = 0;
  u32 completedHard = 0;
  u32 completedCrazy = 0;

  for (u32 i = 0; i < librarySize; i++) {
    if (SAVEFILE_getStoryGradeOf(GameMode::CAMPAIGN, i,
                                 DifficultyLevel::NORMAL) == GradeType::S)
      completedNormal++;
    if (SAVEFILE_getStoryGradeOf(GameMode::CAMPAIGN, i,
                                 DifficultyLevel::HARD) == GradeType::S)
      completedHard++;
    if (SAVEFILE_getStoryGradeOf(GameMode::CAMPAIGN, i,
                                 DifficultyLevel::CRAZY) == GradeType::S)
      completedCrazy++;
  }

  return getPercentage(completedNormal, librarySize) + "/" +
         getPercentage(completedHard, librarySize) + "/" +
         getPercentage(completedCrazy, librarySize);
}

StatsScene::ArcadePercentages StatsScene::getArcadeProgress() {
  std::vector<std::unique_ptr<SongFile>> songFiles;
  auto library = std::unique_ptr<Library>{new Library(fs)};
  u32 librarySize = SAVEFILE_getLibrarySize();
  u32 pages =
      Div(librarySize, PAGE_SIZE) + (DivMod(librarySize, PAGE_SIZE) > 0);
  for (u32 i = 0; i < pages; i++)
    library->loadSongs(songFiles, DifficultyLevel::CRAZY, i * PAGE_SIZE);

  u32 totalSingle = 0, completedSingle = 0;
  u32 totalDouble = 0, completedDouble = 0;

  for (u32 i = 0; i < songFiles.size(); i++) {
    Song* song = SONG_parse(fs, songFiles[i].get());

    u32 songTotalSingle = 0;
    u32 songTotalDouble = 0;

    for (u32 j = 0; j < song->chartCount; j++) {
      if (song->charts[j].type == ChartType::SINGLE_CHART) {
        if (ARCADE_readSingle(song->id, songTotalSingle) < GradeType::UNPLAYED)
          completedSingle++;
        songTotalSingle++;
      } else {
        if (ARCADE_readDouble(song->id, songTotalDouble) < GradeType::UNPLAYED)
          completedDouble++;
        songTotalDouble++;
      }
    }

    totalSingle += songTotalSingle;
    totalDouble += songTotalDouble;

    SONG_free(song);
  }

  StatsScene::ArcadePercentages percentages;
  percentages.singleProgress = getPercentage(completedSingle, totalSingle);
  percentages.doubleProgress = getPercentage(completedDouble, totalDouble);
  percentages.totalSingle = totalSingle;
  percentages.completedSingle = completedSingle;
  percentages.totalDouble = totalDouble;
  percentages.completedDouble = completedDouble;
  return percentages;
}

std::string StatsScene::getDeathMixProgress() {
  auto librarySize = SAVEFILE_getLibrarySize();

  u32 completedNormal = SAVEFILE_read8(
      SRAM->deathMixProgress.completedSongs[DifficultyLevel::NORMAL]);
  u32 completedHard = SAVEFILE_read8(
      SRAM->deathMixProgress.completedSongs[DifficultyLevel::HARD]);
  u32 completedCrazy = SAVEFILE_read8(
      SRAM->deathMixProgress.completedSongs[DifficultyLevel::CRAZY]);

  return getPercentage(completedNormal, librarySize) + "/" +
         getPercentage(completedHard, librarySize) + "/" +
         getPercentage(completedCrazy, librarySize);
}

std::string StatsScene::getPercentage(u32 current, u32 total) {
  u32 percentage = Div(current * 100, total);
  auto str = percentage >= 100 ? "!!!" : std::to_string(percentage) + "\\";
  STRING_padLeft(str, 3, '0');
  return str;
}

void StatsScene::printFixedLine(std::string name,
                                std::string value,
                                u32 row,
                                u32 extraSpace) {
  TextStream::instance().setText(name, row, TEXT_COL_UNSELECTED);

  if (value.length() > 0) {
    STRING_padLeft(value, 14 - extraSpace);
    TextStream::instance().setText(value, row,
                                   TEXT_COL_VALUE_MIDDLE - 8 + extraSpace);
  }
}
