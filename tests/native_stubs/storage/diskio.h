#ifndef MCUJS_TEST_STORAGE_DISKIO_H
#define MCUJS_TEST_STORAGE_DISKIO_H
#include "ff.h"
typedef uint32_t LBA_t;
typedef BYTE DSTATUS;
typedef enum { RES_OK, RES_ERROR, RES_WRPRT, RES_NOTRDY, RES_PARERR } DRESULT;
#define STA_NOINIT 1
#define STA_NODISK 2
#define STA_PROTECT 4
#define CTRL_SYNC 0
#define GET_SECTOR_COUNT 1
DSTATUS disk_status(BYTE drive);
DSTATUS disk_initialize(BYTE drive);
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer);
DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count);
DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count);
#endif
