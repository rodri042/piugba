#ifndef DEBUG_TOOLS_H
#define DEBUG_TOOLS_H

#include <stdarg.h>
#include <stdio.h>
#include "utils/SceneUtils.h"

extern "C" {
#include "player/player.h"
}

#ifndef ENV_DEBUG
// Defined in Makefile
#define ENV_DEBUG false
#endif

#ifndef ENV_DEVELOPMENT
// Defined in Makefile
#define ENV_DEVELOPMENT false
#endif

#ifndef ENV_ARCADE
// Defined in Makefile
#define ENV_ARCADE false
#endif

#define STRESSTEST_MODE false
#define TIMINGTEST_MODE false
#define IFSTRESSTEST if (STRESSTEST_MODE)
#define IFTIMINGTEST if (TIMINGTEST_MODE)
#define DSTR(EXP) std::to_string((EXP))
#define LOGN(NUM, LINE) (LOGSTR(DSTR(NUM).c_str(), LINE))
#define LOGSTR(STR, LINE) (TextStream::instance().setText(STR, 1 + LINE, 12))

#define DEBULOG(NUM) LOGN(NUM, -1)
static int DEBULIST_LINE = -1;
inline void DEBULIST(int num) {
  LOGN(num, DEBULIST_LINE);
  DEBULIST_LINE++;
}
#ifdef SENV_DEBUG
static int DEBUTRACE_LINE = -1;
#endif
inline void DEBUTRACE(std::string string) {
#ifdef SENV_DEBUG
  if (string.empty())
    return;

  if (DEBUTRACE_LINE == -1)
    TextStream::instance().clear();
  TextStream::instance().setText(string, 1 + DEBUTRACE_LINE, -3);
  DEBUTRACE_LINE++;
  if (DEBUTRACE_LINE == 17)
    DEBUTRACE_LINE = -1;
#endif
}

static bool BSOD_ON = false;

inline void BSOD(std::string message) {
  if (BSOD_ON)
    return;
  BSOD_ON = true;

  REG_RCNT |= 1 << 15;  // (disable link cable)
  player_stop();
  SCENE_init();
  BACKGROUND_enable(true, false, false, false);
  TextStream::instance().setText(std::string("piuGBA - ") +
                                     (ENV_ARCADE ? "arcade - " : "full - ") +
                                     (ENV_DEVELOPMENT ? "dev" : "prod"),
                                 0, -3);
  TextStream::instance().setText(message, 2, -3);
  while (true)
    ;
}

inline vu16& _REG_LOG_ENABLE = *reinterpret_cast<vu16*>(0x4FFF780);
inline vu16& _REG_LOG_LEVEL = *reinterpret_cast<vu16*>(0x4FFF700);

static inline void log(const char* fmt, ...) {
  _REG_LOG_ENABLE = 0xC0DE;

  va_list args;
  va_start(args, fmt);

  char* const log = (char*)0x4FFF600;
  vsnprintf(log, 0x100, fmt, args);
  _REG_LOG_LEVEL = 0x102;  // Level: WARN

  va_end(args);
}

#endif  // DEBUG_TOOLS_H
