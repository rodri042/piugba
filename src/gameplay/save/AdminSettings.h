#ifndef ADMIN_SETTINGS_H
#define ADMIN_SETTINGS_H

#include <libgba-sprite-engine/gba/tonc_core.h>

enum ArcadeChartsOpts : u8 { SINGLE, DOUBLE };
enum IOBlinkOpts : u8 { IO_BLINK_OFF, IO_BLINK_ON_BEAT, IO_BLINK_ON_KEY };
enum SRAMBlinkOpts : u8 {
  SRAM_BLINK_OFF,
  SRAM_BLINK_ON_BEAT,
  SRAM_BLINK_ON_HIT
};
enum NavigationStyleOpts : u8 { PIU, GBA };

typedef struct __attribute__((__packed__)) {
  ArcadeChartsOpts arcadeCharts;
  bool rumble;
  IOBlinkOpts ioBlink;
  SRAMBlinkOpts sramBlink;
  NavigationStyleOpts navigationStyle;
  bool offsetEditingEnabled;
} AdminSettings;

#endif  // ADMIN_SETTINGS_H
