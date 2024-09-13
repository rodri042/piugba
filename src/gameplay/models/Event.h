#ifndef EVENT_H
#define EVENT_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#define EVENT_TYPE_BITS 3
#define EVENT_TYPE /*             */ 0b00000111
#define EVENT_ARROW_DOWNLEFT /*   */ 0b00001000
#define EVENT_ARROW_UPLEFT /*     */ 0b00010000
#define EVENT_ARROW_CENTER /*     */ 0b00100000
#define EVENT_ARROW_UPRIGHT /*    */ 0b01000000
#define EVENT_ARROW_DOWNRIGHT /*  */ 0b10000000

#define EVENT_HOLD_ARROW_DOWNLEFT /*          */ 0b0000000001
#define EVENT_HOLD_ARROW_UPLEFT /*            */ 0b0000000010
#define EVENT_HOLD_ARROW_CENTER /*            */ 0b0000000100
#define EVENT_HOLD_ARROW_UPRIGHT /*           */ 0b0000001000
#define EVENT_HOLD_ARROW_DOWNRIGHT /*         */ 0b0000010000
#define EVENT_HOLD_ARROW_DOWNLEFT_DOUBLE /*   */ 0b0000100000
#define EVENT_HOLD_ARROW_UPLEFT_DOUBLE /*     */ 0b0001000000
#define EVENT_HOLD_ARROW_CENTER_DOUBLE /*     */ 0b0010000000
#define EVENT_HOLD_ARROW_UPRIGHT_DOUBLE /*    */ 0b0100000000
#define EVENT_HOLD_ARROW_DOWNRIGHT_DOUBLE /*  */ 0b1000000000

#define GAME_MAX_PLAYERS 2

enum EventType {
  NOTE,
  HOLD_START,
  HOLD_END,
  SET_TEMPO,
  SET_TICKCOUNT,
  STOP,
  WARP,
};

const u8 EVENT_ARROW_MASKS[] = {EVENT_ARROW_DOWNLEFT, EVENT_ARROW_UPLEFT,
                                EVENT_ARROW_CENTER, EVENT_ARROW_UPRIGHT,
                                EVENT_ARROW_DOWNRIGHT};

const u16 EVENT_HOLD_ARROW_MASKS[] = {
    EVENT_HOLD_ARROW_DOWNLEFT,       EVENT_HOLD_ARROW_UPLEFT,
    EVENT_HOLD_ARROW_CENTER,         EVENT_HOLD_ARROW_UPRIGHT,
    EVENT_HOLD_ARROW_DOWNRIGHT,      EVENT_HOLD_ARROW_DOWNLEFT_DOUBLE,
    EVENT_HOLD_ARROW_UPLEFT_DOUBLE,  EVENT_HOLD_ARROW_CENTER_DOUBLE,
    EVENT_HOLD_ARROW_UPRIGHT_DOUBLE, EVENT_HOLD_ARROW_DOWNRIGHT_DOUBLE,
};

inline bool EVENT_HAS_DATA2(EventType event, bool isDouble) {
  return isDouble &&
         (event == EventType::NOTE || event == EventType::HOLD_START ||
          event == EventType::HOLD_END);
}

inline bool EVENT_HAS_PARAM(EventType event) {
  return event == EventType::HOLD_START || event == EventType::SET_TEMPO ||
         event == EventType::SET_TICKCOUNT || event == EventType::STOP ||
         event == EventType::WARP;
}

inline bool EVENT_HAS_PARAM2(EventType event) {
  return event == EventType::STOP || event == EventType::SET_TEMPO;
}

inline bool EVENT_HAS_PARAM3(EventType event) {
  return event == EventType::STOP || event == EventType::SET_TEMPO;
}

typedef struct {
  // (PIUS file)
  u32 timestampAndData;
  /*  {
        [bit 0]      is fake (only note types)
        [bits 1-23]  timestamp (signed int, ms)
        [bits 24-26] type (see EventType)
        [bits 27-31] data (5-bit array with the arrows)
      }
  */

  u32 param;
  u32 param2;
  u32 param3;
  // (params are not included in note-related events)

  u8 data2;  // another 5-bit arrow array (only present in double charts)

  // custom fields:
  bool handled[GAME_MAX_PLAYERS];

  inline int timestamp() {
    int timestamp = (timestampAndData >> 1) & 0x7fffff;
    if (timestamp & 0x400000)  // (sign extension)
      timestamp |= 0xff800000;
    return timestamp;
  }

  inline u8 data() { return (timestampAndData >> 24) & 0xff; }
  inline bool isFake() { return timestampAndData & 1; }

  u32 bpm() { return param & 0b1111111111111111111; }
  u32 beatDurationFrames() { return (param >> 20) & 0b111111111111; }
} Event;

#endif  // EVENT_H
