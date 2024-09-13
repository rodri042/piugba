/*
 * File:   disk.h
 * Author: krik
 *
 * Created on December 19, 2015, 1:24 PM
 */

// EverDrive-GBA IO 05.09.16

#ifndef DISK_H
#define DISK_H

#pragma GCC system_header

#include "../sys.h"

#define DISK_ERR_INIT 0xC0
#define DISK_ERR_RD1 0xD2
#define DISK_ERR_RD2 0xD3

#define DISK_ERR_CMD_TIMEOUT 0xD9
#define DISK_ERR_CRC_ERROR 0xDA
#define DISK_ERR_CLOSE_RW1 0xDB
#define DISK_ERR_CLOSE_RW2 0xDC

u8 diskInit();
u8 diskRead(u32 sd_addr, u8* dst, u16 slen);

#endif /* DISK_H */
