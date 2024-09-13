#include "Syncer.h"

#include "gameplay/Key.h"

#define ASSERT(CONDITION, FAILURE_REASON) \
  if (!(CONDITION)) {                     \
    fail(FAILURE_REASON);                 \
    return;                               \
  }

#define ASSERT_EVENT(EXPECTED_EVENT, LOG)  \
  if (incomingEvent != (EXPECTED_EVENT)) { \
    timeoutCount++;                        \
    break;                                 \
  } else {                                 \
    DEBUTRACE((LOG));                      \
    timeoutCount = 0;                      \
  }

void Syncer::initialize(SyncMode mode) {
  if (mode != SyncMode::SYNC_MODE_OFFLINE)
    linkUniversal->activate();
  else
    linkUniversal->deactivate();

  this->mode = mode;
  reset();
  resetError();
}

void Syncer::update() {
  if (mode == SyncMode::SYNC_MODE_OFFLINE)
    return;

  linkUniversal->sync();

#ifdef SENV_DEBUG
  if (!linkUniversal->isConnected())
    DEBUTRACE("disconnected...");
#endif

  ASSERT(linkUniversal->isConnected(), SyncError::SYNC_ERROR_NONE);
  ASSERT(linkUniversal->playerCount() == 2,
         SyncError::SYNC_ERROR_TOO_MANY_PLAYERS);

  if (!isActive()) {
    reset();
    playerId = linkUniversal->currentPlayerId();

#ifdef SENV_DEBUG
    DEBUTRACE("* init: player " + DSTR(playerId));
#endif
  }

  if (isPlaying()) {
    resetError();
    // (in gameplay, messaging is handled directly by scenes)
  } else {
    do {
      sync();
    } while (isActive() && linkUniversal->canRead(getRemotePlayerId()));
  }
}

void Syncer::send(u8 event, u16 payload) {
  u16 outgoingData = SYNC_MSG_BUILD(event, payload);
  directSend(outgoingData);

#ifdef SENV_DEBUG
  if (outgoingData != LINK_CABLE_NO_DATA && !$isPlayingSong)
    DEBUTRACE("(" + DSTR(state) + ")...-> " + DSTR(outgoingData) + " (" +
              DSTR(outgoingEvent) + "-" + DSTR(outgoingPayload) + ")");
#endif
}

void Syncer::directSend(u16 data) {
  linkUniversal->send(data);
}

void Syncer::registerTimeout() {
  timeoutCount++;
  checkTimeout();
}

void Syncer::clearTimeout() {
  timeoutCount = 0;
}

void Syncer::sync() {
  u16 incomingData = linkUniversal->read(getRemotePlayerId());
  u8 incomingEvent = SYNC_MSG_EVENT(incomingData);
  u16 incomingPayload = SYNC_MSG_PAYLOAD(incomingData);

#ifdef SENV_DEBUG
  DEBUTRACE("(" + DSTR(state) + ")<- " + DSTR(incomingData) + " (" +
            DSTR(incomingEvent) + "-" + DSTR(incomingPayload) + ")");
#endif

  outgoingEvent = LINK_CABLE_NO_DATA;
  outgoingPayload = LINK_CABLE_NO_DATA;

  switch (state) {
    case SyncState::SYNC_STATE_SEND_ROM_ID: {
      outgoingEvent = SYNC_EVENT_ROM_ID;
      outgoingPayload = getPartialRomId();

      ASSERT_EVENT(SYNC_EVENT_ROM_ID, "* rom id received");

      ASSERT(incomingPayload == getPartialRomId(),
             SyncError::SYNC_ERROR_ROM_MISMATCH);
      setState(SyncState::SYNC_STATE_SEND_PROGRESS);

      break;
    }
    case SyncState::SYNC_STATE_SEND_PROGRESS: {
      u8 modeBit = mode == SyncMode::SYNC_MODE_COOP;
      u8 libraryType = SAVEFILE_getMaxLibraryType();
      u8 completedSongs = SAVEFILE_getMaxCompletedSongs();
      outgoingEvent = SYNC_EVENT_PROGRESS;
      outgoingPayload = isMaster() ? SYNC_MSG_PROGRESS_BUILD(
                                         modeBit, libraryType, completedSongs)
                                   : modeBit;

      ASSERT_EVENT(SYNC_EVENT_PROGRESS, "* progress received");

      if (isMaster()) {
        ASSERT(incomingPayload == modeBit, SyncError::SYNC_ERROR_WRONG_MODE);
        resetGameState();
        $libraryType = libraryType;
        $completedSongs = completedSongs;
        startPlaying();
      } else {
        u8 receivedModeBit = SYNC_MSG_PROGRESS_MODE(incomingPayload);
        u8 receivedLibraryType =
            SYNC_MSG_PROGRESS_LIBRARY_TYPE(incomingPayload);
        u8 receivedCompletedSongs =
            SYNC_MSG_PROGRESS_COMPLETED_SONGS(incomingPayload);

        ASSERT(receivedModeBit == modeBit, SyncError::SYNC_ERROR_WRONG_MODE);
        ASSERT(receivedLibraryType >= DifficultyLevel::NORMAL &&
                   receivedLibraryType <= DifficultyLevel::CRAZY,
               SyncError::SYNC_ERROR_WTF);
        ASSERT(receivedCompletedSongs >= 0 &&
                   receivedCompletedSongs <= SAVEFILE_getLibrarySize(),
               SyncError::SYNC_ERROR_WTF);

        resetGameState();
        $libraryType = receivedLibraryType;
        $completedSongs = receivedCompletedSongs;
        startPlaying();
      }

      break;
    }
    default: {
    }
  }

  sendOutgoingData();
  checkTimeout();
}

void Syncer::sendOutgoingData() {
  send(outgoingEvent, outgoingPayload);
}

void Syncer::checkTimeout() {
  if (timeoutCount >= SYNC_TIMEOUT) {
#ifdef SENV_DEBUG
    DEBUTRACE("! state timeout: " + DSTR(timeoutCount));
#endif

    reset();
  }
}

void Syncer::startPlaying() {
  setState(SyncState::SYNC_STATE_PLAYING);
}

void Syncer::fail(SyncError error) {
#ifdef SENV_DEBUG
  DEBUTRACE("* fail: " + DSTR(error));
#endif

  if (isActive())
    sendOutgoingData();
  reset();
  this->error = error;
}

void Syncer::reset() {
#ifdef SENV_DEBUG
  DEBUTRACE("* reset");
#endif

  playerId = -1;
  setState(SyncState::SYNC_STATE_SEND_ROM_ID);
  resetData();
}

void Syncer::resetData() {
  resetGameState();
  outgoingEvent = LINK_CABLE_NO_DATA;
  outgoingPayload = LINK_CABLE_NO_DATA;
}

void Syncer::resetGameState() {
  $libraryType = 0;
  $completedSongs = 0;

  $remoteNumericLevelIndex = -1;
  $remoteNumericLevel = -1;
  $remoteLastNumericLevel = 0;
  SAVEFILE_write8(SRAM->memory.numericLevel, 0);
  SAVEFILE_write8(SRAM->memory.pageIndex, 0);
  SAVEFILE_write8(SRAM->memory.songIndex, 0);
  SAVEFILE_write32(SRAM->lastNumericLevel, 0);
  SAVEFILE_write8(
      SRAM->adminSettings.arcadeCharts,
      isCoop() ? ArcadeChartsOpts::DOUBLE : ArcadeChartsOpts::SINGLE);

  resetSongState();
}

void Syncer::resetSongState() {
  $isPlayingSong = false;
  $hasStartedAudio = false;
  $currentSongChecksum = 0;
  $currentAudioChunk = 0;
}

void Syncer::resetError() {
  error = SyncError::SYNC_ERROR_NONE;
}
