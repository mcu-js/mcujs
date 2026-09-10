#ifndef SD_TEST_DISKIO_H
#define SD_TEST_DISKIO_H
#include "ff.h"
typedef BYTE DSTATUS;
typedef enum { RES_OK, RES_ERROR, RES_WRPRT, RES_NOTRDY, RES_PARERR } DRESULT;
#define STA_NOINIT 1
#define STA_NODISK 2
#define STA_PROTECT 4
#define CTRL_SYNC 0
#define GET_SECTOR_COUNT 1
#define GET_SECTOR_SIZE 2
#define GET_BLOCK_SIZE 3
#define CTRL_TRIM 4
#endif
