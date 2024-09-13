#ifndef CHART_READER_H
#define CHART_READER_H

#include <libgba-sprite-engine/gba/tonc_bios.h>
#include <libgba-sprite-engine/gba/tonc_math.h>

#include <vector>

#include "HoldArrow.h"
#include "Judge.h"
#include "TimingProvider.h"
#include "models/Song.h"
#include "objects/Arrow.h"
#include "utils/MathUtils.h"
#include "utils/PixelBlink.h"
#include "utils/pool/ObjectPool.h"

const u8 ARROW_MIRROR_INDEXES[] = {0, 1, 2, 3, 4, 3, 4, 2, 0, 1};
const u32 FRACUMUL_RATE_SCALE[] = {2018634629, 3178275798, 3736621547, 0,
                                   558345748,  1116691497, 2276332666};
// (0.47, 0.74, 0.87, 0, (1+)0.13, (1+)0.26, (1)+0.53)
// (empirical measure, checked multiple times)

class ChartReader : public TimingProvider {
 public:
  int customOffset = 0;
  int beatDurationFrames = -1;
  u32 beatFrame = 0;

  ChartReader(Chart* chart,
              u8 playerId,
              ObjectPool<Arrow>* arrowPool,
              Judge* judge,
              PixelBlink* pixelBlink,
              int audioLag,
              int globalOffset,
              u32 multiplier);

  bool update(int msecs);
  int getYFor(Arrow* arrow);

  inline u32 getMultiplier() { return multiplier; }
  inline bool setMultiplier(u32 multiplier) {
    u32 oldMultiplier = this->multiplier;
    this->multiplier =
        max(min(multiplier, ARROW_MAX_MULTIPLIER), ARROW_MIN_MULTIPLIER);
    if (didSetInitialBpm)
      syncScrollSpeed();
    else
      syncInitialScrollSpeed(this->multiplier);
    resetMaxArrowTimeJump();

    return this->multiplier != oldMultiplier;
  }

  inline void syncRate(u32 rateLevels, int rate) {
    this->currentRate = rate;
    rateAudioLag = scaleByRate(rateLevels, audioLag, rate);
    beatDurationFrames = -1;
  }

  inline int scaleByRate(u32 origin, int number, int rate) {
    if (rate > 0)
      number += MATH_fracumul(number, FRACUMUL_RATE_SCALE[origin + rate]);
    if (rate < 0)
      number = MATH_fracumul(number, FRACUMUL_RATE_SCALE[origin + rate]);

    return number;
  }

  inline int getJudgementOffset() {
    if (!hasStopped)
      return 0;

    return stopAsync ? -(msecs - stopStart)
                     : (isAboutToResume() ? -stopLength : 0);
  }

  inline bool isHoldActive(ArrowDirection direction) {
    return holdArrowStates[direction].isActive;
  }
  inline bool hasJustStopped() {
    return hasStopped && judge->isInsideTimingWindow(msecs - stopStart);
  }
  inline bool isAboutToResume() {
    if (!hasStopped)
      return false;

    return judge->isInsideTimingWindow((stopStart + (int)stopLength) - msecs);
  }

  inline void turnOffObjectPools() { holdArrows->turnOff(); }
  inline void turnOnObjectPools() { holdArrows->turnOn(); }

  template <typename DEBUG>
  void logDebugInfo();

 private:
  Chart* chart;
  u8 playerId;
  ObjectPool<Arrow>* arrowPool;
  Judge* judge;
  PixelBlink* pixelBlink;
  int audioLag;
  int rateAudioLag;
  u32 targetArrowTime;
  u32 multiplier;
  std::unique_ptr<ObjectPool<HoldArrow>> holdArrows;
  std::array<HoldArrowState, ARROWS_TOTAL * GAME_MAX_PLAYERS> holdArrowStates;
  u32 rhythmEventIndex = 0;
  u32 eventIndex = 0;
  u32 bpm = 0;
  u32 autoVelocityFactor = 1;
  u32 maxArrowTimeJump = MAX_ARROW_TIME_JUMP;
  int lastBpmChange = 0;
  u32 tickCount = 2;  // 8th notes
  bool didSetInitialBpm = false;
  int lastBeat = -1;
  int lastTick = -1;
  u32 stoppedMs = 0;
  u32 asyncStoppedMs = 0;
  u32 warpedMs = 0;
  int currentRate = 0;

  template <typename F>
  inline void processEvents(Event* events,
                            u32 count,
                            u32& index,
                            int targetMsecs,
                            F action) {
    u32 currentIndex = index;
    bool skipped = false;

    while (targetMsecs >= events[currentIndex].timestamp() &&
           currentIndex < count) {
      auto event = events + currentIndex;
      EventType type = static_cast<EventType>(event->data() & EVENT_TYPE);

      if (event->handled[playerId]) {
        currentIndex++;
        continue;
      }

      bool stop = false;
      event->handled[playerId] = action(type, event, &stop);
      currentIndex++;

      if (!event->handled[playerId])
        skipped = true;
      if (!skipped)
        index = currentIndex;
      if (stop)
        return;
    }
  }

  template <typename F>
  inline void withNextHoldArrow(ArrowDirection direction, F action) {
    HoldArrow* min = NULL;
    holdArrows->forEachActive(
        [&direction, &action, &min, this](HoldArrow* holdArrow) {
          if (holdArrow->direction != direction)
            return;

          if (min == NULL || holdArrow->startTime < min->startTime)
            min = holdArrow;
        });

    if (min != NULL)
      action(min);
  }

  template <typename F>
  inline void withLastHoldArrow(ArrowDirection direction, F action) {
    HoldArrow* max = NULL;
    holdArrows->forEachActive(
        [&direction, &action, &max, this](HoldArrow* holdArrow) {
          if (holdArrow->direction != direction)
            return;

          if (max == NULL || holdArrow->startTime > max->startTime)
            max = holdArrow;
        });

    if (max != NULL &&
        max->startTime == holdArrowStates[direction].lastStartTime)
      action(max);
  }

  template <typename F>
  inline void forEachDirection(u8 data, F action) {
    u32 start = GameState.mods.mirrorSteps * ARROWS_TOTAL;

    for (u32 i = 0; i < ARROWS_TOTAL; i++) {
      if (data & EVENT_ARROW_MASKS[i])
        action(static_cast<ArrowDirection>(ARROW_MIRROR_INDEXES[start + i]));
    }
  }

  void syncInitialScrollSpeed(u32 multiplier) {
    targetArrowTime = ARROW_TIME[multiplier];
    syncArrowTime();
  }
  inline void syncScrollSpeed() {
    bool isAutoVelocity =
        GameState.mods.speedHack == SpeedHackOpts::hAUTO_VELOCITY;
    bool isFixedVelocity =
        GameState.mods.speedHack == SpeedHackOpts::hFIXED_VELOCITY;

    if (isAutoVelocity || isFixedVelocity) {
      u32 userScrollBpm = AUTOVELOCITY_VALUES[multiplier - 1];
      if (isAutoVelocity && autoVelocityFactor != 1)
        userScrollBpm = MATH_fracumul(userScrollBpm, autoVelocityFactor);
      targetArrowTime =
          min(MATH_div(MINUTE * ARROW_SCROLL_LENGTH_BEATS, userScrollBpm),
              MAX_ARROW_TIME);
    } else {
      targetArrowTime = min(MATH_div(MINUTE * ARROW_SCROLL_LENGTH_BEATS,
                                     MATH_mul(scrollBpm, multiplier)),
                            MAX_ARROW_TIME);
    }
  }
  inline void syncArrowTime() { arrowTime = targetArrowTime; }
  inline void resetMaxArrowTimeJump() {
    maxArrowTimeJump = MAX_ARROW_TIME_JUMP;
  }

  int getYFor(int timestamp);
  void processRhythmEvents();
  void processNextEvents(int now);
  void processUniqueNote(int timestamp, u8 data, u8 param, bool isFake);
  void startHoldNote(int timestamp,
                     u8 data,
                     u32 length,
                     u8 offset,
                     bool isFake);
  void endHoldNote(int timestamp, u8 data, u8 offset = 0);
  void orchestrateHoldArrows();
  bool processTicks(int rhythmMsecs, bool checkHoldArrows);
  void connectArrows(std::vector<Arrow*>& arrows);
  int getFillTopY(HoldArrow* holdArrow);
  int getFillBottomY(HoldArrow* holdArrow, int topY);
  u8 getRandomStep(int timestamp, u8 data);
};

class CHART_DEBUG;

const u8 STOMP_SIZE_BY_DATA[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2,
                                 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3,
                                 3, 4, 2, 3, 3, 4, 3, 4, 4, 5};
const u8 DATA_BY_STOMP_SIZE[][10] = {{1, 2, 4, 8, 16},
                                     {3, 5, 6, 9, 10, 12, 17, 18, 20, 24},
                                     {7, 11, 13, 14, 19, 21, 22, 25, 26, 28},
                                     {15, 23, 27, 29, 30},
                                     {31}};
const u8 DATA_BY_STOMP_SIZE_COUNTS[] = {5, 10, 10, 5, 1};

#endif  // CHART_READER_H
