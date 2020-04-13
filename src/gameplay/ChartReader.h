#ifndef CHART_READER_H
#define CHART_READER_H

#include <functional>
#include <vector>

#include "gameplay/Judge.h"
#include "models/Song.h"
#include "objects/Arrow.h"
#include "utils/pool/ObjectPool.h"

typedef struct {
  bool isHolding;
  u32 startTime;
  u32 endTime;
  Arrow* lastFill;
} HoldState;

class ChartReader {
 public:
  bool hasStopped = false;

  ChartReader(Chart* chart, Judge* judge);

  bool update(u32 msecs, ObjectPool<Arrow>* arrowPool);

 private:
  Chart* chart;
  Judge* judge;
  u32 timeNeeded = 0;
  u32 eventIndex = 0;
  u32 bpm = 0;
  int lastBeat = -1;
  u32 lastBpmChange = 0;
  u32 tickCount = 4;
  int lastTick = 0;
  u32 stopStart = 0;
  u32 stopEnd = 0;
  std::array<HoldState, ARROWS_TOTAL> holdState;

  bool animateBpm(u32 msecs);
  void processNextEvent(u32 msecs, ObjectPool<Arrow>* arrowPool);
  void processUniqueNote(u8 data, ObjectPool<Arrow>* arrowPool);
  void startHoldNote(Event* event, ObjectPool<Arrow>* arrowPool);
  void endHoldNote(Event* event, ObjectPool<Arrow>* arrowPool);
  void processHoldArrows(u32 msecs, ObjectPool<Arrow>* arrowPool);
  void processHoldTicks(u32 msecs);
  void connectArrows(std::vector<Arrow*>& arrows);
  void forEachDirection(u8 data, std::function<void(ArrowDirection)> action);
};

#endif  // CHART_READER_H
