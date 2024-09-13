#ifndef SCORE_H
#define SCORE_H

#include <libgba-sprite-engine/sprites/sprite.h>

#include <array>
#include <vector>

#include "Feedback.h"
#include "combo/Combo.h"
#include "gameplay/Evaluation.h"
#include "objects/LifeBar.h"
#include "utils/MathUtils.h"

const u32 FRACUMUL_0_05 = 214748365;
const u32 FRACUMUL_0_20 = 858993459;
const u32 FRACUMUL_0_45 = 1932735283;
const u32 FRACUMUL_0_60 = 2576980377;
const u32 FRACUMUL_0_90 = 3865470565;

class Score {
 public:
  Score(LifeBar* lifeBar, u8 playerId, bool isVs, bool isLocal);

  inline u32 getPoints() { return points; }
  inline u32 getPercent() {
    auto perfects = counters[FeedbackType::PERFECT];
    auto greats = counters[FeedbackType::GREAT];
    auto goods = counters[FeedbackType::GOOD];
    auto bads = counters[FeedbackType::BAD];
    auto misses = counters[FeedbackType::MISS];
    auto totalNotes = perfects + greats + goods + bads + misses;
    return Div(max(perfects + MATH_fracumul(perfects, FRACUMUL_0_20) +
                       MATH_fracumul(greats, FRACUMUL_0_90) +
                       MATH_fracumul(goods, FRACUMUL_0_60) -
                       MATH_fracumul(bads, FRACUMUL_0_45) -
                       MATH_fracumul(misses, FRACUMUL_0_90) -
                       MATH_fracumul(longNotes, FRACUMUL_0_20) +
                       MATH_fracumul(maxCombo, FRACUMUL_0_05),
                   0) *
                   100,
               totalNotes);
  }

  bool update(FeedbackType feedbackType, bool isLong);
  std::unique_ptr<Evaluation> evaluate();
  void relocate();

  void tick();

  inline void die() { lifeBar->die(); }
  inline Feedback* getFeedback() { return feedback.get(); }
  inline Combo* getCombo() { return combo.get(); }

  void log(int number) {
    combo->setValue(number);
    combo->show();
  }

  // ---
  inline int getLife() { return life; }
  inline bool getHasMissCombo() { return hasMissCombo; }
  inline bool getHalfLifeBonus() { return halfLifeBonus; }
  inline u32 getMaxCombo() { return maxCombo; }
  inline std::array<u32, FEEDBACK_TYPES_TOTAL> getCounters() {
    return counters;
  }
  inline u32 getLongNotes() { return longNotes; }
  inline void setLife(int life) {
    this->life = life;
    lifeBar->setLife(life);
  }
  inline void setHasMissCombo(bool hasMissCombo) {
    this->hasMissCombo = hasMissCombo;
  }
  inline void setHalfLifeBonus(bool halfLifeBonus) {
    this->halfLifeBonus = halfLifeBonus;
  }
  inline void setMaxCombo(u32 maxCombo) { this->maxCombo = maxCombo; }
  inline void setCounters(std::array<u32, FEEDBACK_TYPES_TOTAL> counters) {
    for (u32 i = 0; i < FEEDBACK_TYPES_TOTAL; i++)
      this->counters[i] = counters[i];
  }
  inline void setPoints(u32 points) { this->points = points; }
  inline void setLongNotes(u32 longNotes) { this->longNotes = longNotes; }
  // ---

 private:
  std::unique_ptr<Feedback> feedback;
  std::unique_ptr<Combo> combo;
  LifeBar* lifeBar;
  u8 playerId;

  int life = INITIAL_LIFE;
  bool hasMissCombo = false;
  bool halfLifeBonus = false;
  u32 maxCombo = 0;
  std::array<u32, FEEDBACK_TYPES_TOTAL> counters;
  u32 points = 0;
  u32 longNotes = 0;
  u32 scaleStep = 0;

  bool updateLife(FeedbackType feedbackType);
  void updateCombo(FeedbackType feedbackType);
  void updateCounters(FeedbackType feedbackType);
  void updatePoints(FeedbackType feedbackType);
};

#endif  // SCORE_H
