#ifndef VIDEO_STORE_H
#define VIDEO_STORE_H

#include <libgba-sprite-engine/gba/tonc_core.h>

#include <string>

#define VIDEOS_FOLDER_NAME "/piuGBA_videos/"
#define VIDEO_SIZE_PALETTE 512
#define VIDEO_SIZE_MAP 2048
#define VIDEO_SIZE_TILES 38912
#define VIDEO_SECTOR 512

class VideoStore {
 public:
  enum State { OFF, NO_SUPPORTED_FLASHCART, MOUNT_ERROR, ACTIVE };

  bool isActive() { return state == ACTIVE; }
  bool isPreRead() { return !frameLatch; }
  void advanceFrame() { frameLatch = !frameLatch; }

  // TODO: seek(...) const u32 FRACUMUL_DIV_BY_33 = 130150524;
  bool isEnabled();
  void disable();
  bool isActivating();
  State activate();
  bool load(std::string videoPath);
  void unload();

  bool preRead();
  bool endRead(u8* buffer, u32 sectors);

 private:
  State state = OFF;
  u8* memory = NULL;
  u32 cursor = 0;
  u32 memoryCursor = 0;
  bool isPlaying = false;
  bool frameLatch = false;
};

extern VideoStore* videoStore;

#endif  // VIDEO_STORE_H
