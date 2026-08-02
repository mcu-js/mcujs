#ifndef MCUJS_TEST_FF_H
#define MCUJS_TEST_FF_H

#include <stdint.h>

typedef uint8_t BYTE;
typedef unsigned int UINT;
typedef uint32_t DWORD;
typedef uint32_t FSIZE_t;

typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER,
} FRESULT;

typedef struct {
    DWORD csize;
} FATFS;

typedef struct {
    FSIZE_t size;
    FSIZE_t position;
} FIL;

typedef struct {
    unsigned marker;
} DIR;

typedef struct {
    FSIZE_t fsize;
    BYTE fattrib;
    char fname[65];
} FILINFO;

typedef struct {
    BYTE fmt;
    BYTE n_fat;
    UINT align;
    UINT n_root;
    DWORD au_size;
} MKFS_PARM;

#define FF_MAX_SS 512
#define FM_FAT 0x01
#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_ALWAYS 0x10
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_EXISTING 0x00
#define AM_DIR 0x10

FRESULT f_mount(FATFS *fs, const char *path, BYTE option);
FRESULT f_opendir(DIR *dir, const char *path);
FRESULT f_closedir(DIR *dir);
FRESULT f_mkfs(const char *path, const MKFS_PARM *options, void *work, UINT length);
FRESULT f_setlabel(const char *label);
FRESULT f_close(FIL *file);
FRESULT f_sync(FIL *file);
FRESULT f_getfree(const char *path, DWORD *clusters, FATFS **fs);
FRESULT f_open(FIL *file, const char *path, BYTE mode);
FRESULT f_lseek(FIL *file, FSIZE_t offset);
FSIZE_t f_size(FIL *file);
FRESULT f_read(FIL *file, void *buffer, UINT size, UINT *read);
FRESULT f_write(FIL *file, const void *buffer, UINT size, UINT *written);
FRESULT f_stat(const char *path, FILINFO *info);
FRESULT f_unlink(const char *path);
FRESULT f_rename(const char *old_path, const char *new_path);
FRESULT f_mkdir(const char *path);
FRESULT f_readdir(DIR *dir, FILINFO *info);

#endif /* MCUJS_TEST_FF_H */
