#pragma GCC diagnostic ignored "-Wunused-value"

#include "disk.h"

#include "bios.h"

#define CMD0 0x40   // software reset
#define CMD1 0x41   // brings card out of idle state
#define CMD8 0x48   // Reserved
#define CMD12 0x4C  // stop transmission on multiple block read
#define CMD17 0x51  // read single block
#define CMD18 0x52  // read multiple block
#define CMD58 0x7A  // reads the OCR register
#define CMD55 0x77
#define CMD41 0x69
#define CMD24 0x58  // writes a single block
#define CMD25 0x59  // writes a multi block
#define ACMD41 0x69
#define ACMD6 0x46
#define SD_V2 2
#define SD_HC 1

#define CMD2 0x42  // read cid
#define CMD3 0x43  // read rca
#define CMD7 0x47
#define CMD9 0x49
#define CMD6 0x46

#define R1 1
#define R2 2
#define R3 3
#define R6 6
#define R7 7

u8 bi_sd_cmd_val();

u8 sd_resp_buff[18];
u8 card_type;
static u32 disk_addr;

u8 diskCloseRW();
u32 diskCrc7(u8* buff, u32 len);

u32 diskCrc7(u8* buff, u32 len) {
  unsigned int a, crc = 0;

  while (len--) {
    crc ^= *buff++;
    a = 8;
    do {
      crc <<= 1;
      if (crc & (1 << 8))
        crc ^= 0x12;
    } while (--a);
  }
  return (crc & 0xfe);
}

u8 diskGetRespTypeSD(u8 cmd) {
  switch (cmd) {
    case CMD3:
      return R6;
    case CMD8:
      return R7;
    case CMD2:
    case CMD9:
      return R2;
    case CMD58:
    case CMD41:
      return R3;

    default:
      return R1;
  }
}

#define WAIT 2048

u8 diskCmdSD(u8 cmd, u32 arg) {
  u8 resp_type = diskGetRespTypeSD(cmd);
  u8 p = 0;
  u8 buff[6];
  volatile u32 i = 0;

  u8 resp_len = resp_type == R2 ? 17 : 6;
  u8 crc;
  buff[p++] = cmd;
  buff[p++] = (arg >> 24);
  buff[p++] = (arg >> 16);
  buff[p++] = (arg >> 8);
  buff[p++] = (arg >> 0);
  crc = diskCrc7(buff, 5) | 1;

  // biSDcmdWriteMode(0);
  // bi_sd_mode(SPI_MODE_CMD_W4);
  bi_sd_mode(SD_MODE8);

  bi_sd_cmd_wr(0xff);
  bi_sd_cmd_wr(cmd);
  bi_sd_cmd_wr(arg >> 24);
  bi_sd_cmd_wr(arg >> 16);
  bi_sd_cmd_wr(arg >> 8);
  bi_sd_cmd_wr(arg);
  bi_sd_cmd_wr(crc);

  if (cmd == CMD18)
    return 0;

  bi_sd_cmd_rd();
  bi_sd_mode(SD_MODE1);
  i = 0;
  for (;;) {
    if ((bi_sd_cmd_val() & 192) == 0)
      break;
    // if (bi_sd_cmd_val() != 0xff)break;
    // if ((bi_sd_cmd_val() & 192) != 0)break;
    if (i++ == WAIT)
      return DISK_ERR_CMD_TIMEOUT;
    bi_sd_cmd_rd();
  }

  bi_sd_mode(SD_MODE8);
  // bi_sd_mode(SPI_MODE_CMD_R4);

  sd_resp_buff[0] = bi_sd_cmd_rd();

  for (i = 1; i < resp_len - 1; i++) {
    sd_resp_buff[i] = bi_sd_cmd_rd();
  }
  sd_resp_buff[i] = bi_sd_cmd_val();

  if (resp_type != R3) {
    if (resp_type == R2) {
      crc = diskCrc7(sd_resp_buff + 1, resp_len - 2) | 1;
    } else {
      crc = diskCrc7(sd_resp_buff, resp_len - 1) | 1;
    }
    if (crc != sd_resp_buff[resp_len - 1])
      return DISK_ERR_CRC_ERROR;
  }

  return 0;
}

u8 diskInit() {
  u16 i;
  volatile u8 resp = 0;
  u32 rca;
  u32 wait_len = WAIT;

  card_type = 0;

  bi_sd_speed(SD_SPD_LO);

  bi_sd_mode(SD_MODE8);

  for (i = 0; i < 40; i++)
    bi_sd_dat_wr(0xff);
  resp = diskCmdSD(CMD0, 0x1aa);

  for (i = 0; i < 40; i++)
    bi_sd_dat_wr(0xff);

  resp = diskCmdSD(CMD8, 0x1aa);

  if (resp != 0 && resp != DISK_ERR_CMD_TIMEOUT) {
    return DISK_ERR_INIT + 0;
  }
  if (resp == 0)
    card_type |= SD_V2;

  if (card_type == SD_V2) {
    for (i = 0; i < wait_len; i++) {
      resp = diskCmdSD(CMD55, 0);
      if (resp)
        return DISK_ERR_INIT + 1;
      if ((sd_resp_buff[3] & 1) != 1)
        continue;
      resp = diskCmdSD(CMD41, 0x40300000);
      if ((sd_resp_buff[1] & 128) == 0)
        continue;

      break;
    }
  } else {
    i = 0;
    do {
      resp = diskCmdSD(CMD55, 0);
      if (resp)
        return DISK_ERR_INIT + 2;
      resp = diskCmdSD(CMD41, 0x40300000);
      if (resp)
        return DISK_ERR_INIT + 3;

    } while (sd_resp_buff[1] < 1 && i++ < wait_len);
  }

  if (i == wait_len)
    return DISK_ERR_INIT + 4;

  if ((sd_resp_buff[1] & 64) && card_type != 0)
    card_type |= SD_HC;

  resp = diskCmdSD(CMD2, 0);
  if (resp)
    return DISK_ERR_INIT + 5;

  resp = diskCmdSD(CMD3, 0);
  if (resp)
    return DISK_ERR_INIT + 6;

  resp = diskCmdSD(CMD7, 0);

  rca = (sd_resp_buff[1] << 24) | (sd_resp_buff[2] << 16) |
        (sd_resp_buff[3] << 8) | (sd_resp_buff[4] << 0);

  resp = diskCmdSD(CMD9, rca);  // get csd
  if (resp)
    return DISK_ERR_INIT + 7;

  resp = diskCmdSD(CMD7, rca);
  if (resp)
    return DISK_ERR_INIT + 8;

  resp = diskCmdSD(CMD55, rca);
  if (resp)
    return DISK_ERR_INIT + 9;

  resp = diskCmdSD(CMD6, 2);
  if (resp)
    return DISK_ERR_INIT + 10;

  bi_sd_speed(SD_SPD_HI);

  disk_addr = ~0;

  return 0;
}

u8 diskOpenRead(u32 saddr) {
  u8 resp;
  if ((card_type & SD_HC) == 0)
    saddr *= 512;

  resp = diskCmdSD(CMD18, saddr);
  if (resp)
    return DISK_ERR_RD1;

  return 0;
}

u8 diskRead(u32 sd_addr, u8* dst, u16 slen) {
  u8 resp;
  if (sd_addr != disk_addr) {
    resp = diskCloseRW();
    if (resp)
      return resp;
    resp = diskOpenRead(sd_addr);
    if (resp)
      return resp;
    disk_addr = sd_addr;
  }

  resp = bi_sd_dma_rd(dst, slen);
  if (resp)
    return DISK_ERR_RD2;

  disk_addr += slen;

  return 0;
}

u8 diskCloseRW() {
  u8 resp;
  u16 i;

  if (disk_addr == ~0)
    return 0;
  disk_addr = ~0;
  resp = diskCmdSD(CMD12, 0);
  if (resp)
    return DISK_ERR_CLOSE_RW1;

  // gVsync();
  // gVsync();

  bi_sd_mode(SD_MODE1);
  bi_sd_dat_rd();
  bi_sd_dat_rd();
  bi_sd_dat_rd();
  bi_sd_mode(SD_MODE2);

  i = 65535;
  while (--i) {
    if (bi_sd_dat_rd() == 0xff)
      break;
  }
  if (i == 0)
    return DISK_ERR_CLOSE_RW2;

  return 0;
}
