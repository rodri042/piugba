#ifndef PLAYBACK_STATE_H
#define PLAYBACK_STATE_H

typedef struct {
  unsigned int msecs;
  bool hasFinished;
  bool isLooping;
  bool mute;
  void* fatfs;
} Playback;

extern Playback PlaybackState;

#endif  // PLAYBACK_STATE_H
