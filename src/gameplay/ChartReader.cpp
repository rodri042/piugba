#include "ChartReader.h"

#include <libgba-sprite-engine/gba/tonc_math.h>

#include <vector>

/*
  x = x0 + v * t
  ARROW_CORNER_MARGIN_Y = GBA_SCREEN_HEIGHT + ARROW_SPEED * t
  t = (ARROW_CORNER_MARGIN_Y - GBA_SCREEN_HEIGHT) px / ARROW_SPEED px/frame
  t = (15 - 160) / 3 = -48.33 frames * 16.73322954 ms/frame = -808,77 frames
  => Look-up table for speeds 0, 1, 2, 3 and 4 px/frame
*/
const u32 TIME_NEEDED[] = {0, 2426, 1213, 809, 607};
const int AUDIO_LAG = 170;

ChartReader::ChartReader(Chart* chart) {
  this->chart = chart;
  for (u32 i = 0; i < ARROWS_TOTAL; i++)
    holdState[i] = false;

  timeNeeded = TIME_NEEDED[ARROW_SPEED];
};

bool ChartReader::update(u32 msecs, ObjectPool<Arrow>* arrowPool) {
  processNextEvent(msecs, arrowPool);
  return animateBpm(msecs);
};

bool ChartReader::animateBpm(u32 msecs) {
  int msecsWithOffset = (msecs - lastBpmChange) - chart->offset;

  // 60000 ms           -> BPM beats
  // msecsWithOffset ms -> x = millis * BPM / 60000
  int beat = Div(msecsWithOffset * bpm, 60000);
  bool hasChanged = beat != lastBeat;
  lastBeat = beat;

  return hasChanged;
}

void ChartReader::processNextEvent(u32 msecs, ObjectPool<Arrow>* arrowPool) {
  msecs = max((int)msecs - AUDIO_LAG, 0);
  u32 currentIndex = eventIndex;
  int anticipation = getAnticipationFor(msecs);
  bool skipped = false;

  while ((anticipation == -1
              ? msecs >= chart->events[currentIndex].timestamp
              : (int)msecs >= (int)chart->events[currentIndex].timestamp -
                                  anticipation) &&
         currentIndex < chart->eventCount) {
    auto event = chart->events + currentIndex;
    EventType type = static_cast<EventType>((event->data & EVENT_TYPE));
    bool handled = true;

    if (event->handled) {
      currentIndex++;
      continue;
    }

    if (anticipation != -1 && msecs < event->timestamp) {
      // events with anticipation

      std::vector<Arrow*> arrows;
      switch (type) {
        case EventType::NOTE:
          processUniqueNote(event->data, arrows, arrowPool);
          break;
        case EventType::HOLD_START:
          startHoldNote(event->data, arrows, arrowPool);
          break;
        case EventType::HOLD_TICK:
          // TODO: Process tick
          break;
        case EventType::HOLD_END:
          endHoldNote(event->data, arrows, arrowPool);
          break;
        default:
          handled = false;
          skipped = true;
          break;
      }
      connectArrows(arrows);
    } else {
      // exact events

      if (hasStopped && msecs >= stopEnd)  // TODO: Borrar stopStart & stopEnd
        hasStopped = false;

      switch (type) {
        case EventType::SET_TEMPO:
          bpm = event->extra;
          lastBeat = -1;
          lastBpmChange = msecs;
          break;
        case EventType::STOP:
          hasStopped = true;
          stopStart = msecs;
          stopEnd = msecs + event->extra;
          arrowPool->forEachActive(
              [&event](Arrow* it) { it->freeze(event->extra); });
          break;
        default:
          break;
      }
    }

    event->handled = handled;
    currentIndex++;
    if (!skipped)
      eventIndex++;
  }
}

void ChartReader::processUniqueNote(u8 data,
                                    std::vector<Arrow*>& arrows,
                                    ObjectPool<Arrow>* arrowPool) {
  forEachDirection(data, [&arrowPool, &arrows](ArrowDirection direction) {
    arrows.push_back(arrowPool->create([&direction](Arrow* it) {
      it->initialize(ArrowType::UNIQUE, direction);
    }));
  });
}

void ChartReader::startHoldNote(u8 data,
                                std::vector<Arrow*>& arrows,
                                ObjectPool<Arrow>* arrowPool) {
  return;  // TODO: IMPLEMENT
  forEachDirection(data, [&arrowPool, &arrows, this](ArrowDirection direction) {
    holdState[(int)direction] = true;

    arrows.push_back(arrowPool->create([&direction](Arrow* it) {
      it->initialize(ArrowType::HOLD_HEAD, direction);
    }));
  });
}

void ChartReader::endHoldNote(u8 data,
                              std::vector<Arrow*>& arrows,
                              ObjectPool<Arrow>* arrowPool) {
  return;  // TODO: IMPLEMENT
  forEachDirection(data, [&arrowPool, &arrows, this](ArrowDirection direction) {
    holdState[(int)direction] = false;

    arrows.push_back(arrowPool->create([&direction](Arrow* it) {
      it->initialize(ArrowType::HOLD_TAIL, direction);
    }));
  });
}

void ChartReader::connectArrows(std::vector<Arrow*>& arrows) {
  if (arrows.size() <= 1)
    return;

  for (u32 i = 0; i < arrows.size(); i++) {
    arrows[i]->setSiblingId(arrows[i == arrows.size() - 1 ? 0 : i + 1]->id);
  }
}

void ChartReader::forEachDirection(u8 data,
                                   std::function<void(ArrowDirection)> action) {
  if (data & EVENT_ARROW_DOWNLEFT)
    action(ArrowDirection::DOWNLEFT);

  if (data & EVENT_ARROW_UPLEFT)
    action(ArrowDirection::UPLEFT);

  if (data & EVENT_ARROW_CENTER)
    action(ArrowDirection::CENTER);

  if (data & EVENT_ARROW_UPRIGHT)
    action(ArrowDirection::UPRIGHT);

  if (data & EVENT_ARROW_DOWNRIGHT)
    action(ArrowDirection::DOWNRIGHT);
}

int ChartReader::getAnticipationFor(u32 msecs) {
  u32 stoppedTime = 0;
  u32 targetMsecs = msecs + timeNeeded;
  u32 currentIndex = eventIndex;

  // if (hasStopped)
  //   return -1;

  while (targetMsecs >= chart->events[currentIndex].timestamp &&
         currentIndex < chart->eventCount) {
    auto event = chart->events + currentIndex;
    EventType type = static_cast<EventType>((event->data & EVENT_TYPE));

    if (event->handled) {
      currentIndex++;
      continue;
    }

    if (type == EventType::STOP) {
      u32 stopStart = event->timestamp;
      u32 stopEnd = stopStart + event->extra;
      u32 lostTime = min(stopEnd, targetMsecs) - stopStart;

      targetMsecs += lostTime;
      stoppedTime += lostTime;
    }

    currentIndex++;
  }

  return (int)(timeNeeded + stoppedTime);
}
