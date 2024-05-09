#ifndef SELECTION_SCENE_H
#define SELECTION_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba/tonc_math.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include <string>
#include <vector>

#include "gameplay/Library.h"
#include "gameplay/multiplayer/Syncer.h"
#include "gameplay/save/SaveFile.h"
#include "objects/base/InputHandler.h"
#include "objects/ui/ArrowSelector.h"
#include "objects/ui/Button.h"
#include "objects/ui/ChannelBadge.h"
#include "objects/ui/Difficulty.h"
#include "objects/ui/Explosion.h"
#include "objects/ui/GradeBadge.h"
#include "objects/ui/Lock.h"
#include "objects/ui/Multiplier.h"
#include "objects/ui/NumericProgress.h"
#include "ui/Highlighter.h"
#include "utils/PixelBlink.h"

extern "C" {
#include "player/player.h"
#include "utils/gbfs/gbfs.h"
}

enum InitialLevel { KEEP_LEVEL, FIRST_LEVEL, LAST_LEVEL };

class SelectionScene : public Scene {
 public:
  SelectionScene(std::shared_ptr<GBAEngine> engine,
                 const GBFS_FILE* fs,
                 InitialLevel initialLevel = InitialLevel::KEEP_LEVEL);

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;
  void render() override;

 private:
  u32 init = 0;
  std::unique_ptr<Background> bg;
  std::unique_ptr<PixelBlink> pixelBlink;
  const GBFS_FILE* fs;
  InitialLevel initialLevel;
  std::string pendingAudio = "";
  u32 pendingSeek = 0;
  u32 animationFrame = 0;

  std::unique_ptr<Library> library;
  std::vector<std::unique_ptr<SongFile>> songs;
  std::vector<std::unique_ptr<ArrowSelector>> arrowSelectors;
  std::vector<std::unique_ptr<ChannelBadge>> channelBadges;
  std::vector<std::unique_ptr<GradeBadge>> gradeBadges;
  std::vector<std::unique_ptr<Lock>> locks;
  std::unique_ptr<Difficulty> difficulty;
  std::unique_ptr<Multiplier> multiplier;
  std::unique_ptr<NumericProgress> progress;
  std::unique_ptr<InputHandler> settingsMenuInput;
  std::unique_ptr<Button> numericLevelBadge;
  std::unique_ptr<Explosion> loadingIndicator1;
  std::unique_ptr<Explosion> loadingIndicator2;
  std::vector<u8> numericLevels;
  u32 page = 0;
  u32 selected = 0;
  u32 count = 0;
  u32 selectedSongId = 0;
  bool confirmed = false;
  u8 isCrossingPage = false;
  u32 blendAlpha = HIGHLIGHTER_OPACITY;
  u32 blendCount = 0;

  inline void playNow(const char* name) {
    player_playSfx(name);
    pendingAudio = "";
    pendingSeek = 0;
  }

  inline SongFile* getSelectedSong() { return songs[selected].get(); }
  inline u32 getSelectedSongIndex() { return getPageStart() + selected; }
  inline u32 getPageStart() { return page * PAGE_SIZE; }

  inline u32 getLastUnlockedSongIndex() {
    return min(getCompletedSongs(), count - 1);
  }

  inline u32 getCompletedSongs() {
#ifdef SENV_DEVELOPMENT
    if (isMultiplayer())
      return SAVEFILE_getLibrarySize();
#endif

    u32 completed;
    if (ENV_ARCADE)
      completed =
          isBonusMode() ? SAVEFILE_bonusCount(fs) : SAVEFILE_getLibrarySize();
    else if (isMultiplayer())
      completed = syncer->$completedSongs;
    else
      completed = SAVEFILE_getGameMode() == GameMode::ARCADE
                      ? isBonusMode() ? SAVEFILE_bonusCount(fs)
                                      : SAVEFILE_getMaxCompletedSongs()
                      : SAVEFILE_getCompletedSongsOf(difficulty->getValue());

    return min(completed, SAVEFILE_getLibrarySize());
  }

  inline u8 getSelectedNumericLevel() {
    if (numericLevels.empty())
      return 0;

    return numericLevels[getSelectedNumericLevelIndex()];
  }

  inline u8 getSelectedNumericLevelIndex() {
    if (numericLevels.empty())
      return 0;

    return SAVEFILE_read8(SRAM->memory.numericLevel);
  }

  inline void setClosestNumericLevel(u8 level) {
    if (numericLevels.empty()) {
      SAVEFILE_write8(SRAM->memory.numericLevel, 0);
      return;
    }

    auto currentLevel = getSelectedNumericLevel();
    auto currentLevelIndex = getSelectedNumericLevelIndex();
    bool wantsCurrentLevel = level == currentLevel;

    u32 min = 0;
    u32 minDiff = abs((int)numericLevels[0] - (int)level);
    for (u32 i = 0; i < numericLevels.size(); i++) {
      if (wantsCurrentLevel && numericLevels[i] == currentLevel &&
          i == currentLevelIndex)
        return;

      u32 diff = (u32)abs((int)numericLevels[i] - (int)level);
      if (diff < minDiff) {
        min = i;
        minDiff = diff;
      }
    }

    SAVEFILE_write8(SRAM->memory.numericLevel, min);
  }

  inline DifficultyLevel getLibraryType() {
    if (ENV_ARCADE)
      return isBonusMode() ? DifficultyLevel::NUMERIC : DifficultyLevel::CRAZY;

    if (isMultiplayer())
      return static_cast<DifficultyLevel>(syncer->$libraryType);

    if (IS_STORY(SAVEFILE_getGameMode()))
      return difficulty->getValue();

    return isBonusMode() ? DifficultyLevel::NUMERIC
                         : SAVEFILE_getMaxLibraryType();
  }

  inline bool isBonusMode() { return SAVEFILE_read8(SRAM->isBonusMode); }

  inline bool isCustomOffsetAdjustmentEnabled() {
    bool isOffsetEditingEnabled =
        SAVEFILE_read8(SRAM->adminSettings.offsetEditingEnabled);
    return SAVEFILE_getGameMode() == GameMode::ARCADE && isOffsetEditingEnabled;
  }
  inline int getCustomOffset() {
    return OFFSET_get(selectedSongId, getSelectedNumericLevelIndex(),
                      isDouble());
  }
  inline void updateCustomOffset(int change) {
    if (numericLevels.empty())
      return;
    auto selectedNumericLevelIndex = getSelectedNumericLevelIndex();

    OFFSET_set(
        selectedSongId, selectedNumericLevelIndex, isDouble(),
        OFFSET_get(selectedSongId, selectedNumericLevelIndex, isDouble()) +
            change);
  }

  void setUpSpritesPalette();
  void setUpBackground();
  void setUpArrows();
  void setUpChannelBadges();
  void setUpGradeBadges();
  void setUpLocks();
  void setUpPager();

  void scrollTo(u32 songIndex);
  void scrollTo(u32 page, u32 selected);
  void setNumericLevel(u8 numericLevelIndex);
  void goToSong();

  void processKeys(u16 keys);
  void processDifficultyChangeEvents();
  void processSelectionChangeEvents();
  void processConfirmEvents();
  void processMenuEvents();
  bool onDifficultyLevelChange(ArrowDirection selector,
                               DifficultyLevel newValue);
  bool onNumericLevelChange(ArrowDirection selector, u8 newValue);
  bool onSelectionChange(ArrowDirection selector,
                         bool isOnListEdge,
                         bool isOnPageEdge,
                         int direction);
  void onConfirmOrStart(bool isConfirmed);
  bool onCustomOffsetChange(ArrowDirection selector, int offset);

  void updateSelection(bool isChangingLevel = false);
  void updateLevel(Song* song, bool isChangingLevel);
  void confirm();
  void unconfirm();
  void setPage(u32 page, int direction);
  void startPageCross(int direction);
  void stopPageCross1();
  void stopPageCross2();
  void loadChannels();
  void loadProgress();
  void setNames(std::string title, std::string artist);
  void printNumericLevel(Chart* chart) { printNumericLevel(chart, 0); }
  void printNumericLevel(Chart* chart, s8 offsetY);
  void loadSelectedSongGrade();
  void processMultiplayerUpdates();
  void syncNumericLevelChanged(u8 newValue);
  void quit();

  ~SelectionScene();
};

#endif  // SELECTION_SCENE_H
