#pragma GCC diagnostic ignored "-Wunused-value"

#include "bios.h"

u16 bi_reg_rd(u16 reg);
void bi_reg_wr(u16 reg, u16 data);
u8 bi_sd_dma_to_rom(void* dst, int slen);
void bi_set_ram_bank(u16 bank);
u8 bi_eep_read_dw(u8* dst, u16 addr);
u8 bi_eep_write_dw(u8* src, u16 addr);

#define BUS_CONFIG *((u32*)0x4000204)

#define REG_BASE 0x9FC0000
#define EEP_BASE 0x9FE0000

#define REG_CFG 0x00
#define REG_STATUS 0x01
#define REG_FPGA_VER 0x05
#define REG_SD_CMD 0x08
#define REG_SD_DAT 0x09
#define REG_SD_CFG 0x0A
#define REG_SD_RAM 0x0B
#define REG_KEY 0x5A

#define STAT_SD_BUSY 1
#define STAT_SDC_TOUT 2

#define CFG_REGS_ON 1
#define CFG_NROM_RAM 2
#define CFG_ROM_WE_ON 4
#define CFG_AUTO_WE 8
#define CFG_RTC_ON 0x200
#define CFG_ROM_BANK 0x400
#define CFG_BIG_ROM 0x800  // special eeprom mapping for 32MB games

#define SD_WAIT_F0 8
#define SD_STRT_F0 16
#define SD_MODE_BITS 30
#define SD_SPD_BITS 1

u16 cart_cfg;
u8 sd_cfg;
u16 eep_size;

bool bi_init_sd_only() {
  bi_reg_wr(REG_KEY, 0);
  u16 config = bi_reg_rd(REG_SD_CFG);
  bi_reg_wr(REG_SD_CFG, 0);
  if (bi_reg_rd(REG_SD_CFG) != config)
    return false;
  bi_reg_wr(REG_KEY, 0xA5);  // unlock everdrive registers (write only)
  bi_reg_wr(REG_SD_CFG, 0);
  if (bi_reg_rd(REG_SD_CFG) == config)
    return false;

  sd_cfg = 0;

  return true;
}

void bi_init() {
  // regs read on, switch from boot rom to psram, psram write on, big rom (32MB)
  cart_cfg = CFG_REGS_ON | CFG_NROM_RAM | CFG_ROM_WE_ON |
             (FLASHCARTIO_ED_BIG_ROM ? CFG_BIG_ROM : 0);

  bi_reg_wr(REG_KEY, 0xA5);  // unlock everdrive registers (write only)
  bi_reg_wr(REG_CFG, cart_cfg);
  bi_set_save_type(FLASHCARTIO_ED_SAVE_TYPE);
}

u16 bi_reg_rd(u16 reg) {
  return *((vu16*)(REG_BASE + reg * 2));
}

void bi_reg_wr(u16 reg, u16 data) {
  *((vu16*)(REG_BASE + reg * 2)) = data;
}

void bi_lock_regs() {
  cart_cfg &= ~(CFG_REGS_ON | CFG_ROM_WE_ON);
  bi_reg_wr(REG_CFG, cart_cfg);
}

void bi_unlock_regs() {
  bi_reg_wr(REG_KEY, 0xA5);
  cart_cfg |= (CFG_REGS_ON | CFG_ROM_WE_ON);
  bi_reg_wr(REG_CFG, cart_cfg);
}

u8 bi_sd_wait_f0() {
  u8 resp;
  u16 i;
  u8 mode = SD_MODE4 | SD_WAIT_F0 | SD_STRT_F0;

  for (i = 0; i < 65000; i++) {
    bi_sd_mode(mode);
    bi_reg_rd(REG_SD_DAT);

    for (;;) {
      resp = bi_reg_rd(REG_STATUS);
      if ((resp & STAT_SD_BUSY) == 0)
        break;
    }

    if ((resp & STAT_SDC_TOUT) == 0)
      return 0;

    mode = SD_MODE4 | SD_WAIT_F0;
  }

  return 1;
}

u8 bi_sd_dma_wr(void* src) {
  bi_reg_wr(REG_SD_RAM, 0);
  bi_sd_mode(SD_MODE4);
  DMA_SRC = (u32)src;
  DMA_DST = (u32)(REG_BASE + REG_SD_DAT * 2);
  DMA_LEN = 256;
  DMA_CTR = 0x8040;

  while ((DMA_CTR & 0x8000) != 0)
    ;

  return 0;
}

void bi_sd_read_crc_ram(void* dst) {
  bi_reg_wr(REG_SD_RAM, 0);
  DMA_SRC = (u32)(REG_BASE + REG_SD_RAM * 2);
  DMA_DST = (u32)dst;
  DMA_LEN = 256;
  DMA_CTR = 0x8100;

  while ((DMA_CTR & 0x8000) != 0)
    ;
}

u8 bi_sd_dma_to_rom(void* dst, int slen) {
  u16 buff[256];

  while (slen) {
    if (bi_sd_wait_f0() != 0)
      return 1;

    bi_reg_wr(REG_CFG, cart_cfg | CFG_AUTO_WE);
    DMA_SRC = (u32)dst;
    DMA_DST = (u32)buff;
    DMA_LEN = 256;
    DMA_CTR = 0x8000;
    while ((DMA_CTR & 0x8000) != 0)
      ;
    bi_reg_wr(REG_CFG, cart_cfg);

    slen--;

    dst += 512;
  }

  return 0;
}

// [!]
__attribute__((section(".iwram"), target("arm"), noinline)) u8
bi_sd_dma_rd(void* dst, int slen) {
  if (((u32)dst & 0xE000000) == 0x8000000)
    return bi_sd_dma_to_rom(dst, slen);

  while (slen) {
    if (bi_sd_wait_f0() != 0)
      return 1;

    DMA_SRC = (u32)(REG_BASE + REG_SD_DAT * 2);
    DMA_DST = (u32)dst;
    DMA_LEN = 256;
    DMA_CTR = 0x8000;

    while ((DMA_CTR & 0x8000) != 0)
      ;

    slen--;
    dst += 512;
  }

  return 0;
}

void bi_sd_cmd_wr(u8 data) {
  bi_reg_wr(REG_SD_CMD, data);
  // if ((sd_cfg & SD_SPD_HI))return;
  while ((bi_reg_rd(REG_STATUS) & STAT_SD_BUSY))
    ;
}

u8 bi_sd_cmd_rd() {
  u8 dat = bi_reg_rd(REG_SD_CMD);
  // if ((sd_cfg & SD_SPD_HI))return dat;
  while ((bi_reg_rd(REG_STATUS) & STAT_SD_BUSY))
    ;
  return dat;
}

u8 bi_sd_cmd_val() {
  u8 dat = bi_reg_rd(REG_SD_CMD + 2);
  return dat;
}

void bi_sd_dat_wr(u8 data) {
  bi_reg_wr(REG_SD_DAT, 0xff00 | data);
  // if ((sd_cfg & SD_SPD_HI))return;
  while ((bi_reg_rd(REG_STATUS) & STAT_SD_BUSY))
    ;
}

void bi_sd_dat_wr16(u16 data) {
  bi_reg_wr(REG_SD_DAT, data);
  // if ((sd_cfg & SD_SPD_HI))return;
  while ((bi_reg_rd(REG_STATUS) & STAT_SD_BUSY))
    ;
}

u8 bi_sd_dat_rd() {
  u16 dat = bi_reg_rd(REG_SD_DAT) >> 8;
  // if ((sd_cfg & SD_SPD_HI))return dat;
  while ((bi_reg_rd(REG_STATUS) & STAT_SD_BUSY))
    ;
  return dat;
}

void bi_sd_mode(u8 mode) {
  sd_cfg &= ~SD_MODE_BITS;
  sd_cfg |= mode & SD_MODE_BITS;

  bi_reg_wr(REG_SD_CFG, sd_cfg);
}

void bi_sd_speed(u8 speed) {
  sd_cfg &= ~SD_SPD_BITS;
  sd_cfg |= speed & SD_SPD_BITS;

  bi_reg_wr(REG_SD_CFG, sd_cfg);
}

#define SRAM_ADDR 0xE000000

void bi_set_save_type(u8 save_type) {
  if (save_type != 0)
    bi_set_save_type(0);
  cart_cfg &= ~BI_SAV_BITS;
  save_type &= BI_SAV_BITS;
  cart_cfg |= save_type;
  bi_reg_wr(REG_CFG, cart_cfg);
}
