const Events = require("./Events");
const _ = require("lodash");

module.exports = class Chart {
  constructor(header, content) {
    this.header = header;
    this.content = content;
  }

  get events() {
    const timingEvents = this._getTimingEvents();
    const noteEvents = this._getNoteEvents(timingEvents);

    return this._applyOffset(
      this._applyAsyncStops(
        this._sort([...timingEvents, ...noteEvents], ["timestamp", "type"])
      )
    );
  }

  _getNoteEvents(timingEvents) {
    const measures = this._getMeasures();
    let cursor = 0;

    return _.flatMap(measures, (measure) => {
      // 1 measure = 1 whole note = BEAT_UNIT beats
      const lines = this._getMeasureLines(measure);
      const subdivision = 1 / lines.length;

      return _.flatMap(lines, (line) => {
        const bpm = this._getBpmByTimestamp(cursor, timingEvents);
        const wholeNoteLength = this._getWholeNoteLengthByBpm(bpm);
        const noteDuration = subdivision * wholeNoteLength;
        const timestamp = cursor;
        cursor += noteDuration;
        const eventsByType = this._getEventsByType(line);

        return _(eventsByType)
          .map(({ type, arrows }) => ({
            timestamp,
            type,
            arrows: _.range(0, 5).map((id) => _.includes(arrows, id)),
          }))
          .filter((it) => _.some(it.arrows))
          .reject(
            (it) =>
              it.type === Events.NOTE &&
              this._isInsideWarp(it.timestamp, timingEvents)
          )
          .value();
      });
    });
  }

  _getTimingEvents() {
    const segments = _([
      this.header.bpms.map((it) => ({ type: Events.SET_TEMPO, data: it })),
      this.header.tickcounts.map((it) => ({
        type: Events.SET_TICKCOUNT,
        data: it,
      })),
      [...this.header.stops, ...this.header.delays].map((it) => ({
        type: Events.STOP,
        data: it,
      })),
      this.header.scrolls.map((it) => ({ type: Events.STOP_ASYNC, data: it })),
      this.header.warps.map((it) => ({
        type: Events.WARP,
        data: it,
      })),
    ])
      .flatten()
      .sortBy(["data.key", "type"])
      .value();

    let currentTimestamp = 0;
    let currentBeat = 0;
    let currentBpm = this._getBpmByBeat(0);
    let warpStart = -1;
    let currentScrollEnabled = true;
    let currentScrollTimestamp = 0;

    return _(segments)
      .flatMap(({ type, data }) => {
        const beat = data.key;
        const beatLength = this._getBeatLengthByBpm(currentBpm);
        currentTimestamp += (beat - currentBeat) * beatLength;
        currentBeat = beat;
        currentBpm = this._getBpmByBeat(beat);
        const timestamp = currentTimestamp;

        if (data.value < 0) throw new Error("invalid_negative_timing_segment");
        const createWarp = () => {
          const length = timestamp - warpStart;
          if (length === 0) return null;

          return {
            timestamp: warpStart,
            type: Events.WARP,
            length: timestamp - warpStart,
          };
        };

        switch (type) {
          case Events.SET_TEMPO: {
            if (data.value > FAST_BPM_WARP) {
              if (warpStart === -1) warpStart = timestamp;
              return null;
            }

            const bpmChange = {
              timestamp,
              type,
              bpm: currentBpm,
            };

            if (warpStart > -1) {
              const warp = createWarp();
              warpStart = -1;
              return [warp, bpmChange];
            } else {
              return bpmChange;
            }
          }
          case Events.SET_TICKCOUNT: {
            return {
              timestamp,
              type,
              tickcount: data.value,
            };
          }
          case Events.STOP: {
            const length = data.value * SECOND;
            const stop = { timestamp, type, length };

            if (warpStart > -1) {
              const warp = createWarp();
              warpStart = timestamp;

              return [warp, stop];
            } else {
              return stop;
            }
          }
          case Events.STOP_ASYNC: {
            const scrollEnabled = data.value > 0;

            if (!currentScrollEnabled && scrollEnabled) {
              const length = timestamp - currentScrollTimestamp;
              currentScrollEnabled = true;

              return {
                timestamp: currentScrollTimestamp,
                type,
                length,
              };
            }

            if (currentScrollEnabled && !scrollEnabled) {
              currentScrollEnabled = false;
              currentScrollTimestamp = timestamp;
            }

            return null;
          }
          case Events.WARP: {
            const length = this._getRangeDuration(
              currentBeat,
              currentBeat + data.value
            );

            return {
              timestamp,
              type,
              length,
            };
          }
          default:
            throw new Error("unknown_timing_segment");
        }
      })
      .compact()
      .value();
  }

  _applyOffset(events) {
    return events.map((it) => ({
      ...it,
      timestamp: Math.round(this.header.offset + it.timestamp),
    }));
  }

  _applyAsyncStops(events) {
    let stoppedTime = 0;

    return this._sort(
      _(events)
        .map((it) => {
          if (it.type === Events.STOP_ASYNC) {
            const timestamp = it.timestamp - stoppedTime;
            stoppedTime += it.length;

            return {
              ...it,
              timestamp,
              type: Events.STOP,
            };
          }

          return {
            ...it,
            timestamp: it.timestamp - stoppedTime,
          };
        })
        .compact()
        .value()
    );
  }

  _sort(events) {
    return _.sortBy(events, ["timestamp", "type"]);
  }

  _getMeasures() {
    return this.content
      .split(",")
      .map((it) => it.trim())
      .filter(_.identity);
  }

  _getMeasureLines(measure) {
    return measure
      .split(/\r?\n/)
      .map((it) => it.replace(/\/\/.*/g, ""))
      .map((it) => it.trim())
      .filter((it) => NOTE_DATA.test(it));
  }

  _getEventsByType(line) {
    return _(line)
      .split("")
      .map((eventType, i) => ({ i, eventType }))
      .groupBy("eventType")
      .map((events, eventType) => ({
        type: Events.parse(eventType),
        arrows: events.map((it) => it.i),
      }))
      .values()
      .filter((it) => it.type != null)
      .value();
  }

  _getWholeNoteLengthByBpm(bpm) {
    return this._getBeatLengthByBpm(bpm) * BEAT_UNIT;
  }

  _getBeatLengthByBpm(bpm) {
    if (bpm === 0) return 0;
    return MINUTE / bpm;
  }

  _getBpmByBeat(beat) {
    const bpm = _.findLast(this._getFiniteBpms(), (bpm) => beat >= bpm.key);
    if (!bpm) return 0;

    return bpm.value;
  }

  _getBpmByTimestamp(timestamp, timingEvents) {
    const event = _.findLast(
      timingEvents,
      (event) => event.type === Events.SET_TEMPO && timestamp >= event.timestamp
    );

    return (event && event.bpm) || 0;
  }

  _getRangeDuration(startBeat, endBeat) {
    let length = 0;

    for (let beat = startBeat; beat < endBeat; beat += FUSE) {
      const bpm = this._getBpmByBeat(beat);
      length += this._getBeatLengthByBpm(bpm) * FUSE;
    }

    return length;
  }

  _getFiniteBpms() {
    return this.header.bpms.filter((it) => it.value <= FAST_BPM_WARP);
  }

  _isInsideWarp(timestamp, timingEvents) {
    return _.some(
      timingEvents,
      (event) =>
        event.type === Events.WARP &&
        timestamp >= event.timestamp &&
        timestamp < event.timestamp + event.length
    );
  }
};

const SECOND = 1000;
const MINUTE = 60 * SECOND;
const BEAT_UNIT = 4;
const FAST_BPM_WARP = 9999999;
const NOTE_DATA = /^\d\d\d\d\d$/;
const FUSE = 1 / 2 / 2 / 2 / 2;
const FIXED_POINT_PRECISION = 0xffffffff + 1;
