#ifndef DANCE_GRADE_SCENE_H
#define DANCE_GRADE_SCENE_H

#include <libgba-sprite-engine/background/background.h>
#include <libgba-sprite-engine/gba_engine.h>
#include <libgba-sprite-engine/scene.h>
#include <libgba-sprite-engine/sprites/sprite.h>

#include "gameplay/Evaluation.h"
#include "gameplay/models/Song.h"
#include "objects/score/Feedback.h"
#include "objects/score/Grade.h"
#include "objects/score/Total.h"
#include "objects/ui/GradeBadge.h"

extern "C" {
#include "utils/gbfs/gbfs.h"
}

class DanceGradeScene : public Scene {
 public:
  std::unique_ptr<Evaluation> remoteEvaluation = NULL;

  DanceGradeScene(std::shared_ptr<GBAEngine> engine,
                  const GBFS_FILE* fs,
                  std::unique_ptr<Evaluation> evaluation,
                  bool isLastSong);

  std::vector<Background*> backgrounds() override;
  std::vector<Sprite*> sprites() override;

  void load() override;
  void tick(u16 keys) override;

 private:
  bool hasStarted = false;
  std::unique_ptr<Background> bg;
  const GBFS_FILE* fs;

  std::unique_ptr<Evaluation> evaluation;
  std::unique_ptr<Grade> grade;
  std::array<std::unique_ptr<Total>, FEEDBACK_TYPES_TOTAL> totals;
  std::unique_ptr<Total> maxComboTotal;
  bool isLastSong;

  std::array<std::unique_ptr<GradeBadge>, GAME_MAX_PLAYERS> miniGrades;
  std::array<std::unique_ptr<Total>, FEEDBACK_TYPES_TOTAL> remoteTotals;
  std::unique_ptr<Total> remoteMaxComboTotal;

  void setUpSpritesPalette();
  void setUpBackground();
  void finish();

  void printScore();
  std::string pointsToString(u32 points);
  u32 getMultiplayerPointsOf(Evaluation* evaluation, bool totalNotesMatches);

  void playSound();
  void processMultiplayerUpdates();
};

#endif  // DANCE_GRADE_SCENE_H
