#ifndef SYNC_H
#define SYNC_H

#include <libgba-sprite-engine/gba/tonc_core.h>
#include <libgba-sprite-engine/gba/tonc_memdef.h>
#include <libgba-sprite-engine/gba/tonc_memmap.h>

#include <string>
#include "Protocol.h"
#include "gameplay/debug/DebugTools.h"
#include "gameplay/save/SaveFile.h"
#include "utils/gba-link-connection/LinkUniversal.hpp"

// Max invalid messages
#define SYNC_TIMEOUT 50

// Max frames without a serial IRQ
#define SYNC_CABLE_TIMEOUT 8

// Max frames without a wireless packet
#define SYNC_WIRELESS_TIMEOUT 16

// Number of timer ticks (61.04μs) between messages (75 = 4.578ms)
#define SYNC_SEND_INTERVAL 75

enum SyncState {
  SYNC_STATE_SEND_ROM_ID,
  SYNC_STATE_SEND_PROGRESS,
  SYNC_STATE_PLAYING
};
enum SyncMode { SYNC_MODE_OFFLINE, SYNC_MODE_VS, SYNC_MODE_COOP };
enum SyncError {
  SYNC_ERROR_NONE,
  SYNC_ERROR_WTF,
  SYNC_ERROR_TOO_MANY_PLAYERS,
  SYNC_ERROR_ROM_MISMATCH,
  SYNC_ERROR_WRONG_MODE
};

inline bool isMultiplayer() {
  return IS_MULTIPLAYER(SAVEFILE_getGameMode());
}

inline bool isVs() {
  return SAVEFILE_getGameMode() == GameMode::MULTI_VS;
}

inline bool isCoop() {
  return SAVEFILE_getGameMode() == GameMode::MULTI_COOP;
}

inline bool isSinglePlayerDouble() {
  return SAVEFILE_isPlayingSinglePlayerDouble();
}

inline bool isDouble() {
  return SAVEFILE_isPlayingDouble();
}

class Syncer {
 public:
  u8 $libraryType = 0;
  u8 $completedSongs = 0;
  int $remoteNumericLevelIndex = -1;
  int $remoteNumericLevel = -1;
  int $remoteLastNumericLevel = 0;
  bool $isPlayingSong = false;
  bool $hasStartedAudio = false;
  bool $resetFlag = false;
  u8 $currentSongChecksum = 0;
  u32 $currentAudioChunk = 0;
  Syncer() {}

  inline bool isPlaying() { return state >= SyncState::SYNC_STATE_PLAYING; }
  inline bool isMaster() { return playerId == 0; }
  inline int getLocalPlayerId() { return isActive() ? playerId : 0; }
  inline int getRemotePlayerId() { return isActive() ? !playerId : 0; }
  inline SyncError getLastError() { return error; }

  inline SyncState getState() { return state; }

  inline void setState(SyncState newState) {
#ifdef SENV_DEBUG
    DEBUTRACE("--> state " + DSTR(newState));
#endif

    state = newState;
    clearTimeout();
  }

  inline SyncMode getMode() { return mode; }
  inline bool isOnline() { return mode > SyncMode::SYNC_MODE_OFFLINE; }

  void initialize(SyncMode mode);
  void update();
  void send(u8 event, u16 payload);
  void directSend(u16 data);
  void registerTimeout();
  void clearTimeout();
  void resetSongState();

  void setRemoteNumericLevel(int newIndex, int newLevel) {
    $remoteNumericLevelIndex = newIndex;
    $remoteNumericLevel = newLevel;
  }

 private:
  SyncState state = SyncState::SYNC_STATE_SEND_ROM_ID;
  SyncMode mode = SyncMode::SYNC_MODE_OFFLINE;
  SyncError error = SyncError::SYNC_ERROR_NONE;
  int playerId = -1;
  u8 outgoingEvent = 0;
  u16 outgoingPayload = 0;
  u32 timeoutCount = 0;

  inline bool isActive() { return playerId > -1; }

  inline u16 getPartialRomId() {
    u32 romId = SAVEFILE_read32(SRAM->romId);
    return (romId & 0b00000000000111111111100000000000) >> 11;
  }

  void sync();
  void sendOutgoingData();
  void checkTimeout();
  void startPlaying();
  void fail(SyncError error);
  void reset();
  void resetData();
  void resetGameState();
  void resetError();

 public:
  // ---
  // HACK: This doesn't belong here.
  // Used by SelectionScene to delay music playback until render finishes
  std::string pendingAudio = "";
  u32 pendingSeek = 0;
};

extern Syncer* syncer;

#endif  // SYNC_H
