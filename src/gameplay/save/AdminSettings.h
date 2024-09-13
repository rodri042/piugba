#ifndef ADMIN_SETTINGS_H
#define ADMIN_SETTINGS_H

#include <libgba-sprite-engine/gba/tonc_core.h>

enum ArcadeChartsOpts : u8 { SINGLE, DOUBLE };
enum RumbleOpts : u8 {
  rNO_RUMBLE,
  rCARTRIDGE,
  rSC_PIN,
};
enum IOBlinkOpts : u8 { IO_BLINK_OFF, IO_BLINK_ON_BEAT, IO_BLINK_ON_KEY };
enum SRAMBlinkOpts : u8 {
  SRAM_BLINK_OFF,
  SRAM_BLINK_ON_BEAT,
  SRAM_BLINK_ON_HIT
};
enum NavigationStyleOpts : u8 { PIU, GBA };
enum HQModeOpts : u8 { dOFF, dACTIVATING, dACTIVE, dVIDEO_ONLY, dAUDIO_ONLY };

#define RUMBLE_PREROLL(OPTS) (((OPTS) >> 4) & 0b1111)
#define RUMBLE_IDLE(OPTS) (((OPTS) >> 0) & 0b1111)
#define RUMBLE_OPTS_BUILD(PREROLL, IDLE) \
  ((((PREROLL) & 0b1111) << 4) | (((IDLE) & 0b1111) << 0))

typedef struct __attribute__((__packed__)) {
  ArcadeChartsOpts arcadeCharts;
  RumbleOpts rumble;
  IOBlinkOpts ioBlink;
  SRAMBlinkOpts sramBlink;
  NavigationStyleOpts navigationStyle;
  bool offsetEditingEnabled;
  HQModeOpts hqMode;
  bool ewramOverclock;
  bool ps2Input;
  u8 rumbleFrames;
  u8 rumbleOpts;
} AdminSettings;

#endif  // ADMIN_SETTINGS_H
