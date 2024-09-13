#include "Score.h"

#include <libgba-sprite-engine/gba/tonc_math.h>

#include "gameplay/save/State.h"

const int LIFE_DIFFS[] = {1, 1, 0, -6, -12};
const int POINT_DIFFS[] = {1000, 500, 100, -200, -500};

const u32 MIN_VISIBLE_COMBO = 4;
const u32 MAX_SCALE_STEP = 5;

const u32 SCALE_STEPS[] = {256, 233, 213, 197, 171, 205};

Score::Score(LifeBar* lifeBar, u8 playerId, bool isVs, bool isLocal) {
  lifeBar->setLife(life);
  this->lifeBar = lifeBar;
  this->playerId = playerId;

  feedback = std::unique_ptr<Feedback>{new Feedback(playerId)};
  combo = std::unique_ptr<Combo>{new Combo(playerId)};

  for (u32 i = 0; i < counters.size(); i++)
    counters[i] = 0;

  if (GameState.mods.stageBreak == StageBreakOpts::sSUDDEN_DEATH)
    lifeBar->setLife(MIN_LIFE);

  if (isLocal) {
    feedback->get()->setDoubleSize(true);
    feedback->get()->setAffineId(AFFINE_BASE + playerId);
    combo->getTitle()->get()->setDoubleSize(true);
    combo->getTitle()->get()->setAffineId(AFFINE_BASE + playerId);
    if (!isVs) {
      for (u32 i = 0; i < COMBO_DIGITS; i++) {
        combo->getDigits()->at(i)->get()->setDoubleSize(true);
        combo->getDigits()->at(i)->get()->setAffineId(AFFINE_BASE + playerId);
      }
    }
  }
}

bool Score::update(FeedbackType feedbackType, bool isLong) {
  if (isLong)
    longNotes++;

  feedback->setType(feedbackType);
  feedback->show();

  bool isAlive = updateLife(feedbackType);
  updateCombo(feedbackType);
  updateCounters(feedbackType);
  updatePoints(feedbackType);

  scaleStep = MAX_SCALE_STEP;

  return isAlive;
}

std::unique_ptr<Evaluation> Score::evaluate() {
  auto evaluation = std::unique_ptr<Evaluation>{new Evaluation()};
  evaluation->perfects = counters[FeedbackType::PERFECT];
  evaluation->greats = counters[FeedbackType::GREAT];
  evaluation->goods = counters[FeedbackType::GOOD];
  evaluation->bads = counters[FeedbackType::BAD];
  evaluation->misses = counters[FeedbackType::MISS];
  evaluation->maxCombo = maxCombo;
  evaluation->points = points;
  evaluation->longNotes = longNotes;
  evaluation->percent = getPercent();

  return evaluation;
}

void Score::relocate() {
  feedback->relocate();
  combo->relocate();
}

void Score::tick() {
  feedback->tick();
  combo->tick();

  EFFECT_setScale(playerId, SCALE_STEPS[scaleStep], SCALE_STEPS[scaleStep]);
  scaleStep = max(scaleStep - 1, 0);
}

bool Score::updateLife(FeedbackType feedbackType) {
  if (lifeBar->getIsDead())
    return true;
  if (GameState.mods.stageBreak == StageBreakOpts::sSUDDEN_DEATH)
    return feedbackType != FeedbackType::MISS;
  if (GameState.mode == GameMode::DEATH_MIX && !GameState.isShuffleMode &&
      feedbackType != FeedbackType::MISS)
    return true;

  u32 bonus = 0;
  if (feedbackType == FeedbackType::PERFECT) {
    halfLifeBonus = !halfLifeBonus;
    if (halfLifeBonus)
      bonus = 1;
  }

  life = min(life + LIFE_DIFFS[feedbackType] + bonus, MAX_LIFE);
  bool isAlive = life >= MIN_LIFE;
  if (!isAlive)
    life = MIN_LIFE;
  lifeBar->setLife(life);

  return isAlive;
}

void Score::updateCombo(FeedbackType feedbackType) {
  u32 value = combo->getValue();

  switch (feedbackType) {
    case FeedbackType::MISS:
      if (hasMissCombo)
        value++;
      else {
        value = 1;
        hasMissCombo = true;
      }
      break;
    case FeedbackType::BAD:
      hasMissCombo = false;
      value = 0;
      break;
    case FeedbackType::GOOD:
      if (hasMissCombo) {
        hasMissCombo = false;
        value = 0;
      }
      break;
    case FeedbackType::GREAT:
    case FeedbackType::PERFECT:
      if (hasMissCombo) {
        hasMissCombo = false;
        value = 0;
      }

      value = value + 1;
      break;
    default:
      break;
  }

  if (!hasMissCombo && value > maxCombo)
    maxCombo = value;

  combo->setValue(value * (hasMissCombo ? -1 : 1));
  if (value >= MIN_VISIBLE_COMBO)
    combo->show();
  else
    combo->hide();
}

void Score::updateCounters(FeedbackType feedbackType) {
  counters[feedbackType]++;
}

void Score::updatePoints(FeedbackType feedbackType) {
  if (lifeBar->getIsDead())
    return;

  points = max(points + POINT_DIFFS[feedbackType], 0);

  bool isPerfectOrGreat = feedbackType == FeedbackType::PERFECT ||
                          feedbackType == FeedbackType::GREAT;

  if (combo->getValue() > 50 && isPerfectOrGreat)
    points += 1000;
}
