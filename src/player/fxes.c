#include "fxes.h"

#include <gba_dma.h>
#include <gba_input.h>
#include <gba_interrupt.h>
#include <gba_sound.h>
#include <gba_systemcalls.h>
#include <gba_timers.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>  // for memset

#include "core/gsm.h"
#include "core/private.h" /* for sizeof(struct gsm_state) */
#include "utils/gbfs/gbfs.h"

#define TIMER_16MHZ 0
uint32_t fracumul(uint32_t x, uint32_t frac) __attribute__((long_call));

static const GBFS_FILE* fs;
static const unsigned char* src;
static uint32_t src_len;
static const unsigned char* src_pos = NULL;
static const unsigned char* src_end = NULL;

static struct gsm_state decoder;
static signed short out_samples[160];
static signed char double_buffers[2][608] __attribute__((aligned(4)));
static unsigned int decode_pos = 160, cur_buffer = 0;
static signed char* dst_pos;
static int last_sample = 0;
static int i;

static void dsound_switch_buffers(const void* src) {
  REG_DMA2CNT = 0;

  /* no-op to let DMA registers catch up */
  asm volatile("eor r0, r0; eor r0, r0" ::: "r0");

  REG_DMA2SAD = (intptr_t)src;
  REG_DMA2DAD = (intptr_t)0x040000a0; /* write to FIFO A address */
  REG_DMA2CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 | DMA_SPECIAL |
                DMA_ENABLE | 1;
}

static void gsm_init(gsm r) {
  memset((char*)r, 0, sizeof(*r));
  r->nrp = 40;
}

static void unmute() {
  DSOUNDCTRL = DSOUNDCTRL | 0b0011000000000000;
}

static void mute() {
  DSOUNDCTRL = DSOUNDCTRL & 0b1100111111111111;
}

void fxes_init() {
  fs = find_first_gbfs_file(0);

  // TM1CNT_L is count; TM1CNT_H is control
  REG_TM1CNT_H = 0;
  DSOUNDCTRL = 0x0b0e;
  REG_TM1CNT_L = 0x10000 - (924 / 2);
  REG_TM1CNT_H = TIMER_16MHZ | TIMER_START;

  mute();
}

void fxes_play(const char* name) {
  gsm_init(&decoder);
  src = gbfs_get_obj(fs, name, &src_len);
  src_pos = src;
  src_end = src + src_len;
}

void fxes_stop() {
  src_pos = NULL;
  src_end = NULL;
  mute();
}

void fxes_preUpdate() {
  dst_pos = double_buffers[cur_buffer];

  if (src_pos < src_end) {
    for (i = 304 / 4; i > 0; i--) {
      int cur_sample;
      if (decode_pos >= 160) {
        if (src_pos < src_end) {
          gsm_decode(&decoder, src_pos, out_samples);
        }
        src_pos += sizeof(gsm_frame);
        decode_pos = 0;
      }

      /* 2:1 linear interpolation */
      cur_sample = out_samples[decode_pos++];
      *dst_pos++ = (last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      last_sample = cur_sample;

      cur_sample = out_samples[decode_pos++];
      *dst_pos++ = (last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      last_sample = cur_sample;

      cur_sample = out_samples[decode_pos++];
      *dst_pos++ = (last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      last_sample = cur_sample;

      cur_sample = out_samples[decode_pos++];
      *dst_pos++ = (last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      last_sample = cur_sample;
    }
  } else
    fxes_stop();
}

void fxes_postUpdate() {
  dsound_switch_buffers(double_buffers[cur_buffer]);

  if (src_pos != NULL)
    unmute();

  cur_buffer = !cur_buffer;
}
