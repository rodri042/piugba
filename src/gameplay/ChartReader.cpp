#include "ChartReader.h"

#include <libgba-sprite-engine/gba/tonc_math.h>

#include <vector>

#include "debug/logDebugInfo.h"
#include "multiplayer/Syncer.h"

const u32 HOLD_ARROW_POOL_SIZE = 10;
const u32 RANDOM_STEPS_MAX_RETRIES = 5;
u8 RANDOM_STEPS_LAST_DATA = 0;

ChartReader::ChartReader(Chart* chart,
                         u8 playerId,
                         ObjectPool<Arrow>* arrowPool,
                         Judge* judge,
                         PixelBlink* pixelBlink,
                         int audioLag,
                         int globalOffset,
                         u32 multiplier) {
  this->chart = chart;
  this->playerId = playerId;
  this->arrowPool = arrowPool;
  this->judge = judge;
  this->pixelBlink = pixelBlink;
  this->audioLag = audioLag;
  this->rateAudioLag = audioLag;
  this->customOffset = chart->customOffset + globalOffset;

  holdArrows = std::unique_ptr<ObjectPool<HoldArrow>>{new ObjectPool<HoldArrow>(
      HOLD_ARROW_POOL_SIZE * (1 + chart->isDouble),
      [](u32 id) -> HoldArrow* { return new HoldArrow(id); })};
  for (u32 i = 0; i < ARROWS_GAME_TOTAL; i++) {
    holdArrowStates[i].isActive = false;
    holdArrowStates[i].currentStartTime = 0;
    holdArrowStates[i].lastStartTime = 0;
  }

  this->multiplier = multiplier;
  syncInitialScrollSpeed(multiplier);
};

CODE_IWRAM bool ChartReader::update(int songMsecs) {
  int rhythmMsecs = songMsecs - rateAudioLag + customOffset - lastBpmChange;
  msecs =
      songMsecs - rateAudioLag + customOffset - (int)stoppedMs + (int)warpedMs;

  MATH_approximate(&arrowTime, targetArrowTime, maxArrowTimeJump);

  if (hasStopped) {
    if (msecs >= stopStart + (int)stopLength) {
      hasStopped = false;
      stoppedMs += stopLength;
      msecs -= (int)stopLength;
      if (stopAsync)
        asyncStoppedMs = stopAsyncStoppedTime;
    } else {
      if (stopAsync)
        processRhythmEvents();
      processNextEvents(stopStart);
      orchestrateHoldArrows();
      return processTicks(rhythmMsecs, false);
    }
  }

  processRhythmEvents();
  processNextEvents(msecs);
  orchestrateHoldArrows();
  return processTicks(rhythmMsecs, true);
}

CODE_IWRAM int ChartReader::getYFor(Arrow* arrow) {
  HoldArrow* holdArrow = arrow->getHoldArrow();

  int y;
  switch (arrow->type) {
    case ArrowType::HOLD_HEAD: {
      y = getFillTopY(holdArrow) - HOLD_getFirstFillOffset(arrow->direction);
      break;
    }
    case ArrowType::HOLD_FILL: {
      bool isFirst = holdArrow->currentFillOffset == holdArrow->fillOffsetSkip;
      bool isOut = holdArrow->currentFillOffset >= holdArrow->fillOffsetBottom;
      bool isLast = !isOut && holdArrow->hasEndTime() &&
                    holdArrow->currentFillOffset + (int)ARROW_SIZE >=
                        holdArrow->fillOffsetBottom;

      if (isOut)
        return -ARROW_SIZE;

      if (isLast) {
        arrow->get()->setPriority(ARROW_LAYER_FRONT);
        y = getFillBottomY(holdArrow, getFillTopY(holdArrow)) - ARROW_SIZE;
      } else {
        arrow->get()->setPriority(isFirst ? ARROW_LAYER_MIDDLE
                                          : ARROW_LAYER_FRONT);
        y = getFillTopY(holdArrow) + holdArrow->currentFillOffset;
      }
      arrow->setIsLastFill(isLast);
      holdArrow->currentFillOffset += ARROW_SIZE;
      break;
    }
    case ArrowType::HOLD_TAIL: {
      y = getFillBottomY(holdArrow, getFillTopY(holdArrow)) -
          HOLD_getLastFillOffset(arrow->direction) - ARROW_SIZE;
      break;
    }
    default:
      y = getYFor(arrow->timestamp);
  };

  return min(y, ARROW_INITIAL_Y);
}

CODE_IWRAM int ChartReader::getYFor(int timestamp) {
  // arrowTime ms           -> ARROW_DISTANCE() px
  // timeLeft ms            -> x = timeLeft * ARROW_DISTANCE() / arrowTime
  int now = hasStopped ? stopStart : msecs;
  int timeLeft = timestamp - now;

  return min(ARROW_FINAL_Y() + MATH_div(timeLeft * ARROW_DISTANCE(), arrowTime),
             ARROW_INITIAL_Y);
}

CODE_IWRAM void ChartReader::processRhythmEvents() {
  processEvents(
      chart->rhythmEvents, chart->rhythmEventCount, rhythmEventIndex,
      msecs + (int)asyncStoppedMs,
      [this](EventType type, Event* event, bool* stop) {
        if (type == EventType::SET_TEMPO) {
          u32 oldBpm = bpm;
          u32 scrollChangeFrames = event->param2 & 0xffff;
          bpm = event->bpm();
          scrollBpm = event->param2 >> 16;
          if (event->param2 == INFINITY) {
            scrollChangeFrames = 0;
            scrollBpm = INFINITY;
          }
          autoVelocityFactor = event->param3;

          if (oldBpm != bpm) {
            lastBpmChange = event->timestamp();
            lastBeat = -1;
            lastTick = 0;
            beatDurationFrames =
                currentRate == 0 ? event->beatDurationFrames() : -1;
            beatFrame = 0;
            didSetInitialBpm = true;
          }

          syncScrollSpeed();

          if (oldBpm > 0) {
            if (scrollChangeFrames > 0) {
              u32 arrowTimeDiff = abs((int)targetArrowTime - (int)arrowTime);
              maxArrowTimeJump = arrowTimeDiff > 0
                                     ? Div(arrowTimeDiff, scrollChangeFrames)
                                     : MAX_ARROW_TIME_JUMP;
            } else {
              maxArrowTimeJump = MAX_ARROW_TIME_JUMP;
            }
          } else
            syncArrowTime();

          return true;
        } else if (type == EventType::SET_TICKCOUNT) {
          tickCount = event->param;
          lastTick = -1;

          return true;
        }
        return false;
      });
}

CODE_IWRAM void ChartReader::processNextEvents(int now) {
  processEvents(
      chart->events, chart->eventCount, eventIndex, now + arrowTime,
      [&now, this](EventType type, Event* event, bool* stop) {
        switch (type) {
          case EventType::NOTE: {
            if (arrowPool->isFull())
              return false;

            processUniqueNote(event->timestamp(), event->data(), event->data2,
                              event->isFake());
            return true;
          }
          case EventType::HOLD_START: {
            startHoldNote(event->timestamp(), event->data(), event->param, 0,
                          event->isFake());
            if (chart->isDouble)
              startHoldNote(event->timestamp(), event->data2, event->param,
                            ARROWS_TOTAL, event->isFake());
            return true;
          }
          case EventType::HOLD_END: {
            endHoldNote(event->timestamp(), event->data());
            if (chart->isDouble)
              endHoldNote(event->timestamp(), event->data2, ARROWS_TOTAL);
            return true;
          }
          default: {
            if (now < event->timestamp() || hasStopped)
              return false;
          }
        }

        switch (type) {
          case EventType::STOP: {
            hasStopped = true;
            stopStart = event->timestamp();
            stopLength = event->param;
            stopAsync = event->param2;
            stopAsyncStoppedTime = event->param3;

            return true;
          }
          case EventType::WARP: {
            warpedMs += event->param;
            msecs += event->param;
            lastWarpTime = msecs;
            if (playerId == syncer->getLocalPlayerId())
              pixelBlink->blink();

            return true;
          }
          default: {
            return false;
          }
        }
      });
}

CODE_IWRAM void ChartReader::processUniqueNote(int timestamp,
                                               u8 data,
                                               u8 param,
                                               bool isFake) {
  if (GameState.mods.randomSteps)
    data = getRandomStep(timestamp, data);

  std::vector<Arrow*> arrows;

  forEachDirection(data, [&timestamp, &arrows, &isFake,
                          this](ArrowDirection direction) {
    arrowPool->create([&timestamp, &arrows, &direction, &isFake,
                       this](Arrow* it) {
      it->initialize(ArrowType::UNIQUE, direction, playerId, timestamp, isFake);
      arrows.push_back(it);
    });
  });

  if (chart->isDouble)
    forEachDirection(
        param, [&timestamp, &arrows, &isFake, this](ArrowDirection direction) {
          direction = static_cast<ArrowDirection>(ARROWS_TOTAL + direction);

          arrowPool->create(
              [&timestamp, &arrows, &direction, &isFake, this](Arrow* it) {
                it->initialize(ArrowType::UNIQUE, direction, playerId,
                               timestamp, isFake);
                arrows.push_back(it);
              });
        });

  connectArrows(arrows);
}

CODE_IWRAM void ChartReader::startHoldNote(int timestamp,
                                           u8 data,
                                           u32 length,
                                           u8 offset,
                                           bool isFake) {
  if (GameState.mods.randomSteps) {
    // (when using random steps, hold notes are converted to unique notes)
    return processUniqueNote(timestamp, getRandomStep(timestamp, data), 0,
                             isFake);
  }

  forEachDirection(data, [&timestamp, &length, &offset, &isFake,
                          this](ArrowDirection direction) {
    direction = static_cast<ArrowDirection>(direction + offset);

    holdArrowStates[direction].lastStartTime = timestamp;

    holdArrows->create(
        [&timestamp, &direction, &length, &isFake, this](HoldArrow* holdArrow) {
          holdArrow->startTime = timestamp;
          holdArrow->endTime = !!length * (timestamp + length);

          Arrow* head = arrowPool->create([&timestamp, &direction, &holdArrow,
                                           &isFake, this](Arrow* head) {
            head->initializeHoldBorder(ArrowType::HOLD_HEAD, direction,
                                       playerId, timestamp, holdArrow, isFake);
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
          holdArrow->isFake = isFake;
        });
  });
}

CODE_IWRAM void ChartReader::endHoldNote(int timestamp, u8 data, u8 offset) {
  if (GameState.mods.randomSteps)
    return;

  forEachDirection(data, [&timestamp, &offset, this](ArrowDirection direction) {
    direction = static_cast<ArrowDirection>(direction + offset);

    withLastHoldArrow(direction, [&timestamp, &direction,
                                  this](HoldArrow* holdArrow) {
      holdArrow->endTime = timestamp;

      arrowPool->create([&holdArrow, &timestamp, &direction,
                         this](Arrow* tail) {
        tail->initializeHoldBorder(ArrowType::HOLD_TAIL, direction, playerId,
                                   timestamp, holdArrow, holdArrow->isFake);
      });
    });
  });
}

CODE_IWRAM void ChartReader::orchestrateHoldArrows() {
  holdArrows->forEachActive([this](HoldArrow* holdArrow) {
    ArrowDirection direction = holdArrow->direction;
    bool hasStarted = holdArrow->hasStarted(msecs);
    bool hasEnded = holdArrow->hasEnded(msecs);
    holdArrow->resetState();

    if (hasStarted) {
      holdArrowStates[direction].currentStartTime = holdArrow->startTime;
      holdArrowStates[direction].isActive = true;
    }

    if (hasEnded &&
        holdArrowStates[direction].currentStartTime == holdArrow->startTime)
      holdArrowStates[direction].isActive = false;

    int topY = getFillTopY(holdArrow);
    if (hasStarted && judge->isPressed(direction, playerId))
      holdArrow->updateLastPress(topY);
    int screenTopY =
        topY <= holdArrow->lastPressTopY
            ? HOLD_FILL_FINAL_Y() -
                  min(holdArrow->lastPressTopY - topY, HOLD_FILL_FINAL_Y())
            : 0;
    int bottomY = holdArrow->hasEndTime() ? getFillBottomY(holdArrow, topY)
                                          : ARROW_INITIAL_Y;

    if (bottomY < ARROW_OFFSCREEN_LIMIT) {
      holdArrows->discard(holdArrow->id);
      return;
    }

    holdArrow->fillOffsetSkip =
        max(screenTopY - topY, topY > 0 ? 0 : screenTopY);
    holdArrow->fillOffsetBottom = bottomY - topY;
    holdArrow->currentFillOffset = holdArrow->fillOffsetSkip;
    u32 fillSectionLength = holdArrow->getFillSectionLength(topY, bottomY);
    u32 targetFills = MATH_divCeil(fillSectionLength, ARROW_SIZE);

    if (hasEnded)
      return;

    while (holdArrow->activeFillCount < targetFills) {
      Arrow* fill = arrowPool->create([](Arrow* it) {});

      if (fill != NULL)
        fill->initializeHoldFill(direction, playerId, holdArrow);
      else
        break;

      holdArrow->activeFillCount++;
    }
  });
}

CODE_IWRAM bool ChartReader::processTicks(int rhythmMsecs,
                                          bool checkHoldArrows) {
  if (bpm == 0)
    return false;

  // 60000 ms           -> BPM beats
  // rhythmMsecs ms      -> beat = rhythmMsecs * BPM / 60000

  int beat = MATH_fracumul(rhythmMsecs * bpm, FRACUMUL_DIV_BY_MINUTE);
  bool isNewBeat = beat != lastBeat;
  lastBeat = beat;

  int tick =
      MATH_fracumul(rhythmMsecs * bpm * tickCount, FRACUMUL_DIV_BY_MINUTE);
  bool isNewTick = tick != lastTick;
  lastTick = tick;

  if (isNewTick) {
    if (checkHoldArrows) {
      u16 arrows = 0;
      bool isFake = false;
      bool canMiss = true;

      for (u32 i = 0; i < ARROWS_GAME_TOTAL; i++) {
        auto direction = static_cast<ArrowDirection>(i);

        withNextHoldArrow(direction, [&arrows, &canMiss, &direction, &isFake,
                                      this](HoldArrow* holdArrow) {
          if (holdArrow->isOccurring(msecs)) {
            arrows |= EVENT_HOLD_ARROW_MASKS[direction];
            isFake = holdArrow->isFake;

            if (msecs < holdArrow->startTime + HOLD_ARROW_TICK_OFFSET_MS ||
                msecs > holdArrow->endTime - HOLD_ARROW_TICK_OFFSET_MS)
              canMiss = false;
          }
        });
      }

      if (arrows > 0 && !isFake)
        judge->onHoldTick(arrows, playerId, canMiss);
    }
  }

  beatFrame++;

  return isNewBeat;
}

CODE_IWRAM void ChartReader::connectArrows(std::vector<Arrow*>& arrows) {
  if (arrows.size() <= 1)
    return;

  for (u32 i = 0; i < arrows.size(); i++) {
    arrows[i]->setSiblingId(arrows[i == arrows.size() - 1 ? 0 : i + 1]->id);
  }
}

CODE_IWRAM int ChartReader::getFillTopY(HoldArrow* holdArrow) {
  int firstFillOffset = HOLD_getFirstFillOffset(holdArrow->direction);

  int y = holdArrow->getHeadY(
      [holdArrow, this]() { return getYFor(holdArrow->startTime); });
  return y + firstFillOffset;
}

CODE_IWRAM int ChartReader::getFillBottomY(HoldArrow* holdArrow, int topY) {
  int lastFillOffset = HOLD_getLastFillOffset(holdArrow->direction);

  return holdArrow->getTailY([holdArrow, &topY, &lastFillOffset, this]() {
    return max(getYFor(holdArrow->endTime) + lastFillOffset, topY) + ARROW_SIZE;
  });
}

u8 ChartReader::getRandomStep(int timestamp, u8 data) {
  u32 retries = 0;
retry:
  retries++;

  u8 stompSize = STOMP_SIZE_BY_DATA[data >> EVENT_TYPE_BITS] - 1;
  data = DATA_BY_STOMP_SIZE[stompSize]
                           [qran_range(0, DATA_BY_STOMP_SIZE_COUNTS[stompSize])]
         << EVENT_TYPE_BITS;

  if (stompSize == 0) {
    bool isRepeatedNote = data == RANDOM_STEPS_LAST_DATA;
    if (isRepeatedNote && retries < RANDOM_STEPS_MAX_RETRIES)
      goto retry;

    RANDOM_STEPS_LAST_DATA = data;
  }

  return data;
}
