#ifndef DEBUG_TOOLS_H
#define DEBUG_TOOLS_H

#include <libgba-sprite-engine/background/text_stream.h>

#ifndef ENV_DEBUG
// Defined in Makefile
#define ENV_DEBUG false
#endif

#ifndef ENV_DEVELOPMENT
// Defined in Makefile
#define ENV_DEVELOPMENT false
#endif

#define STRESSTEST_MODE false
#define TIMINGTEST_MODE false
#define IFSTRESSTEST if (STRESSTEST_MODE)
#define IFTIMINGTEST if (TIMINGTEST_MODE)
#define LOGN(NUM, LINE) (LOGSTR(std::to_string(NUM).c_str(), LINE))
#define LOGSTR(STR, LINE) (TextStream::instance().setText(STR, 1 + LINE, 15))
#define DEBULOG(NUM) LOGN(NUM, -1)
static int DEBULIST_LINE = -1;
inline void DEBULIST(int num) {
  LOGN(num, DEBULIST_LINE);
  DEBULIST_LINE++;
}
static int DEBUTRACE_LINE = -1;
inline void DEBUTRACE(std::string string) {
  // TODO: IF DEBUG
  if (DEBUTRACE_LINE == -1)
    TextStream::instance().clear();
  TextStream::instance().setText("                              ",
                                 1 + DEBUTRACE_LINE, -3);
  TextStream::instance().setText(string, 1 + DEBUTRACE_LINE, -3);
  DEBUTRACE_LINE++;
  if (DEBUTRACE_LINE == 17)
    DEBUTRACE_LINE = -1;
}
#endif  // DEBUG_TOOLS_H
