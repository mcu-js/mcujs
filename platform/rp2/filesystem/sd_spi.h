/* SD v2 block-addressed SPI media. No formatting, erase, or TRIM. */
#ifndef MCUJS_SD_SPI_H
#define MCUJS_SD_SPI_H
#include "ff.h"
#include "diskio.h"
DSTATUS mcujs_sd_initialize(void);
DSTATUS mcujs_sd_status(void);
DRESULT mcujs_sd_read(BYTE *buffer, LBA_t sector, UINT count);
DRESULT mcujs_sd_write(const BYTE *buffer, LBA_t sector, UINT count);
DRESULT mcujs_sd_ioctl(BYTE command, void *buffer);
#endif
