#pragma once
#include "ff.h"
#include "sdmmc_cmd.h"
typedef BYTE DSTATUS;
typedef enum {RES_OK, RES_ERROR, RES_WRPRT, RES_NOTRDY, RES_PARERR} DRESULT;
#define STA_NOINIT 1
#define STA_PROTECT 4
#define CTRL_SYNC 0
#define GET_SECTOR_COUNT 1
#define GET_SECTOR_SIZE 2
#define GET_BLOCK_SIZE 3
#define CTRL_TRIM 4
typedef struct {
 DSTATUS (*init)(BYTE);
 DSTATUS (*status)(BYTE);
 DRESULT (*read)(BYTE,BYTE *,DWORD,UINT);
 DRESULT (*write)(BYTE,const BYTE *,DWORD,UINT);
 DRESULT (*ioctl)(BYTE,BYTE,void *);
} ff_diskio_impl_t;
esp_err_t ff_diskio_get_drive(BYTE *);
void ff_diskio_register(BYTE, const ff_diskio_impl_t *);
#define ff_diskio_unregister(d) ff_diskio_register(d, NULL)
