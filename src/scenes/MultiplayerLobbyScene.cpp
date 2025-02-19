#include "MultiplayerLobbyScene.h"

#include <string>

#include "SelectionScene.h"
#include "StartScene.h"
#include "gameplay/Key.h"
#include "utils/SceneUtils.h"

const std::string messages[] = {
    "Connect a Link Cable or\r\na Wireless Adapter...\r\n\r\n(Press SELECT to "
    "cancel)",
    "ERROR:\r\nwtf?!",
    "ERROR:\r\nToo many players!",
    "ERROR:\r\nROM IDs don't match!",
    "ERROR:\r\nMixed game modes!",
    "Connecting...\r\n                 [cable]\r\n\r\n(Press SELECT to cancel)",
    "Connecting...\r\n       [wireless / host]\r\n\r\n(Press SELECT to "
    "cancel)"};

const u32 LOADING_INDICATOR_X = GBA_SCREEN_WIDTH - 16 - 4;
const u32 LOADING_INDICATOR_Y = GBA_SCREEN_HEIGHT - 16 - 4;

std::vector<Sprite*> MultiplayerLobbyScene::sprites() {
  std::vector<Sprite*> sprites;

  sprites.push_back(loadingIndicator->get());

  return sprites;
}

void MultiplayerLobbyScene::load() {
  TextScene::load();

  loadingIndicator = std::unique_ptr<Explosion>{
      new Explosion(LOADING_INDICATOR_X, LOADING_INDICATOR_Y, false)};

  syncer->initialize(mode);
#ifndef SENV_DEVELOPMENT
  refresh(0);
#endif
}

void MultiplayerLobbyScene::tick(u16 keys) {
  if (engine->isTransitioning())
    return;

  loadingIndicator->tick();

  if (syncer->isPlaying()) {
    start();
    return;
  }

  if (!KEY_ANYKEY(keys))
    canExit = true;

  if (canExit && KEY_SEL(keys)) {
    goBack();
    return;
  }

#ifndef SENV_DEVELOPMENT
  refresh(syncer->getLastError());
#else
  TextStream::instance().setText(
      "P" + std::to_string(linkUniversal->currentPlayerId()) + "/" +
          std::to_string(linkUniversal->playerCount()) + " [" +
          std::to_string((int)linkUniversal->getState()) + "]<" +
          std::to_string((int)linkUniversal->getMode()) + ">(" +
          std::to_string((int)linkUniversal->getWirelessState()) + ") w(" +
          std::to_string(linkUniversal->_getWaitCount()) + ") sw(" +
          std::to_string(linkUniversal->_getSubWaitCount()) + ")",
      2, 0);
#endif

#ifdef SENV_DEBUG
  if (!hasStarted) {
    BACKGROUND_enable(true, true, false, false);
    SPRITE_enable();
    hasStarted = true;
  }
#endif

#ifndef SENV_DEBUG
#ifndef SENV_DEVELOPMENT
  TextScene::tick(keys);
#endif
#endif
}

void MultiplayerLobbyScene::refresh(int newMessageId) {
  if (newMessageId == 0 &&
      linkUniversal->getProtocol() == LinkUniversal::Protocol::CABLE)
    newMessageId = 5;
  if (newMessageId == 0 &&
      linkUniversal->getProtocol() == LinkUniversal::Protocol::WIRELESS_SERVER)
    newMessageId = 6;

  if (newMessageId != messageId) {
    messageId = newMessageId;
    write(messages[messageId]);
  }
}

void MultiplayerLobbyScene::start() {
  player_stop();
  engine->transitionIntoScene(new SelectionScene(engine, fs),
                              new PixelTransitionEffect());
}

void MultiplayerLobbyScene::goBack() {
  player_stop();
  engine->transitionIntoScene(new StartScene(engine, fs),
                              new PixelTransitionEffect());
  syncer->initialize(SyncMode::SYNC_MODE_OFFLINE);
}
