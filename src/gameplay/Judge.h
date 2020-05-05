#ifndef JUDGE_H
#define JUDGE_H

#include <functional>

#include "objects/Arrow.h"
#include "objects/ArrowHolder.h"
#include "objects/score/Score.h"
#include "utils/pool/ObjectPool.h"

const u32 OFFSET_MISS = 9;
const u32 OFFSET_BAD = 7;
const u32 OFFSET_GOOD = 5;
const u32 OFFSET_GREAT = 3;

class Judge {
 public:
  Judge(ObjectPool<Arrow>* arrowPool,
        std::vector<std::unique_ptr<ArrowHolder>>* arrowHolders,
        Score* score,
        std::function<void()> onStageBreak);

  void onPress(Arrow* arrow);
  void onOut(Arrow* arrow);
  void onHoldTick(u8 arrows, bool canMiss);

 private:
  ObjectPool<Arrow>* arrowPool;
  std::vector<std::unique_ptr<ArrowHolder>>* arrowHolders;
  Score* score;
  std::function<void()> onStageBreak;

  FeedbackType onResult(Arrow* arrow, FeedbackType partialResult);
  void updateScore(FeedbackType result);
};

#endif  // JUDGE_H
