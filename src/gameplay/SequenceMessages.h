#ifndef SEQUENCE_MESSAGES_H
#define SEQUENCE_MESSAGES_H

#include <string>

#define CALIBRATE_AUDIO_LAG     \
  "Emulators need some\r\n"     \
  "calibration. If you're\r\n"  \
  "emulating, press START.\r\n" \
  "If not, press SELECT."

#define SRAM_TEST_FAILED          \
  "Save file test failed :(\r\n"  \
  "Please set your emulator\r\n"  \
  "or flashcart to SRAM mode\r\n" \
  "and restart the game."

#define ARCADE_MODE_LOCKED       \
  "         LOCKED\r\n"          \
  "Win at least one song in\r\n" \
  "  the CAMPAIGN mode to\r\n"   \
  "      unlock this."

#define IMPOSSIBLE_MODE_LOCKED  \
  "         LOCKED\r\n"         \
  " Complete the CAMPAIGN\r\n"  \
  " mode in any difficulty\r\n" \
  " level to unlock this."

#define MODE_CAMPAIGN             \
  "Play the CAMPAIGN MODE to\r\n" \
  "make progress in the game\r\n" \
  "and unlock new songs."

#define MODE_ARCADE               \
  "In ARCADE MODE, you are\r\n"   \
  "free to play the songs in\r\n" \
  "any difficulty level.\r\n"     \
  "Use SELECT to add mods!"

#define MODE_ARCADE_BONUS      \
  "Huh... What's this?\r\n"    \
  "You've hit REMIX MODE.\r\n" \
  "Bonus tracks await.\r\n"    \
  "Enjoy!"

#define MODE_IMPOSSIBLE         \
  "In IMPOSSIBLE MODE:\r\n"     \
  "- Songs play FASTER\r\n"     \
  "- Patterns are MIRRORED\r\n" \
  "- There are visual MODS"

#define MODE_DEATH_MIX            \
  "In DEATHMIX MODE, you'll\r\n"  \
  "play a mixed nonstop set.\r\n" \
  "Life can't be recovered,\r\n"  \
  "so try not to MISS!"

#define MODE_SHUFFLE              \
  "In SHUFFLE MODE, you'll\r\n"   \
  "play a mixed nonstop set.\r\n" \
  "Pick a difficulty level\r\n"   \
  "and have fun!"

#define KEYS_HINT                 \
  "To change the speed, use:\r\n" \
  "      START/SELECT\r\n"        \
  " To abort the song, use:\r\n"  \
  "    A+B+START+SELECT"

#define KEYS_TRAINING_HINT        \
  "How to use Training Mode:\r\n" \
  "- Speed: START/SELECT\r\n"     \
  "- Rate: B + START/SELECT\r\n"  \
  "- Seek: A + START/SELECT"

#define COOP_HINT               \
  "       CO-OP LEVEL\r\n"      \
  "1) Get a friend.\r\n"        \
  "2) Grab a Link Adapter.\r\n" \
  "3) Use Multi COOP."

#define DOUBLE_PS2_INPUT_HINT  \
  "PS/2 input is enabled!\r\n" \
  "- Connect a keyboard\r\n"   \
  "- P1: Use ZQSEC\r\n"        \
  "- P2: Use Numpad 17593"

#define WIN                      \
  "  *YOU'VE WON THE GAME*\r\n"  \
  "\r\n"                         \
  "Now, I challenge you to \r\n" \
  "play the IMPOSSIBLE MODE"

#define WIN_IMPOSSIBLE         \
  "   WHAT?! You did it??\r\n" \
  "\r\n"                       \
  "   You are an absolute\r\n" \
  "  legend. Amazing work!"

#define WIN_DEATHMIX            \
  " WHOA, congratulations!\r\n" \
  "  Ok, here's a secret:\r\n"  \
  "Try holding START when\r\n"  \
  "opening the ARCADE MODE!"

#define SAVE_FILE_FIXED_1 \
  "Save file fixed!\r\n"  \
  " -> code: "
#define SAVE_FILE_FIXED_2 \
  "\r\n\r\n"              \
  "=> Press A+B+START+SELECT"

#define VIDEO_ACTIVATION_FAILED_CRASH \
  "HQ mode disabled!\r\n"             \
  " -> it crashed :/\r\n"             \
  "\r\n"                              \
  "=> Press A+B+START+SELECT"

#define VIDEO_ACTIVATION_FAILED_NO_FLASHCART \
  "HQ mode disabled!\r\n"                    \
  " -> no flashcart found\r\n"               \
  "\r\n"                                     \
  "=> Press A+B+START+SELECT"

#define VIDEO_ACTIVATION_FAILED_MOUNT_FAILED \
  "HQ mode disabled!\r\n"                    \
  " -> FAT mount error\r\n"                  \
  "\r\n"                                     \
  "=> Press A+B+START+SELECT"

#define VIDEO_ACTIVATION_SUCCESS \
  "HQ mode is now enabled!\r\n"  \
  "\r\n"                         \
  "\r\n"                         \
  "=> Press A+B+START+SELECT"

#define VIDEO_DEACTIVATION_SUCCESS \
  "HQ mode is now disabled!\r\n"   \
  "\r\n"                           \
  "\r\n"                           \
  "=> Press A+B+START+SELECT"

#define EWRAM_OVERCLOCK_ENABLED \
  "EWRAM overclock ON!\r\n"     \
  "\r\n"                        \
  "\r\n"                        \
  "=> Press A+B+START+SELECT"

#define EWRAM_OVERCLOCK_DISABLED \
  "EWRAM overclock OFF!\r\n"     \
  "\r\n"                         \
  "\r\n"                         \
  "=> Press A+B+START+SELECT"

#define OPEN_ADMIN_MENU_HINT    \
  "To open the ADMIN MENU:\r\n" \
  "- Go to the MAIN MENU\r\n"   \
  "- Press L+R+START+SELECT"

#define MULTIPLAYER_UNAVAILABLE_PS2_ON \
  "       UNAVAILABLE\r\n"             \
  "To use the multiplayer\r\n"         \
  "modes, disable PS/2 input\r\n"      \
  "in the ADMIN MENU."

#define HQ_AUDIO_REQUIRED         \
  "HQ audio failed!\r\n"          \
  "\r\n"                          \
  "It is required when using\r\n" \
  "videos on this flashcart.\r\n"

#endif  // SEQUENCE_MESSAGES_H
