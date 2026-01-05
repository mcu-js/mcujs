/*
 * mcujs - Filesystem Implementation
 * 
 * FatFs wrapper providing filesystem operations.
 * Uses ChaN's FatFs library for FAT12/16 with Long Filename (LFN) support.
 * 
 * The disk I/O layer (diskio.c) handles flash read/write operations.
 * Direct sector access functions are provided for USB MSC.
 */

#include "fs.h"
#include "ff.h"
#include "../usb/usb_msc.h"

#include <string.h>
#include <stdlib.h>

/* External disk I/O functions for USB MSC (defined in diskio.c) */
extern uint32_t diskio_get_sector_count(void);
extern int diskio_read_sector(uint32_t sector, uint32_t offset, void *buffer, uint32_t size);
extern int diskio_write_sector(uint32_t sector, uint32_t offset, const void *buffer, uint32_t size);
extern void diskio_sync(void);

/* FatFs objects */
static FATFS s_fatfs;
static bool s_initialized = false;

/* File handle pool (FatFs FIL objects are ~550 bytes each with LFN) */
#define MAX_OPEN_FILES 4
static FIL s_fil_pool[MAX_OPEN_FILES];
static bool s_fil_used[MAX_OPEN_FILES];

/*
 * Convert fs_mode_t to FatFs mode flags
 */
static BYTE mode_to_fatfs(fs_mode_t mode) {
    BYTE fa_mode = 0;
    
    if (mode & FS_MODE_READ) {
        fa_mode |= FA_READ;
    }
    if (mode & FS_MODE_WRITE) {
        fa_mode |= FA_WRITE;
    }
    if (mode & FS_MODE_CREATE) {
        fa_mode |= FA_OPEN_ALWAYS;  /* Open if exists, create if not */
    }
    if (mode & FS_MODE_TRUNCATE) {
        fa_mode |= FA_CREATE_ALWAYS;  /* Create new, truncate if exists */
    }
    
    /* If only read mode, use FA_OPEN_EXISTING */
    if (fa_mode == FA_READ) {
        fa_mode = FA_READ | FA_OPEN_EXISTING;
    }
    
    return fa_mode;
}

/*
 * Convert FatFs result to fs_result_t
 */
static fs_result_t fresult_to_fs(FRESULT fr) {
    switch (fr) {
        case FR_OK:
            return FS_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:
            return FS_ERROR_NOT_FOUND;
        case FR_EXIST:
            return FS_ERROR_EXISTS;
        case FR_DENIED:
        case FR_WRITE_PROTECTED:
            return FS_ERROR_INVALID;
        case FR_DISK_ERR:
        case FR_INT_ERR:
        case FR_NOT_READY:
            return FS_ERROR_IO;
        case FR_INVALID_NAME:
        case FR_INVALID_PARAMETER:
            return FS_ERROR_INVALID;
        default:
            return FS_ERROR;
    }
}

/*
 * Initialize filesystem
 */
fs_result_t fs_init(void) {
    if (s_initialized) {
        return FS_OK;
    }
    
    /* Initialize file pool */
    memset(s_fil_used, 0, sizeof(s_fil_used));
    
    /* Mount the filesystem */
    FRESULT fr = f_mount(&s_fatfs, "", 1);  /* 1 = mount immediately */
    
    /* Format if no filesystem or filesystem is corrupted */
    if (fr == FR_NO_FILESYSTEM || fr == FR_DISK_ERR || fr == FR_INT_ERR) {
        /* No valid filesystem or corrupted - format it */
        MKFS_PARM mkfs_opt = {
            .fmt = FM_FAT,      /* FAT12/16 (auto-select based on size) */
            .n_fat = 2,         /* 2 FATs for redundancy */
            .align = 1,         /* Sector alignment */
            .n_root = 512,      /* Root directory entries */
            .au_size = 0        /* Auto cluster size */
        };
        
        /* Work buffer for f_mkfs (needs >= 512 bytes) */
        static BYTE mkfs_work[FF_MAX_SS];
        
        fr = f_mkfs("", &mkfs_opt, mkfs_work, sizeof(mkfs_work));
        if (fr != FR_OK) {
            return fresult_to_fs(fr);
        }
        
        /* Set volume label */
        f_setlabel("MCUJS");
        
        /* Mount again after formatting */
        fr = f_mount(&s_fatfs, "", 1);
    }
    
    if (fr != FR_OK) {
        return fresult_to_fs(fr);
    }
    
    s_initialized = true;
    return FS_OK;
}

/*
 * Sync filesystem to flash
 */
fs_result_t fs_sync(void) {
    if (!s_initialized) {
        return FS_ERROR;
    }
    
    /* Sync all open files */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (s_fil_used[i]) {
            f_sync(&s_fil_pool[i]);
        }
    }
    
    /* Sync disk I/O layer */
    diskio_sync();
    
    return FS_OK;
}

/*
 * Invalidate filesystem cache
 * Remounts FatFs to pick up changes made via USB MSC
 */
fs_result_t fs_invalidate(void) {
    if (!s_initialized) {
        return FS_OK;  /* Nothing to invalidate */
    }
    
    /* Close any open files first */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (s_fil_used[i]) {
            f_close(&s_fil_pool[i]);
            s_fil_used[i] = false;
        }
    }
    
    /* Unmount and remount to clear FatFs internal caches */
    f_mount(NULL, "", 0);  /* Unmount */
    
    FRESULT fr = f_mount(&s_fatfs, "", 1);  /* Remount */
    if (fr != FR_OK) {
        s_initialized = false;
        return fresult_to_fs(fr);
    }
    
    return FS_OK;
}

/*
 * Notify host that filesystem has changed
 */
void fs_notify_host(void) {
    usb_msc_media_changed();
}

/*
 * Get total number of sectors
 */
uint32_t fs_get_total_sectors(void) {
    return diskio_get_sector_count();
}

/*
 * Get free space in bytes
 */
uint32_t fs_get_free_space(void) {
    if (!s_initialized) {
        return 0;
    }
    
    DWORD free_clusters;
    FATFS *fs;
    
    FRESULT fr = f_getfree("", &free_clusters, &fs);
    if (fr != FR_OK) {
        return 0;
    }
    
    /* free_clusters * sectors_per_cluster * bytes_per_sector */
    return free_clusters * fs->csize * FS_SECTOR_SIZE;
}

/*
 * Read sector directly (for USB MSC)
 * Bypasses FatFs and reads directly from flash
 */
fs_result_t fs_read_sector(uint32_t sector, uint32_t offset,
                           void *buffer, uint32_t size) {
    if (diskio_read_sector(sector, offset, buffer, size) != 0) {
        return FS_ERROR_IO;
    }
    return FS_OK;
}

/*
 * Write sector directly (for USB MSC)
 * Bypasses FatFs and writes directly to flash
 */
fs_result_t fs_write_sector(uint32_t sector, uint32_t offset,
                            const void *buffer, uint32_t size) {
    if (diskio_write_sector(sector, offset, buffer, size) != 0) {
        return FS_ERROR_IO;
    }
    return FS_OK;
}

/*
 * Open a file
 */
fs_result_t fs_open(fs_file_t *file, const char *path, fs_mode_t mode) {
    if (file == NULL || path == NULL) {
        return FS_ERROR_INVALID;
    }
    
    if (!s_initialized) {
        fs_result_t res = fs_init();
        if (res != FS_OK) {
            return res;
        }
    }
    
    /* Find free file slot */
    int slot = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!s_fil_used[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        return FS_ERROR;  /* No free file slots */
    }
    
    /* Open the file */
    BYTE fa_mode = mode_to_fatfs(mode);
    FRESULT fr = f_open(&s_fil_pool[slot], path, fa_mode);
    
    if (fr != FR_OK) {
        return fresult_to_fs(fr);
    }
    
    /* Handle append mode */
    if (mode & FS_MODE_APPEND) {
        f_lseek(&s_fil_pool[slot], f_size(&s_fil_pool[slot]));
    }
    
    s_fil_used[slot] = true;
    file->internal = &s_fil_pool[slot];
    file->is_open = true;
    
    return FS_OK;
}

/*
 * Close a file
 */
fs_result_t fs_close(fs_file_t *file) {
    if (file == NULL || !file->is_open) {
        return FS_ERROR_INVALID;
    }
    
    FIL *fp = (FIL *)file->internal;
    
    /* Find and free the slot */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (&s_fil_pool[i] == fp) {
            FRESULT fr = f_close(fp);
            s_fil_used[i] = false;
            file->is_open = false;
            file->internal = NULL;
            
            /* Flush diskio cache to flash after close */
            diskio_sync();
            
            return fresult_to_fs(fr);
        }
    }
    
    return FS_ERROR_INVALID;
}

/*
 * Read from a file
 */
fs_result_t fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read) {
    if (file == NULL || !file->is_open || buffer == NULL) {
        return FS_ERROR_INVALID;
    }
    
    FIL *fp = (FIL *)file->internal;
    UINT br;
    
    FRESULT fr = f_read(fp, buffer, size, &br);
    
    if (bytes_read != NULL) {
        *bytes_read = br;
    }
    
    return fresult_to_fs(fr);
}

/*
 * Write to a file
 */
fs_result_t fs_write(fs_file_t *file, const void *buffer, size_t size, size_t *bytes_written) {
    if (file == NULL || !file->is_open || buffer == NULL) {
        return FS_ERROR_INVALID;
    }
    
    FIL *fp = (FIL *)file->internal;
    UINT bw;
    
    FRESULT fr = f_write(fp, buffer, size, &bw);
    
    if (bytes_written != NULL) {
        *bytes_written = bw;
    }
    
    return fresult_to_fs(fr);
}

/*
 * Seek within a file
 */
fs_result_t fs_seek(fs_file_t *file, uint32_t offset) {
    if (file == NULL || !file->is_open) {
        return FS_ERROR_INVALID;
    }
    
    FIL *fp = (FIL *)file->internal;
    FRESULT fr = f_lseek(fp, offset);
    
    return fresult_to_fs(fr);
}

/*
 * Get file size
 */
fs_result_t fs_size(fs_file_t *file, size_t *size) {
    if (file == NULL || !file->is_open || size == NULL) {
        return FS_ERROR_INVALID;
    }
    
    FIL *fp = (FIL *)file->internal;
    *size = f_size(fp);
    
    return FS_OK;
}

/*
 * Check if file/directory exists
 */
fs_result_t fs_exists(const char *path) {
    if (path == NULL) {
        return FS_ERROR_INVALID;
    }
    
    if (!s_initialized) {
        fs_result_t res = fs_init();
        if (res != FS_OK) {
            return res;
        }
    }
    
    FILINFO finfo;
    FRESULT fr = f_stat(path, &finfo);
    
    if (fr == FR_OK) {
        return FS_OK;
    }
    
    return FS_ERROR_NOT_FOUND;
}

/*
 * Remove a file
 */
fs_result_t fs_remove(const char *path) {
    if (path == NULL) {
        return FS_ERROR_INVALID;
    }
    
    if (!s_initialized) {
        return FS_ERROR;
    }
    
    FRESULT fr = f_unlink(path);
    return fresult_to_fs(fr);
}

/*
 * Rename a file
 */
fs_result_t fs_rename(const char *old_path, const char *new_path) {
    if (old_path == NULL || new_path == NULL) {
        return FS_ERROR_INVALID;
    }
    
    if (!s_initialized) {
        return FS_ERROR;
    }
    
    FRESULT fr = f_rename(old_path, new_path);
    return fresult_to_fs(fr);
}

/*
 * Create a directory
 */
fs_result_t fs_mkdir(const char *path) {
    if (path == NULL) {
        return FS_ERROR_INVALID;
    }
    
    if (!s_initialized) {
        fs_result_t res = fs_init();
        if (res != FS_OK) {
            return res;
        }
    }
    
    FRESULT fr = f_mkdir(path);
    return fresult_to_fs(fr);
}

/*
 * List directory contents
 */
fs_result_t fs_list_dir(const char *path, fs_dir_callback_t callback, void *user_data) {
    if (callback == NULL) {
        return FS_ERROR_INVALID;
    }
    
    if (!s_initialized) {
        fs_result_t res = fs_init();
        if (res != FS_OK) {
            return res;
        }
    }
    
    DIR dir;
    FILINFO finfo;
    fs_entry_t entry;
    
    /* Use root if path is NULL or empty */
    const char *dir_path = (path == NULL || path[0] == '\0') ? "/" : path;
    
    FRESULT fr = f_opendir(&dir, dir_path);
    if (fr != FR_OK) {
        return fresult_to_fs(fr);
    }
    
    while (1) {
        fr = f_readdir(&dir, &finfo);
        if (fr != FR_OK || finfo.fname[0] == 0) {
            break;  /* Error or end of directory */
        }
        
        /* Skip . and .. entries */
        if (finfo.fname[0] == '.') {
            continue;
        }
        
        /* Fill entry structure */
        strncpy(entry.name, finfo.fname, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
        entry.size = finfo.fsize;
        entry.is_dir = (finfo.fattrib & AM_DIR) != 0;
        
        /* Call callback */
        if (!callback(&entry, user_data)) {
            break;  /* Callback requested stop */
        }
    }
    
    f_closedir(&dir);
    
    return FS_OK;
}
