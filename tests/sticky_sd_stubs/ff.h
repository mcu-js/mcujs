#pragma once
#include <stdint.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned UINT;
typedef struct {int mounted;} FATFS;
typedef enum {FR_OK, FR_DISK_ERR, FR_NOT_READY, FR_NO_FILESYSTEM} FRESULT;
FRESULT f_mount(FATFS *, const char *, BYTE);
