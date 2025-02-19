#include "player.h"

#include <gba_dma.h>
#include <gba_input.h>
#include <gba_interrupt.h>
#include <gba_sound.h>
#include <gba_systemcalls.h>
#include <gba_timers.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>  // for memset

#include "PlaybackState.h"
#include "core/gsm.h"
#include "core/private.h" /* for sizeof(struct gsm_state) */
#include "utils/gbfs/gbfs.h"

#include "audio_store.h"
#include "utils/flashcartio/flashcartio.h"

#define TIMER_16MHZ 0
#define FIFO_ADDR_A 0x040000a0
#define FIFO_ADDR_B 0x040000A4
#define CHANNEL_A_MUTE 0b1111110011111111
#define CHANNEL_A_UNMUTE 0b0000001100000000
#define CHANNEL_B_MUTE 0b1100111111111111
#define CHANNEL_B_UNMUTE 0b0011000000000000
#define AUDIO_CHUNK_SIZE_GSM 33
#define AUDIO_CHUNK_SIZE_PCM 304
#define FRACUMUL_PRECISION 0xFFFFFFFF
#define AS_MSECS_GSM 1146880000
#define AS_MSECS_PCM 118273043  // 0xffffffff * (1000/36314)
#define AS_CURSOR_GSM 3201039125
#define AS_CURSOR_PCM 1348619731
#define REG_DMA2CNT_L *(vu16*)(REG_BASE + 0x0d0)
#define REG_DMA2CNT_H *(vu16*)(REG_BASE + 0x0d2)

#define CODE_ROM __attribute__((section(".code")))
#define CODE_EWRAM __attribute__((section(".ewram")))
#define INLINE static inline __attribute__((always_inline))

Playback PlaybackState;

// - In GSM mode:
//   Audio is taken from the embedded GBFS file in ROM.
//   The sample rate is 18157hz, linearly interpolated to 36314hz.
//   Each GSM chunk is 33 bytes and represents 304 samples.
//   Two chunks are copied per frame, filling the 608 entries of the buffer.
//   (This is one of the few combinations of sample rate / buffer size that
//   time out perfectly in the 280896 cycles of a GBA frame)
//   See: (JS code)
//     for (var i=80; i<1000; i++)
//       if ((280896%i)==0 && (i%16) == 0)
//         console.log(
//           'timer =', 65536-(280896/i), '; buffer =',
//           i, '; sample rate =', i*(1<<24)/280896, 'hz'
//         );
//   Playback rate can be changed by +/- 13%, 26%, or 53%.
// - In PCM s8 mode:
//   Audio is taken from the flash cart's SD card (gba-flashcartio).
//   The sample rate is 36314hz.
//   Each PCM chunk is 304 bytes and represents 304 samples.
//   Two chunks are copied per frame.
//   Playback rate can be either 1 or 1.13.

static const int rate_delays[] = {1, 2, 4, 0, 4, 2, 1};
static signed char rate_xfade[304];

static bool is_pcm = false;
static int rate = 0;
static u32 rate_counter = 0;
static u32 current_audio_chunk = 0;
static bool did_run = false;

#define AS_MSECS (is_pcm ? AS_MSECS_PCM : AS_MSECS_GSM)

#define AUDIO_PROCESS(ON_STEP, ON_STOP, ON_ERROR)                        \
  did_run = true;                                                        \
  buffer = double_buffers[cur_buffer];                                   \
                                                                         \
  if (src != NULL) {                                                     \
    if (is_pcm) {                                                        \
      if (src_pos < src_len) {                                           \
        bool skipSamples = is_pcm && rate != 0 && rate_counter == 0;     \
        if (skipSamples) {                                               \
          audio_store_read(rate_xfade, AUDIO_CHUNK_SIZE_PCM);            \
          src_pos += AUDIO_CHUNK_SIZE_PCM;                               \
        }                                                                \
        if (!audio_store_read(buffer, 608)) {                            \
          ON_ERROR;                                                      \
        } else {                                                         \
          src_pos += 608;                                                \
        }                                                                \
        if (skipSamples) {                                               \
          const int FIXED_POINT_MULTIPLIER = 1024;                       \
          const int FIXED_POINT_SHIFT = 10;                              \
          for (int i = 0; i < 304; i++) {                                \
            int lerp_weight = (304 - i) * FIXED_POINT_MULTIPLIER / 304;  \
            int inv_lerp_weight = i * FIXED_POINT_MULTIPLIER / 304;      \
            int sample_rate_xfade = (int)rate_xfade[i];                  \
            int sample_buffer = (int)buffer[i];                          \
            int crossfaded_sample = (sample_rate_xfade * lerp_weight +   \
                                     sample_buffer * inv_lerp_weight) >> \
                                    FIXED_POINT_SHIFT;                   \
            buffer[i] = (signed char)crossfaded_sample;                  \
          }                                                              \
        }                                                                \
        if (src_pos >= src_len) {                                        \
          ON_STOP;                                                       \
        }                                                                \
      } else {                                                           \
        ON_STOP;                                                         \
      }                                                                  \
    } else {                                                             \
      if (src_pos < src_len) {                                           \
        for (i = 304 / 4; i > 0; i--) {                                  \
          int cur_sample;                                                \
          if (decode_pos >= 160) {                                       \
            if (src_pos < src_len)                                       \
              gsm_decode(&decoder, (src + src_pos), out_samples);        \
            src_pos += sizeof(gsm_frame);                                \
            decode_pos = 0;                                              \
            ON_STEP;                                                     \
          }                                                              \
                                                                         \
          /* 2:1 linear interpolation */                                 \
          cur_sample = out_samples[decode_pos++];                        \
          *buffer++ = (last_sample + cur_sample) >> 9;                   \
          *buffer++ = cur_sample >> 8;                                   \
          last_sample = cur_sample;                                      \
                                                                         \
          cur_sample = out_samples[decode_pos++];                        \
          *buffer++ = (last_sample + cur_sample) >> 9;                   \
          *buffer++ = cur_sample >> 8;                                   \
          last_sample = cur_sample;                                      \
                                                                         \
          cur_sample = out_samples[decode_pos++];                        \
          *buffer++ = (last_sample + cur_sample) >> 9;                   \
          *buffer++ = cur_sample >> 8;                                   \
          last_sample = cur_sample;                                      \
                                                                         \
          cur_sample = out_samples[decode_pos++];                        \
          *buffer++ = (last_sample + cur_sample) >> 9;                   \
          *buffer++ = cur_sample >> 8;                                   \
          last_sample = cur_sample;                                      \
        }                                                                \
        if (src_pos >= src_len) {                                        \
          ON_STOP;                                                       \
        }                                                                \
      } else {                                                           \
        ON_STOP;                                                         \
      }                                                                  \
    }                                                                    \
  }

uint32_t fracumul(uint32_t x, uint32_t frac) __attribute__((long_call));
static const GBFS_FILE* fs;
static const unsigned char* src = NULL;
static uint32_t src_len = 0;
static uint32_t src_pos = 0;
static struct gsm_state decoder;
static signed short out_samples[160];
static signed char double_buffers[2][608] __attribute__((aligned(4)));
static unsigned int decode_pos = 160, cur_buffer = 0;
static signed char* buffer;
static int last_sample = 0;
static int i;

INLINE void gsm_init(gsm r) {
  memset((char*)r, 0, sizeof(*r));
  r->nrp = 40;
}

INLINE void mute() {
  DSOUNDCTRL = DSOUNDCTRL & CHANNEL_B_MUTE;
}

INLINE void unmute() {
  DSOUNDCTRL = DSOUNDCTRL | CHANNEL_B_UNMUTE;
}

INLINE void turn_on_sound() {
  SETSNDRES(1);
  SNDSTAT = SNDSTAT_ENABLE;
  DSOUNDCTRL = 0b1111000000001100;
  mute();
}

INLINE void init() {
  /* TMxCNT_L is count; TMxCNT_H is control */
  REG_TM1CNT_H = 0;
  REG_TM1CNT_L = 0x10000 - (924 / 2);        // 0x10000 - (16777216 / 36314)
  REG_TM1CNT_H = TIMER_16MHZ | TIMER_START;  //            cpuFreq  / sampleRate

  mute();
}

INLINE void stop() {
  mute();
  src = NULL;
  decode_pos = 160;
  cur_buffer = 0;
  last_sample = 0;
  for (u32 i = 0; i < 2; i++) {
    u32* bufferPtr = (u32*)double_buffers[i];
    for (u32 j = 0; j < 608 / 4; j++)
      bufferPtr[j] = 0;
  }
}

INLINE void play(const char* name) {
  stop();

  if (is_pcm) {
    src = (const unsigned char*)1;  // (unused)
    src_pos = 0;
    src_len = audio_store_len();
  } else {
    gsm_init(&decoder);
    src = gbfs_get_obj(fs, name, &src_len);
    src_pos = 0;
  }
}

INLINE void disable_audio_dma() {
  // ----------------------------------------------------
  // This convoluted process was taken from the official manual.
  // It's supposed to disable DMA2 in a "safe" way, avoiding DMA lockups.
  //
  // 32-bit write
  // enabled = 1; start timing = immediately; transfer type = 32 bits;
  // repeat = off; destination = fixed; other bits = no change
  REG_DMA2CNT = (REG_DMA2CNT & 0b00000000000000001100110111111111) |
                (0x0004 << 16) | DMA_ENABLE | DMA32 | DMA_DST_FIXED;
  //
  // wait 4 cycles
  asm volatile("eor r0, r0; eor r0, r0" ::: "r0");
  asm volatile("eor r0, r0; eor r0, r0" ::: "r0");
  //
  // 16-bit write
  // enabled = 0; start timing = immediately; transfer type = 32 bits;
  // repeat = off; destination = fixed; other bits = no change
  REG_DMA2CNT_H = (REG_DMA2CNT_H & 0b0100110111111111) |
                  0b0000010100000000;  // DMA32 | DMA_DST_FIXED
  //
  // wait 4 more cycles
  asm volatile("eor r0, r0; eor r0, r0" ::: "r0");
  asm volatile("eor r0, r0; eor r0, r0" ::: "r0");
}

INLINE void dsound_start_audio_copy(const void* source) {
  // disable DMA2
  disable_audio_dma();

  // setup DMA2 for audio
  REG_DMA2SAD = (intptr_t)source;
  REG_DMA2DAD = (intptr_t)FIFO_ADDR_B;
  REG_DMA2CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 | DMA_SPECIAL |
                DMA_ENABLE | 1;
}

INLINE void load_file(const char* name, bool forceGSM) {
  PlaybackState.msecs = 0;
  PlaybackState.hasFinished = false;
  PlaybackState.isLooping = false;
  rate = 0;
  rate_counter = 0;
  current_audio_chunk = 0;

  if (PlaybackState.fatfs != NULL && !PlaybackState.isPCMDisabled &&
      !forceGSM) {
    char fileName[64];
    strcpy(fileName, name);
    strcat(fileName, ".aud.bin");

    stop();
    bool success = audio_store_load(fileName);
    if (success) {
      is_pcm = true;
      play(fileName);
      return;
    }
  }

  char fileName[64];
  strcpy(fileName, name);
  strcat(fileName, ".gsm");
  is_pcm = false;
  play(fileName);
}

CODE_ROM void player_init() {
  fs = find_first_gbfs_file(0);
  turn_on_sound();
  init();

  PlaybackState.msecs = 0;
  PlaybackState.hasFinished = false;
  PlaybackState.isLooping = false;
  PlaybackState.isPCMDisabled = false;
  PlaybackState.fatfs = NULL;
}

CODE_ROM void player_unload() {
  disable_audio_dma();
}

CODE_ROM bool player_playSfx(const char* name) {
  load_file(name, active_flashcart == EZ_FLASH_OMEGA);
  return is_pcm;
}

CODE_ROM bool player_play(const char* name, bool forceGSM) {
  load_file(name, forceGSM);
  return is_pcm;
}

CODE_ROM void player_enableLoop() {
  PlaybackState.isLooping = true;
}

CODE_ROM void player_seek(unsigned int msecs) {
  // (cursor must be a multiple of AUDIO_CHUNK_SIZE)
  // cursor = src_pos

  if (is_pcm) {
    // cursor = msecs * (sampleRate / 1000) = msecs * 36.314
    // => cursor = msecs * (36 + 0.314)

    unsigned int cursor = msecs * 36 + fracumul(msecs, AS_CURSOR_PCM);
    cursor = (cursor / AUDIO_CHUNK_SIZE_PCM) * AUDIO_CHUNK_SIZE_PCM;
    src_pos = cursor;
    rate_counter = 0;
    current_audio_chunk = 0;
    audio_store_seek(src_pos);
  } else {
    // msecs = cursor * msecsPerByte
    // msecsPerByte = AS_MSECS / FRACUMUL_PRECISION ~= 0.267
    // => msecs = cursor * 0.267
    // => cursor = msecs / 0.267 = msecs * 3.7453
    // => cursor = msecs * (3 + 0.7453)

    unsigned int cursor = msecs * 3 + fracumul(msecs, AS_CURSOR_GSM);
    cursor = (cursor / AUDIO_CHUNK_SIZE_GSM) * AUDIO_CHUNK_SIZE_GSM;
    src_pos = cursor;
    rate_counter = 0;
    current_audio_chunk = 0;
  }
}

CODE_ROM void player_setRate(int newRate) {
  rate = newRate;
  rate_counter = 0;
}

CODE_ROM void player_stop() {
  stop();

  PlaybackState.msecs = 0;
  PlaybackState.hasFinished = false;
  PlaybackState.isLooping = false;
  rate = 0;
  rate_counter = 0;
  current_audio_chunk = 0;
}

CODE_ROM bool player_isPlaying() {
  return src != NULL;
}

void player_onVBlank() {
  dsound_start_audio_copy(double_buffers[cur_buffer]);

  if (!did_run)
    return;

  if (src != NULL)
    unmute();

  cur_buffer = !cur_buffer;
  did_run = false;
}

CODE_ROM void update_rate() {
  if (rate != 0) {
    rate_counter++;
    if (rate_counter == rate_delays[rate + RATE_LEVELS]) {
      if (!is_pcm)
        src_pos += AUDIO_CHUNK_SIZE_GSM * (rate < 0 ? -1 : 1);
      rate_counter = 0;
    }
  }
}

void player_forever(int (*onUpdate)(),
                    void (*onRender)(),
                    void (*onAudioChunks)(unsigned int current),
                    void (*onError)()) {
  while (1) {
    // > main game loop
    int expectedAudioChunk = onUpdate();

    // > multiplayer audio sync
    bool isSynchronized = expectedAudioChunk > 0;
    int availableAudioChunks = expectedAudioChunk - current_audio_chunk;
    bool skipped = false;
    if (isSynchronized) {
      if (availableAudioChunks > AUDIO_SYNC_LIMIT) {
        // underrun (slave is behind master)
        unsigned int diff = availableAudioChunks - AUDIO_SYNC_LIMIT;

        src_pos += AUDIO_CHUNK_SIZE_GSM * diff;
        current_audio_chunk += diff;
        availableAudioChunks = AUDIO_SYNC_LIMIT;
      } else if (availableAudioChunks < -AUDIO_SYNC_LIMIT) {
        // overrun (master is behind slave)
        skipped = true;
      }
    }

    // > adjust position based on audio rate
    update_rate();

    if (!skipped) {
      // > audio processing (back buffer)
      AUDIO_PROCESS(
          {
            if (isSynchronized)
              availableAudioChunks--;
            current_audio_chunk++;
          },
          {
            if (PlaybackState.isLooping)
              player_seek(0);
            else {
              player_stop();
              PlaybackState.hasFinished = true;
            }
          },
          { onError(); });
    }

    // > notify multiplayer audio sync cursor
    onAudioChunks(current_audio_chunk);

    // > calculate played milliseconds
    PlaybackState.msecs = fracumul(src_pos, AS_MSECS);

    // > wait for vertical blank
    VBlankIntrWait();

    // > draw
    onRender();
  }
}
