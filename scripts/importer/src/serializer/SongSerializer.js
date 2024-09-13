const Protocol = require("bin-protocol");
const Events = require("../parser/Events");
const DifficultyLevels = require("../parser/DifficultyLevels");
const Channels = require("../parser/Channels");
const Mods = require("../parser/Mods");
const _ = require("lodash");

let TMP = {};

module.exports = class SongSerializer {
  constructor(simfile) {
    this.simfile = simfile;
    this.protocol = new Protocol();

    this._defineTypes();
  }

  serialize() {
    TMP = {};

    const buffer = this.protocol.write();
    const { id, metadata, charts } = this.simfile;

    return buffer
      .UInt8(id)
      .String(metadata.title, TITLE_LEN)
      .String(metadata.artist, ARTIST_LEN)
      .UInt8(Channels[metadata.channel])
      .UInt32LE(metadata.lastMillisecond)
      .UInt32LE(metadata.sampleStart)
      .UInt32LE(metadata.sampleLength)
      .Int32LE(metadata.videoOffset)
      .Config(metadata.config)
      .ChartArray(charts).result;
  }

  _defineTypes() {
    this.protocol.define("String", {
      write: function (string, size) {
        const characters = string
          .substring(0, size - 1)
          .split("")
          .map((char) => char.charCodeAt(0));

        this.loop(characters, this.UInt8);
        const padding = size - characters.length;
        for (let i = 0; i < padding; i++) this.UInt8(0);
      },
    });

    this.protocol.define("Config", {
      write: function (config) {
        config.APPLY_TO.forEach((it) => this.UInt8(it));
        this.UInt8(config.IS_BOSS);

        _.forEach(Mods, (v, k) => {
          this.UInt8(v[config[k]]);
        });

        const hasMessage = config.MESSAGE !== "";
        this.UInt8(hasMessage);
        if (hasMessage) this.String(config.MESSAGE, MESSAGE_LEN);
      },
    });

    this.protocol.define("Chart", {
      write: function (chart) {
        TMP.chart = chart;

        const events = chart.events;
        const [rhythmEvents, normalEvents] = _.partition(
          events,
          (it) =>
            it.type === Events.SET_TEMPO || it.type === Events.SET_TICKCOUNT
        );

        const eventChunkSize = _.sumBy(events, (it) =>
          EVENT_SERIALIZERS.get(it).size(it)
        );

        this.UInt8(DifficultyLevels[chart.header.difficulty])
          .UInt8(chart.header.level)
          .UInt8(chart.header.variant.charCodeAt(0))
          .UInt8(chart.header.offsetLabel.charCodeAt(0))
          .UInt8(chart.header.isMultiplayer ? 2 : chart.header.isDouble ? 1 : 0)
          .UInt32LE(4 * 2 /* (eventCounts) */ + eventChunkSize)
          .EventArray(rhythmEvents)
          .EventArray(normalEvents);
      },
    });

    this.protocol.define("Event", {
      write: function (event) {
        const { write } = EVENT_SERIALIZERS.get(event);
        write.bind(this)(event);
      },
    });

    this.protocol.define("ChartArray", {
      write: function (charts) {
        this.UInt8(charts.length).loop(charts, this.Chart);
      },
    });

    this.protocol.define("EventArray", {
      write: function (events) {
        this.UInt32LE(events.length).loop(events, this.Event);
      },
    });
  }
};

const SERIALIZE_ARROWS = (arrows) =>
  _.range(0, 5).reduce(
    (accum, elem) => accum | (arrows[elem] ? ARROW_MASKS[elem] : 0),
    0
  );

const EVENT_SERIALIZERS = {
  get(event) {
    return this[event.type] || this.NOTES;
  },
  NOTES: {
    write: function (event) {
      const timestamp = event.timestamp;
      const data = SERIALIZE_ARROWS(event.arrows) | event.type;
      const timestampAndData = combine(timestamp, data, event.isFake);
      this.UInt32LE(timestampAndData);
      if (event.arrows2) this.UInt8(SERIALIZE_ARROWS(event.arrows2));
      if (event.type === Events.HOLD_START)
        this.UInt32LE(event.length != null ? normalizeInt(event.length) : 0);
    },
    size: (event) =>
      4 + (event.arrows2 ? 1 : 0) + (event.type === Events.HOLD_START ? 4 : 0),
  },
  [Events.SET_TEMPO]: {
    write: function (event) {
      const chart = TMP?.chart;
      if (chart == null) throw new Error("serializer_chart_not_found");

      function findDominantScrollBpm(events) {
        if (events.length === 0) return 0;

        const finalTimestamp = _.last(chart.events).timestamp;
        let bpmDurations = {};
        events.forEach((item, index) => {
          const nextTimestamp =
            index < events.length - 1
              ? events[index + 1].timestamp
              : finalTimestamp;
          const length = nextTimestamp - item.timestamp;

          if (!bpmDurations[item.scrollBpm]) bpmDurations[item.scrollBpm] = 0;
          bpmDurations[item.scrollBpm] += length;
        });

        let maxDuration = 0;
        let dominantBpm = null;

        _.forEach(bpmDurations, (duration, bpm) => {
          if (duration > maxDuration) {
            maxDuration = duration;
            dominantBpm = parseInt(bpm);
          }
        });

        return dominantBpm || 0;
      }

      const dominantScrollBpm = findDominantScrollBpm(
        chart.events.filter((it) => it.type === Events.SET_TEMPO)
      );
      const autoVelocityFactor = Math.max(
        event.scrollBpm / dominantScrollBpm,
        0.25
      );

      const scrollBpm = normalizeInt(event.scrollBpm);
      let scrollChangeFrames = normalizeInt(event.scrollChangeFrames);
      if (scrollChangeFrames === INFINITY) scrollChangeFrames = 0;
      const composedScrollBpm = new Uint32Array(1);
      composedScrollBpm[0] =
        scrollBpm === INFINITY
          ? scrollBpm
          : (((scrollBpm & 0xffff) << 16) | scrollChangeFrames) & 0xffffffff;

      this.UInt32LE(combine(event.timestamp, event.type))
        .UInt32LE(buildBPM(normalizeInt(event.bpm)))
        .UInt32LE(composedScrollBpm[0])
        .UInt32LE(
          autoVelocityFactor >= 1 || autoVelocityFactor === 0
            ? 1
            : INFINITY * autoVelocityFactor
        );
    },
    size: () => 4 + 4 + 4 + 4,
  },
  [Events.SET_TICKCOUNT]: {
    write: function (event) {
      this.UInt32LE(combine(event.timestamp, event.type)).UInt32LE(
        normalizeInt(event.tickcount)
      );
    },
    size: () => 4 + 4,
  },
  [Events.STOP]: {
    write: function (event) {
      this.UInt32LE(combine(event.timestamp, event.type))
        .UInt32LE(normalizeInt(event.length))
        .UInt32LE(event.async ? 1 : 0)
        .UInt32LE(event.async ? normalizeInt(event.asyncStoppedTime) : 0);
    },
    size: () => 4 + 4 + 4 + 4,
  },
  [Events.WARP]: {
    write: function (event) {
      this.UInt32LE(combine(event.timestamp, event.type)).UInt32LE(
        normalizeInt(event.length)
      );
    },
    size: () => 4 + 4,
  },
};

const combine = (timestamp, data, isFake = 0) => {
  const value = new Uint32Array(1);
  value[0] = normalizeInt(timestamp);
  value[0] = +!!isFake | ((value[0] & 0x7fffff) << 1) | ((data & 0xff) << 24);
  return value[0];
};

const buildBPM = (normalizedBpm) => {
  if (normalizedBpm > MAX_BPM) normalizedBpm = MAX_BPM;

  let beatDurationFrames = Math.round(FRAMES_PER_MINUTE / normalizedBpm);
  if (beatDurationFrames > MAX_BEAT_DURATION_FRAMES)
    beatDurationFrames = MAX_BEAT_DURATION_FRAMES;

  const serialized = new Uint32Array(1);
  serialized[0] =
    ((beatDurationFrames & MAX_BEAT_DURATION_FRAMES) << 20) |
    (normalizedBpm & MAX_BPM);

  return serialized[0];
};

const normalizeInt = (number) => {
  if (number === Infinity || number > INFINITY) return INFINITY;
  const array = new Uint32Array(1);
  array[0] = Math.round(number);
  return array[0];
};

const TITLE_LEN = 30 + 1; // +1 = \0;
const ARTIST_LEN = 26 + 1; // +1 = \0;
const MESSAGE_LEN = 25 + 2 + 25 + 2 + 25 + 2 + 25 + 1; // +2 = \r\n ; +1 = \0
const INFINITY = 0xffffffff;
const MAX_BPM = 0b11111111111111111111;
const MAX_BEAT_DURATION_FRAMES = 0b111111111111;
const FRAMES_PER_MINUTE = 60 * 60;

const ARROW_MASKS = [
  0b00001000,
  0b00010000,
  0b00100000,
  0b01000000,
  0b10000000,
];
