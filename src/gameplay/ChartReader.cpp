#include "ChartReader.h"

#include <libgba-sprite-engine/gba/tonc_math.h>

#include <vector>

#include "debug/logDebugInfo.h"
#include "utils/MathUtils.h"

const u32 HOLD_ARROW_POOL_SIZE = 10;

ChartReader::ChartReader(Chart* chart,
                         ObjectPool<Arrow>* arrowPool,
                         Judge* judge) {
  this->chart = chart;
  this->arrowPool = arrowPool;
  this->judge = judge;

  holdArrows = std::unique_ptr<ObjectPool<HoldArrow>>{new ObjectPool<HoldArrow>(
      HOLD_ARROW_POOL_SIZE,
      [](u32 id) -> HoldArrow* { return new HoldArrow(id); })};
  for (u32 i = 0; i < ARROWS_TOTAL; i++)
    holdArrowFlags[i] = false;

  multiplier = ARROW_DEFAULT_MULTIPLIER;
  targetArrowTime = ARROW_TIME[multiplier];
  syncArrowTime();
};

bool ChartReader::update(int songMsecs) {
  int rythmMsecs = songMsecs - lastBpmChange;
  msecs = songMsecs - AUDIO_LAG - (int)stoppedMs + (int)warpedMs;

  if (targetArrowTime > arrowTime)
    arrowTime += min(targetArrowTime - arrowTime, MAX_ARROW_TIME_JUMP);
  else
    arrowTime -= min(arrowTime - targetArrowTime, MAX_ARROW_TIME_JUMP);

  if (hasStopped) {
    if (msecs >= stopStart + (int)stopLength) {
      hasStopped = false;
      stoppedMs += stopLength;
      msecs -= (int)stopLength;
    } else
      return processTicks(rythmMsecs, false);
  }

  processNextEvents();
  predictNoteEvents();
  orchestrateHoldArrows();
  return processTicks(rythmMsecs, true);
}

int ChartReader::getYFor(Arrow* arrow) {
  HoldArrow* holdArrow = arrow->getHoldArrow();

  int y;
  switch (arrow->type) {
    {
      case ArrowType::HOLD_HEAD_EXTRA_FILL:
      case ArrowType::HOLD_HEAD_ARROW:
        y = getHoldTopY(holdArrow) - ARROW_SIZE;
        if (arrow->type == ArrowType::HOLD_HEAD_ARROW)
          y -= HOLD_getFirstFillOffset(arrow->direction);
        break;
    }
    {
      case ArrowType::HOLD_TAIL_EXTRA_FILL:
      case ArrowType::HOLD_TAIL_ARROW:
        int topY = getHoldTopY(holdArrow);
        y = getHoldBottomY(holdArrow, topY);
        if (arrow->type == ArrowType::HOLD_TAIL_ARROW)
          y -= HOLD_getLastFillOffset(arrow->direction);
        break;
    }
    {
      case ArrowType::HOLD_FILL:
        if (holdArrow->currentFillOffset >= holdArrow->fillOffsetBottom) {
          y = -ARROW_SIZE;
          break;
        }
        int topY = getHoldTopY(holdArrow);
        y = topY + holdArrow->currentFillOffset;
        holdArrow->currentFillOffset += ARROW_SIZE;
        break;
    }
    default:
      y = getYFor(arrow->timestamp);
  };

  return min(y, ARROW_INITIAL_Y);
}

bool ChartReader::isHoldActive(ArrowDirection direction) {
  return holdArrowFlags[direction];
}

bool ChartReader::hasJustStopped() {
  return hasStopped && judge->isInsideTimingWindow(msecs - stopStart);
}

bool ChartReader::isAboutToResume() {
  if (!hasStopped)
    return false;

  return judge->isInsideTimingWindow((stopStart + (int)stopLength) - msecs);
}

int ChartReader::getYFor(int timestamp) {
  // arrowTime ms           -> ARROW_DISTANCE px
  // timeLeft ms             -> x = timeLeft * ARROW_DISTANCE / arrowTime
  int now = hasStopped ? stopStart : msecs;
  int timeLeft = timestamp - now;

  return min(ARROW_FINAL_Y + Div(timeLeft * ARROW_DISTANCE, arrowTime),
             ARROW_INITIAL_Y);
}

void ChartReader::processNextEvents() {
  processEvents(msecs, [this](EventType type, Event* event, bool* stop) {
    switch (type) {
      case EventType::SET_TEMPO:
        setScrollSpeed(event->extra);

        if (bpm > 0) {
          lastBpmChange = event->timestamp;
          subtick = 0;
        } else
          syncArrowTime();

        bpm = event->extra;
        return true;
      case EventType::SET_TICKCOUNT:
        tickCount = event->extra;
        lastTick = -1;
        subtick = 0;
        return true;
      case EventType::WARP:
        warpedMs += event->extra;
        msecs += event->extra;

        arrowPool->forEachActive([](Arrow* it) { it->scheduleDiscard(); });
        holdArrows->clear();

        *stop = true;
        return true;
      default:
        return false;
    }
  });
}

void ChartReader::predictNoteEvents() {
  processEvents(msecs + targetArrowTime,
                [this](EventType type, Event* event, bool* stop) {
                  if (msecs < event->timestamp) {
                    switch (type) {
                      case EventType::NOTE:
                        processUniqueNote(event);
                        return true;
                      case EventType::HOLD_START:
                        startHoldNote(event);
                        return true;
                      case EventType::HOLD_END:
                        endHoldNote(event);
                        return true;
                      default:
                        return false;
                    }
                  } else {
                    switch (type) {
                      case EventType::NOTE:
                        processUniqueNote(event);
                        return true;
                      case EventType::HOLD_START:
                        startHoldNote(event);
                        return true;
                      case EventType::HOLD_END:
                        endHoldNote(event);
                        return true;
                      case EventType::STOP:
                        hasStopped = true;
                        stopStart = event->timestamp;
                        stopLength = event->extra;
                        *stop = true;
                        return true;
                      default:
                        return false;
                    }
                  }
                });
}

void ChartReader::processUniqueNote(Event* event) {
  std::vector<Arrow*> arrows;

  forEachDirection(
      event->data, [event, &arrows, this](ArrowDirection direction) {
        arrowPool->create([event, &arrows, &direction](Arrow* it) {
          it->initialize(ArrowType::UNIQUE, direction, event->timestamp);
          arrows.push_back(it);
        });
      });

  connectArrows(arrows);
}

void ChartReader::startHoldNote(Event* event) {
  forEachDirection(event->data, [&event, this](ArrowDirection direction) {
    holdArrows->create([event, &direction, this](HoldArrow* holdArrow) {
      holdArrow->startTime = event->timestamp;
      holdArrow->endTime = 0;

      Arrow* head =
          arrowPool->create([event, &direction, &holdArrow, this](Arrow* head) {
            head->initializeHoldBorder(ArrowType::HOLD_HEAD_ARROW, direction,
                                       event->timestamp, holdArrow);
            arrowPool->createWithIdGreaterThan(
                [event, &direction, &holdArrow](Arrow* extraFill) {
                  extraFill->initializeHoldBorder(
                      ArrowType::HOLD_HEAD_EXTRA_FILL, direction,
                      event->timestamp, holdArrow);
                },
                head->id);
          });

      if (head == NULL) {
        holdArrows->discard(holdArrow->id);
        return;
      }

      holdArrow->direction = direction;
      holdArrow->fillOffsetSkip = 0;
      holdArrow->fillOffsetBottom = 0;
      holdArrow->activeFillCount = 0;
      holdArrow->lastPressTopY = HOLD_NULL;
    });
  });
}

void ChartReader::endHoldNote(Event* event) {
  forEachDirection(event->data, [&event, this](ArrowDirection direction) {
    withLastHoldArrow(direction, [&event, &direction,
                                  this](HoldArrow* holdArrow) {
      holdArrow->endTime = event->timestamp;

      arrowPool->create([&holdArrow, &event, &direction,
                         this](Arrow* extraFill) {
        Arrow* tail = arrowPool->createWithIdGreaterThan(
            [&holdArrow, &event, &extraFill, &direction, this](Arrow* tail) {
              extraFill->initializeHoldBorder(ArrowType::HOLD_TAIL_EXTRA_FILL,
                                              direction, event->timestamp,
                                              holdArrow);
              tail->initializeHoldBorder(ArrowType::HOLD_TAIL_ARROW, direction,
                                         event->timestamp, holdArrow);
            },
            extraFill->id);

        if (tail == NULL)
          arrowPool->discard(extraFill->id);
      });
    });
  });
}

void ChartReader::orchestrateHoldArrows() {
  holdArrows->forEachActive([this](HoldArrow* holdArrow) {
    holdArrow->resetState();

    if (msecs >= holdArrow->startTime)
      holdArrowFlags[holdArrow->direction] = true;

    if (holdArrow->hasEnded() && msecs >= holdArrow->endTime)
      holdArrowFlags[holdArrow->direction] = false;

    int topY = getHoldTopY(holdArrow);
    if (holdArrowFlags[holdArrow->direction] &&
        judge->isPressed(holdArrow->direction))
      holdArrow->updateLastPress(topY);
    int screenTopY =
        topY <= holdArrow->lastPressTopY && topY < 0
            ? HOLD_FILL_FINAL_Y -
                  min(holdArrow->lastPressTopY - topY, HOLD_FILL_FINAL_Y)
            : 0;
    int bottomY = (holdArrow->hasEnded() ? getHoldBottomY(holdArrow, topY)
                                         : ARROW_INITIAL_Y) +
                  ARROW_QUARTER_SIZE;

    if (bottomY < (int)-ARROW_SIZE * 2) {
      holdArrows->discard(holdArrow->id);
      return;
    }

    holdArrow->fillOffsetSkip = max(screenTopY - topY, screenTopY);
    holdArrow->fillOffsetBottom = bottomY - topY;
    u32 targetFills = MATH_divCeil(
        holdArrow->getFillSectionLength(topY, bottomY), ARROW_SIZE);

    while (holdArrow->activeFillCount < targetFills) {
      Arrow* fill = arrowPool->create([](Arrow* it) {});

      if (fill != NULL)
        fill->initializeHoldFill(holdArrow->direction, holdArrow);
      else
        break;

      holdArrow->activeFillCount++;
    }
  });
}

bool ChartReader::processTicks(int rythmMsecs, bool checkHoldArrows) {
  // 60000 ms           -> BPM beats
  // rythmMsecs ms      -> beat = millis * BPM / 60000
  int tick = Div(rythmMsecs * bpm * tickCount, MINUTE);
  bool hasChanged = tick != lastTick;

  if (hasChanged) {
    subtick++;
    if (subtick == tickCount)
      subtick = 0;

    if (checkHoldArrows) {
      u8 arrows = 0;
      bool canMiss = true;

      for (u32 i = 0; i < ARROWS_TOTAL; i++) {
        auto direction = static_cast<ArrowDirection>(i);

        withNextHoldArrow(direction, [&arrows, &canMiss, &direction,
                                      this](HoldArrow* holdArrow) {
          if (msecs >= holdArrow->startTime) {
            arrows |= EVENT_ARROW_MASKS[direction];

            if (msecs < holdArrow->startTime + HOLD_ARROW_TICK_OFFSET_MS)
              canMiss = false;
          }
        });
      }

      if (arrows > 0)
        judge->onHoldTick(arrows, canMiss);
    }
  }

  lastTick = tick;

  return hasChanged && subtick == 0 && bpm > 0;
}

void ChartReader::connectArrows(std::vector<Arrow*>& arrows) {
  if (arrows.size() <= 1)
    return;

  for (u32 i = 0; i < arrows.size(); i++) {
    arrows[i]->setSiblingId(arrows[i == arrows.size() - 1 ? 0 : i + 1]->id);
  }
}

int ChartReader::getHoldTopY(HoldArrow* holdArrow) {
  int firstFillOffset = HOLD_getFirstFillOffset(holdArrow->direction);

  int y = holdArrow->getHeadY(
      [holdArrow, this]() { return getYFor(holdArrow->startTime); });
  return y + firstFillOffset + ARROW_SIZE;
}

int ChartReader::getHoldBottomY(HoldArrow* holdArrow, int topY) {
  int lastFillOffset = HOLD_getLastFillOffset(holdArrow->direction);

  return holdArrow->getTailY([holdArrow, &topY, &lastFillOffset, this]() {
    return max(getYFor(holdArrow->endTime) + lastFillOffset, topY);
  });
}
