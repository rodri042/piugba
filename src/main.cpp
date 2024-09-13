#include <libgba-sprite-engine/gba_engine.h>
#include <tonc.h>

#include "../libs/interrupt.h"
#include "gameplay/Sequence.h"
#include "gameplay/debug/DebugTools.h"
#include "gameplay/multiplayer/PS2Keyboard.h"
#include "gameplay/multiplayer/Syncer.h"
#include "gameplay/video/VideoStore.h"
#include "player/PlaybackState.h"
#include "utils/SceneUtils.h"

extern "C" {
#include "player/player.h"
}

#define CODE_EWRAM __attribute__((section(".ewram")))

// Emulators and flashcarts use this string to autodetect the save type
const char* SAVEFILE_TYPE_HINT = "SRAM_Vnnn\0\0";

void validateBuild();
void setUpInterrupts();
void startRandomSeed();
void stopRandomSeed();
void synchronizeSongStart();
static std::shared_ptr<GBAEngine> engine{new GBAEngine()};
static bool isCalculatingRandomSeed = false;
VideoStore* videoStore = new VideoStore();
PS2Keyboard* ps2Keyboard = new PS2Keyboard();
LinkUniversal* linkUniversal =
    new LinkUniversal(LinkUniversal::Protocol::AUTODETECT,
                      "piuGBA",
                      (LinkUniversal::CableOptions){
                          .baudRate = LinkCable::BAUD_RATE_1,
                          .timeout = SYNC_IRQ_TIMEOUT,
                          .interval = SYNC_SEND_INTERVAL,
                          .sendTimerId = LINK_CABLE_DEFAULT_SEND_TIMER_ID},
                      (LinkUniversal::WirelessOptions){
                          .retransmission = true,
                          .maxPlayers = 2,
                          .timeout = SYNC_IRQ_TIMEOUT,
                          .interval = SYNC_SEND_INTERVAL,
                          .sendTimerId = LINK_WIRELESS_DEFAULT_SEND_TIMER_ID},
                      __qran_seed);
Syncer* syncer = new Syncer();
static const GBFS_FILE* fs = find_first_gbfs_file(0);

int main() {
  linkUniversal->deactivate();
  RUMBLE_init();

  REG_WAITCNT = 0x4317;  // (3,1 waitstates, prefetch ON)

  validateBuild();
  setUpInterrupts();
  player_init();
  SEQUENCE_initialize(engine, fs);
  startRandomSeed();

  engine->setScene(SEQUENCE_getInitialScene());
  player_forever(
      []() {
        // (onUpdate)
        LINK_UNIVERSAL_ISR_VBLANK();
        PS2_ISR_VBLANK();

        syncer->update();
        engine->update();

        if (isCalculatingRandomSeed) {
          (void)qran();
          if ((~REG_KEYS & KEY_ANY) != 0)
            stopRandomSeed();
        }

        if (syncer->$isPlayingSong && !syncer->$hasStartedAudio)
          synchronizeSongStart();

        return syncer->$isPlayingSong && !syncer->isMaster()
                   ? (int)syncer->$currentAudioChunk
                   : 0;  // (unsynchronized)
      },
      []() {
        // (onRender)
        ps2Keyboard->update();
        if (ps2Keyboard->keys.softReset)
          SCENE_softReset();

        EFFECT_render();
        engine->render();

        if (syncer->pendingAudio != "") {
          player_play(syncer->pendingAudio.c_str(),
                      isMultiplayer() || active_flashcart == EZ_FLASH_OMEGA);
          syncer->pendingAudio = "";
        }

        if (syncer->pendingSeek > 0) {
          player_seek(syncer->pendingSeek);
          syncer->pendingSeek = 0;
        }
      },
      [](u32 current) {
        // (onAudioChunk)
        if (syncer->$isPlayingSong) {
          if (syncer->isMaster()) {
            syncer->$currentAudioChunk = current;
            syncer->directSend(SYNC_AUDIO_CHUNK_HEADER |
                               ((u16)current + AUDIO_SYNC_LIMIT));
          }
#ifdef SENV_DEBUG
          LOGN(current, -1);
          LOGN(syncer->$currentAudioChunk, 0);
#endif
        }
      },
      []() { SCENE_softReset(); });

  LOGSTR(SAVEFILE_TYPE_HINT, 0);

  return 0;
}

CODE_EWRAM void ISR_reset() {
  if (syncer->$isPlayingSong || syncer->$resetFlag) {
    syncer->$resetFlag = true;
    return;
  }
  if (syncer->isOnline())
    return;

  SCENE_softReset();
}

void validateBuild() {
  if (fs == NULL)
    BSOD("This is an empty ROM.              Import songs or use a pack!");
  if (!ENV_ARCADE && gbfs_get_obj(fs, "_snm_0_list.txt", NULL) == NULL)
    BSOD("*Error* (Wrong build)             Mixed FULL rom + ARCADE lib!");
}

void setUpInterrupts() {
  interrupt_init();

  // VBlank
  interrupt_set_handler(INTR_VBLANK, player_onVBlank);
  interrupt_enable(INTR_VBLANK);

  // LinkUniversal
  interrupt_set_handler(INTR_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
  interrupt_enable(INTR_SERIAL);
  interrupt_set_handler(INTR_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
  interrupt_enable(INTR_TIMER3);

  // A+B+START+SELECT
  REG_KEYCNT = 0b1100000000001111;
  interrupt_set_handler(INTR_KEYPAD, ISR_reset);
  interrupt_enable(INTR_KEYPAD);
}

inline void startRandomSeed() {
  isCalculatingRandomSeed = true;
  REG_TM2CNT_L = 0;
  REG_TM2CNT_H = 0;
  REG_TM2CNT_H = TM_ENABLE | TM_FREQ_1;
}

inline void stopRandomSeed() {
  isCalculatingRandomSeed = false;

  REG_TM2CNT_H = 0;
  __qran_seed += SAVEFILE_read32(SRAM->randomSeed);
  __qran_seed += (1 + (~REG_KEYS & KEY_ANY)) * 1664525 * REG_TM2CNT_L;
  SAVEFILE_write32(SRAM->randomSeed, __qran_seed);
}

void synchronizeSongStart() {
  // discard all previous messages and wait for sync
  u8 remoteId = syncer->getRemotePlayerId();

  while (linkUniversal->read(remoteId) != LINK_CABLE_NO_DATA)
    ;

  u16 start = SYNC_START_SONG | syncer->$currentSongChecksum;
  bool isOnSync = false;
  while (syncer->$isPlayingSong && !isOnSync) {
    syncer->directSend(start);
    VBlankIntrWait();
    linkUniversal->sync();
    isOnSync = linkUniversal->read(remoteId) == start;
    if (!isOnSync)
      syncer->registerTimeout();
  }
  if (!isOnSync)
    return;

  while (linkUniversal->read(remoteId) != LINK_CABLE_NO_DATA)
    ;

  if (!syncer->isMaster())
    syncer->$currentAudioChunk = AUDIO_SYNC_LIMIT;

  syncer->$hasStartedAudio = true;
  syncer->clearTimeout();
}
