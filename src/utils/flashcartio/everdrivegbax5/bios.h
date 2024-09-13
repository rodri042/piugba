/*
 * File:   bios.h
 * Author: krik
 *
 * Created on December 15, 2015, 1:50 PM
 */

// EverDrive-GBA IO 05.09.16

#ifndef BIOS_H
#define BIOS_H

#pragma GCC system_header

#include "../sys.h"

#define BI_USB_BUFF 32767

#define SD_SPD_LO 0
#define SD_SPD_HI 1

#define SD_MODE1 0
#define SD_MODE2 2
#define SD_MODE4 4
#define SD_MODE8 6

#define BI_SAV_BITS (BI_SAV_EEP | BI_SAV_FLA128 | BI_SAV_FLA64 | BI_SAV_SRM)
#define BI_RAM_BNK_0 0
#define BI_RAM_BNK_1 128
#define BI_RAM_BNK_2 256
#define BI_RAM_BNK_3 384

#define BI_CART_FEA_RTC 0x0001
#define BI_CART_FEA_SPD 0x0002
#define BI_CART_FEA_BAT 0x0004
#define BI_CART_FEA_CRC_RAM 0x0008
#define BI_CART_TYPE 0xff00

#define EEP_SIZE_512 6
#define EEP_SIZE_8K 14

#define GBA_WAITCNT *((vu32*)0x4000204)
#define GBA_TIMER1_VAL *((vu16*)0x4000104)
#define GBA_TIMER1_CFG *((vu16*)0x4000106)
#define IRQ_ACK_REG *(vu16*)0x4000202
#define IRQ_GAME_PAK 0x2000

bool bi_init_sd_only();
void bi_init();
void bi_lock_regs();
void bi_unlock_regs();

void bi_sd_cmd_wr(u8 data);
u8 bi_sd_cmd_rd();
void bi_sd_dat_wr(u8 data);
u8 bi_sd_dat_rd();
u8 bi_sd_dma_rd(void* dst, int slen);

void bi_sd_mode(u8 mode);
void bi_sd_speed(u8 speed);

void bi_set_save_type(u8 save_type);

#endif /* BIOS_H */
