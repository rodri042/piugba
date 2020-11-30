#include <libgba-sprite-engine/gba_engine.h>
#include <tonc.h>

#include "gameplay/Sequence.h"
#include "gameplay/debug/DebugTools.h"
#include "gameplay/multiplayer/Syncer.h"
#include "player/PlaybackState.h"

extern "C" {
#include "player/player.h"
}

// Emulators and flashcarts use this string to autodetect the save type
const char* SAVEFILE_TYPE_HINT = "SRAM_Vnnn\0\0";

void setUpInterrupts();
static std::shared_ptr<GBAEngine> engine{new GBAEngine()};
LinkConnection* linkConnection = new LinkConnection();
Syncer* syncer = new Syncer();
static const GBFS_FILE* fs = find_first_gbfs_file(0);

int main() {
  if (fs == NULL)
    BSOD("GBFS file not found.");

  setUpInterrupts();
  player_init();
  SEQUENCE_initialize(engine, fs);

  engine->setScene(SEQUENCE_getInitialScene());
  player_forever(
      []() {
        syncer->update();
        engine->update();

        return syncer->$isPlayingSong && !syncer->isMaster()
                   ? (syncer->$hasStartedAudio ? syncer->$availableAudioChunks
                                               : 0)
                   : -999;  // (unsynchronized)
      },
      [](u32 consumed) {
        if (syncer->$isPlayingSong) {
#ifdef SENV_DEBUG
          LOGN(PlaybackState.msecs, -1);
          LOGN(syncer->$availableAudioChunks, 0);
#endif

          if (syncer->isMaster())
            syncer->$consumedAudioChunks = consumed;
          else
            syncer->$availableAudioChunks -= consumed;
        }
      });

  LOGSTR(SAVEFILE_TYPE_HINT, 0);

  return 0;
}

void ISR_reset() {
  RegisterRamReset(RESET_REG | RESET_VRAM);
  SoftReset();
}

void ISR_vblank() {
  player_onVBlank();
  LINK_ISR_VBLANK();
}

void setUpInterrupts() {
  irq_init(NULL);

  // VBlank
  irq_add(II_VBLANK, ISR_vblank);

  // LinkConnection
  irq_add(II_SERIAL, LINK_ISR_SERIAL);
  irq_add(II_TIMER3, LINK_ISR_TIMER);
  irq_add(II_TIMER2, NULL);

  // A+B+START+SELECT
  REG_KEYCNT = 0b1100000000001111;
  irq_add(II_KEYPAD, ISR_reset);
}
