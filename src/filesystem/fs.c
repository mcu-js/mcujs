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
#include "storage.h"
#include "ff.h"
#if MCUJS_HAS_SD
#include "diskio.h"
#endif
#include "../usb/usb_msc.h"
#include "../usb/usb_cdc.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* FatFs objects */
static FATFS s_fatfs;
static bool s_initialized = false;

typedef enum {
    STORAGE_UNINITIALIZED = 0,
    STORAGE_DEVICE_OWNED,
    STORAGE_CLAIMING_HOST,
    STORAGE_HOST_OWNED,
    STORAGE_RELEASING_HOST,
    STORAGE_FAULT,
} storage_state_t;

static storage_state_t s_storage_state = STORAGE_UNINITIALIZED;

/* File handle pool (FatFs FIL objects are ~550 bytes each with LFN) */
#define MAX_OPEN_FILES 4
static FIL s_fil_pool[MAX_OPEN_FILES];
static bool s_fil_used[MAX_OPEN_FILES];
static bool s_fil_sd[MAX_OPEN_FILES];
/* Directory callbacks can service USB; reject handoff until iteration ends. */
static unsigned s_directories[2];
#if MCUJS_HAS_SD
static FATFS s_sd_fatfs;
static bool s_sd_mounted;
static storage_state_t s_sd_state = STORAGE_DEVICE_OWNED;
#if MCUJS_USB_SD_MSC
static fs_result_t s_sd_fault = FS_OK;
/* Fixed for a host lease; native FatFs continues to address the whole card. */
static uint32_t s_sd_usb_start, s_sd_usb_sectors;
#endif
#endif

static bool is_sd_path(const char *path) {
    return path && !strncmp(path, "/sd", 3) && (!path[3] || path[3] == '/');
}
static int file_slot(const fs_file_t *file) {
    for (int i=0; i<MAX_OPEN_FILES; i++)
        if (s_fil_used[i] && file->internal == &s_fil_pool[i]) return i;
    return -1;
}
static bool has_sd_files(void) {
    for (int i=0; i<MAX_OPEN_FILES; i++)
        if (s_fil_used[i] && s_fil_sd[i]) return true;
    return false;
}

static bool has_open_files(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (s_fil_used[i] && !s_fil_sd[i]) return true;
    }
    return false;
}

fs_result_t fs_access_status(void) {
    switch (s_storage_state) {
        case STORAGE_DEVICE_OWNED:
            return s_initialized ? FS_OK : FS_ERROR_IO;
        case STORAGE_CLAIMING_HOST:
        case STORAGE_HOST_OWNED:
        case STORAGE_RELEASING_HOST:
            return FS_ERROR_BUSY;
        case STORAGE_FAULT:
            return FS_ERROR_IO;
        case STORAGE_UNINITIALIZED:
        default:
            return FS_ERROR;
    }
}

static fs_result_t ensure_initialized(void) {
    fs_result_t status = fs_access_status();
    if (status == FS_OK || status == FS_ERROR_BUSY || status == FS_ERROR_IO) {
        return status;
    }
    return fs_init();
}

/* Map a normalized /app path to the existing physical volume root. */
static const char *physical_path(char *logical) {
    if (is_sd_path(logical)) {
        /* Private drive prefix; raw caller-supplied drive prefixes are rejected. */
        if (!logical[3]) strcpy(logical, "1:/");
        else { memmove(logical + 2, logical + 3, strlen(logical + 3) + 1); logical[0]='1'; logical[1]=':'; }
        return logical;
    }
    return logical[4] ? logical + 4 : "/";
}

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
            return FS_ERROR_INVALID;
        case FR_WRITE_PROTECTED:
            return FS_ERROR_READ_ONLY;
        case FR_NO_FILESYSTEM:
            return FS_ERROR_UNSUPPORTED;
        case FR_NOT_READY:
            return FS_ERROR_NO_MEDIA;
        case FR_DISK_ERR:
        case FR_INT_ERR:
        case FR_INVALID_OBJECT:
            return FS_ERROR_IO;
        case FR_INVALID_NAME:
        case FR_INVALID_PARAMETER:
            return FS_ERROR_INVALID;
        default:
            return FS_ERROR;
    }
}

static fs_result_t volume_result(bool sd, FRESULT fr) {
#if MCUJS_HAS_SD
    if (sd && (fr == FR_DISK_ERR || fr == FR_INT_ERR || fr == FR_NOT_READY ||
               fr == FR_INVALID_OBJECT)) s_sd_mounted = false;
#else
    (void)sd;
#endif
    return fresult_to_fs(fr);
}

static fs_result_t path_ready(const char *path) {
    if (!is_sd_path(path)) return ensure_initialized();
#if MCUJS_HAS_SD
    if (s_sd_state == STORAGE_HOST_OWNED || s_sd_state == STORAGE_CLAIMING_HOST ||
        s_sd_state == STORAGE_RELEASING_HOST) return FS_ERROR_BUSY;
    if (s_sd_state == STORAGE_FAULT) return FS_ERROR_IO;
    if (s_sd_mounted) {
        if (disk_ioctl(1, CTRL_SYNC, NULL) == RES_OK) return FS_OK;
        s_sd_mounted = false;
        return FS_ERROR_IO;
    }
    /* Stale open handles must be closed before mounting another card. */
    if (has_sd_files()) return FS_ERROR_IO;
    if (f_mount(NULL, "1:", 0) != FR_OK) return FS_ERROR_IO;
    FRESULT fr=f_mount(&s_sd_fatfs, "1:", 1);
    s_sd_mounted = fr == FR_OK;
    /* Never format removable media, regardless of why mount failed. */
    return fresult_to_fs(fr);
#else
    return FS_ERROR_NOT_FOUND;
#endif
}

static fs_result_t prepare_path(const char *path, char logical[FS_PATH_MAX]) {
    fs_result_t result = fs_normalize_path(path, FS_APP_ROOT, logical, FS_PATH_MAX);
    if (result != FS_OK) {
        /* Preserve the existing host-ownership error precedence for app calls. */
        fs_result_t status=fs_access_status();
        if (!is_sd_path(path) && (status == FS_ERROR_BUSY || status == FS_ERROR_IO)) return status;
        return result;
    }
    return path_ready(logical);
}

static fs_result_t file_ready(const fs_file_t *file) {
    int slot=file_slot(file);
    if (slot >= 0 && s_fil_sd[slot]) {
#if MCUJS_HAS_SD
        if (!s_sd_mounted) return FS_ERROR_IO;
        return path_ready("/sd");
#else
        return FS_ERROR_NOT_FOUND;
#endif
    }
    return ensure_initialized();
}
static fs_result_t file_result(const fs_file_t *file, FRESULT fr) {
    int slot=file_slot(file);
    return volume_result(slot >= 0 && s_fil_sd[slot], fr);
}

/*
 * Check if mount error should trigger auto-format
 */
static bool should_auto_format(FRESULT fr) {
    switch (fr) {
        case FR_NO_FILESYSTEM:   /* No valid FAT volume */
        case FR_DISK_ERR:        /* Low-level disk I/O error */
        case FR_INT_ERR:         /* Internal FatFs error */
        case FR_NOT_ENABLED:     /* Workspace not registered */
        case FR_INVALID_OBJECT:  /* Invalid file/directory object */
            return true;
        default:
            return false;
    }
}

/*
 * Validate filesystem by attempting to read root directory
 * Returns true if filesystem is usable
 */
static bool validate_filesystem(void) {
    DIR dir;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        return false;
    }
    f_closedir(&dir);
    return true;
}

/*
 * Internal format function - performs the actual formatting
 * Note: f_setlabel requires filesystem to be mounted, so caller must
 * mount after calling this and then set the label.
 */
static fs_result_t do_format(void) {
    MKFS_PARM mkfs_opt = {
        .fmt = FM_FAT,      /* FAT12/16 (auto-select based on size) */
        .n_fat = 2,         /* 2 FATs for redundancy */
        .align = 1,         /* Sector alignment */
        .n_root = 512,      /* Root directory entries */
        .au_size = 0        /* Auto cluster size */
    };
    
    /* Work buffer for f_mkfs (needs >= 512 bytes) */
    static BYTE mkfs_work[FF_MAX_SS];
    
    FRESULT fr = f_mkfs("", &mkfs_opt, mkfs_work, sizeof(mkfs_work));
    if (fr != FR_OK) {
        return fresult_to_fs(fr);
    }
    
    return FS_OK;
}

/*
 * Initialize filesystem
 */
fs_result_t fs_init(void) {
    fs_result_t status = fs_access_status();
    if (status == FS_ERROR_BUSY || status == FS_ERROR_IO) {
        return status;
    }
    if (s_initialized) {
        s_storage_state = STORAGE_DEVICE_OWNED;
        return FS_OK;
    }
    
    
    /* Mount the filesystem */
    FRESULT fr = f_mount(&s_fatfs, "", 1);  /* 1 = mount immediately */
    
    /* Check if we need to auto-format */
    bool needs_format = false;
    
    if (should_auto_format(fr)) {
        needs_format = true;
    } else if (fr == FR_OK) {
        /* Mount succeeded - validate the filesystem is actually usable */
        if (!validate_filesystem()) {
            needs_format = true;
        }
    }
    
    if (needs_format) {
        usb_cdc_puts("Formatting filesystem...\r\n");
        
        fs_result_t fmt_result = do_format();
        if (fmt_result != FS_OK) {
            usb_cdc_puts("Format failed!\r\n");
            s_storage_state = STORAGE_FAULT;
            return fmt_result;
        }
        
        /* Mount again after formatting */
        fr = f_mount(&s_fatfs, "", 1);
        if (fr != FR_OK) {
            usb_cdc_puts("Mount after format failed!\r\n");
            s_storage_state = STORAGE_FAULT;
            return fresult_to_fs(fr);
        }
        
        /* Set volume label (must be done after mount) */
        f_setlabel("MCUJS");
        
        usb_cdc_puts("Filesystem ready\r\n");
    }
    
    if (fr != FR_OK) {
        s_storage_state = STORAGE_FAULT;
        return fresult_to_fs(fr);
    }
    
    s_initialized = true;
    s_storage_state = STORAGE_DEVICE_OWNED;
    return FS_OK;
}

/*
 * Format filesystem (public API)
 * Formats the filesystem, destroying all data
 */
fs_result_t fs_format(void) {
    if (s_storage_state == STORAGE_CLAIMING_HOST ||
        s_storage_state == STORAGE_HOST_OWNED ||
        s_storage_state == STORAGE_RELEASING_HOST || has_open_files() || has_sd_files()) {
        return FS_ERROR_BUSY;
    }

    if (s_initialized) {
        if (f_mount(NULL, "", 0) != FR_OK) {
            s_storage_state = STORAGE_FAULT;
            return FS_ERROR_IO;
        }
        s_initialized = false;
    }
    
    /* Perform format */
    fs_result_t result = do_format();
    if (result != FS_OK) {
        s_storage_state = STORAGE_FAULT;
        return result;
    }
    
    /* Remount */
    FRESULT fr = f_mount(&s_fatfs, "", 1);
    if (fr != FR_OK) {
        s_storage_state = STORAGE_FAULT;
        return fresult_to_fs(fr);
    }
    
    /* Set volume label (must be done after mount) */
    f_setlabel("MCUJS");
    
    /* Reinitialize file pool */
    memset(s_fil_used, 0, sizeof(s_fil_used));
    s_initialized = true;
    s_storage_state = STORAGE_DEVICE_OWNED;
    
    /* Notify USB host that media changed */
    usb_msc_media_changed();
    
    return FS_OK;
}

/*
 * Sync filesystem to flash
 */
fs_result_t fs_sync(void) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    
    /* Sync all open files */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (s_fil_used[i] && !s_fil_sd[i]) {
            if (f_sync(&s_fil_pool[i]) != FR_OK) {
                return FS_ERROR_IO;
            }
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
    /* Host writes become visible through the fresh mount performed by
     * fs_end_host_access(). Never remount lazily while host-owned. */
    return ensure_initialized();
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
    if (ensure_initialized() != FS_OK) {
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

bool fs_host_owned(void) {
    return s_storage_state == STORAGE_HOST_OWNED;
}

bool fs_storage_ready(void) {
    return fs_access_status() == FS_OK;
}

fs_result_t fs_begin_host_access(void) {
    fs_result_t ready = fs_access_status();
    if (ready != FS_OK) {
        return ready;
    }
    if (!s_initialized || has_open_files() || s_directories[0]) {
        return FS_ERROR_BUSY;
    }

    fs_result_t sync_result = fs_sync();
    if (sync_result != FS_OK) {
        s_storage_state = STORAGE_FAULT;
        return sync_result;
    }
    s_storage_state = STORAGE_CLAIMING_HOST;
    if (f_mount(NULL, "", 0) != FR_OK) {
        s_storage_state = STORAGE_FAULT;
        return FS_ERROR_IO;
    }
    s_initialized = false;
    s_storage_state = STORAGE_HOST_OWNED;
    return FS_OK;
}

fs_result_t fs_end_host_access(void) {
    if (s_storage_state != STORAGE_HOST_OWNED) {
        return s_storage_state == STORAGE_FAULT ? FS_ERROR_IO : FS_ERROR_BUSY;
    }

    s_storage_state = STORAGE_RELEASING_HOST;
    FRESULT mount_result = f_mount(&s_fatfs, "", 1);
    if (mount_result != FR_OK || !validate_filesystem()) {
        if (mount_result == FR_OK) {
            (void)f_mount(NULL, "", 0);
        }
        s_initialized = false;
        s_storage_state = STORAGE_FAULT;
        return FS_ERROR_IO;
    }

    s_initialized = true;
    s_storage_state = STORAGE_DEVICE_OWNED;
    return FS_OK;
}

fs_result_t fs_msc_sync(void) {
    if (!fs_host_owned()) {
        return FS_ERROR_BUSY;
    }
    diskio_sync();
    return FS_OK;
}

/*
 * Read sector directly (for USB MSC)
 * Bypasses FatFs and reads directly from flash
 */
fs_result_t fs_read_sector(uint32_t sector, uint32_t offset,
                           void *buffer, uint32_t size) {
    if (buffer == NULL) {
        return FS_ERROR_INVALID;
    }
    if (!fs_host_owned()) {
        return FS_ERROR_BUSY;
    }
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
    if (buffer == NULL) {
        return FS_ERROR_INVALID;
    }
    if (!fs_host_owned()) {
        return FS_ERROR_BUSY;
    }
    if (diskio_write_sector(sector, offset, buffer, size) != 0) {
        return FS_ERROR_IO;
    }
    return FS_OK;
}

#if MCUJS_USB_SD_MSC
#if !MCUJS_HAS_SD
#error "SD MSC requires an SD backend"
#endif
static fs_result_t sd_disk_result(DRESULT result) {
    switch (result) {
        case RES_OK: return FS_OK;
        case RES_WRPRT: return FS_ERROR_READ_ONLY;
        case RES_NOTRDY: return FS_ERROR_NO_MEDIA;
        case RES_PARERR: return FS_ERROR_INVALID;
        default: return FS_ERROR_IO;
    }
}

/* A failed host transaction fences this lease. Never initialize a replacement
 * card underneath the host's cached FAT, or retry a partial write automatically. */
static fs_result_t sd_host_result(DRESULT result) {
    fs_result_t status = sd_disk_result(result);
    if (status == FS_ERROR_IO || status == FS_ERROR_NO_MEDIA) s_sd_fault = status;
    return status;
}

static uint16_t sd_le16(const BYTE *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t sd_le32(const BYTE *p) {
    return (uint32_t)sd_le16(p) | (uint32_t)sd_le16(p + 2) << 16;
}

/* Match R0.16 check_fs recognition, including legacy FAT12/16 without a
 * signature/type string. Recognition and geometry are separate: a header
 * native FatFs recognizes must not be skipped in favour of another volume. */
static bool sd_fat_header(const BYTE *b) {
    if (b[0] != 0xeb && b[0] != 0xe9 && b[0] != 0xe8) return false;
    if (sd_le16(b + 510) == 0xaa55 && !memcmp(b + 82, "FAT32   ", 8)) return true;
    uint32_t cluster = b[13];
    return sd_le16(b + 11) == FS_SECTOR_SIZE && cluster && !(cluster & (cluster - 1)) &&
        sd_le16(b + 14) && (b[16] == 1 || b[16] == 2) && sd_le16(b + 17) &&
        (sd_le16(b + 19) >= 128 || sd_le32(b + 32) >= 0x10000) && sd_le16(b + 22);
}

/* Validate the BPB before publishing any USB capacity. Do not infer the end
 * from cluster count: FAT volumes may legitimately include trailing padding. */
static bool sd_fat_sectors(const BYTE *b, uint32_t limit, uint32_t *sectors) {
    if (!sd_fat_header(b)) return false;
    uint32_t total = sd_le16(b + 19), fat = sd_le16(b + 22);
    uint32_t reserved = sd_le16(b + 14), roots = sd_le16(b + 17);
    uint32_t cluster = b[13], fats = b[16];
    if (total && sd_le32(b + 32)) return false;
    if (!total) total = sd_le32(b + 32);
    if (!fat) fat = sd_le32(b + 36);
    if (sd_le16(b + 11) != FS_SECTOR_SIZE || !cluster ||
        (cluster & (cluster - 1)) || !reserved || (fats != 1 && fats != 2) ||
        !fat || roots % 16 || total < 128 || total > limit) return false;
    uint64_t overhead = reserved + (uint64_t)fats * fat + roots / 16;
    if (overhead >= total) return false;
    uint32_t clusters = (total - overhead) / cluster;
    if (!clusters || clusters > 0x0ffffff5) return false;
    /* Match FatFs R0.16's FAT subtype thresholds. */
    uint32_t bits = clusters <= 0xff5 ? 12 : clusters <= 0xfff5 ? 16 : 32;
    if (((uint64_t)clusters + 2) * bits > (uint64_t)fat * FS_SECTOR_SIZE * 8)
        return false;
    if (bits == 32) {
        uint32_t root = sd_le32(b + 44);
        uint16_t info = sd_le16(b + 48), backup = sd_le16(b + 50);
        if (sd_le16(b + 510) != 0xaa55 || roots || sd_le16(b + 22) ||
            sd_le16(b + 42) || root < 2 || root >= clusters + 2 ||
            (info != 0xffff && info >= reserved) ||
            (backup != 0xffff && backup >= reserved)) return false;
    } else if (!roots || !sd_le16(b + 22)) {
        return false;
    }
    *sectors = total;
    return true;
}

/* Only MBR/VBR headers are needed. In particular, never probe CSD count - 1.
 * Check partition extents BEFORE reading a VBR or asking FatFs to mount it. */
static fs_result_t sd_usb_geometry(uint32_t *start, uint32_t *sectors) {
    LBA_t count = 0;
    fs_result_t result = sd_disk_result(disk_ioctl(1, GET_SECTOR_COUNT, &count));
    if (result != FS_OK) return result;
    if (count < 128) return FS_ERROR_UNSUPPORTED;
#if FF_LBA64
    if (count > UINT32_MAX) return FS_ERROR_UNSUPPORTED;
#endif
    BYTE boot[FS_SECTOR_SIZE];
    result = sd_disk_result(disk_read(1, boot, 0, 1));
    if (result != FS_OK) return result;
    *start = 0;
    if (sd_fat_header(boot))
        return sd_fat_sectors(boot, (uint32_t)count, sectors) ? FS_OK : FS_ERROR_UNSUPPORTED;
    if (sd_le16(boot + 510) != 0xaa55) return FS_ERROR_UNSUPPORTED;
    BYTE table[64];
    memcpy(table, boot + 446, sizeof(table));
#if FF_LBA64
    if (table[4] == 0xee) return FS_ERROR_UNSUPPORTED; /* No unchecked GPT scan. */
#endif
    /* FatFs scans by start LBA, regardless of type. Validate every possible
     * location before selecting a VBR, not only entries before the first FAT. */
    for (unsigned i = 0; i < 4; i++) {
        const BYTE *entry = table + i * 16;
        uint32_t first = sd_le32(entry + 8), length = sd_le32(entry + 12);
        if (!first && !entry[4] && !length) continue;
        if (!first || first >= count || length < 128 || length > count - first)
            return FS_ERROR_UNSUPPORTED;
    }
    for (unsigned i = 0; i < 4; i++) {
        const BYTE *entry = table + i * 16;
        uint32_t first = sd_le32(entry + 8), length = sd_le32(entry + 12);
        if (!first) continue; /* Extent was validated above. */
        result = sd_disk_result(disk_read(1, boot, first, 1));
        if (result != FS_OK) return result;
        if (sd_fat_header(boot)) {
            if (!sd_fat_sectors(boot, length, sectors)) return FS_ERROR_UNSUPPORTED;
            *start = first;
            return FS_OK;
        }
        /* A malformed FAT partition must not turn into a raw-card export or
         * silently select a different FAT volume. Skip only non-FAT entries. */
        if (entry[4] == 0x01 || entry[4] == 0x04 || entry[4] == 0x06 ||
            entry[4] == 0x0b || entry[4] == 0x0c || entry[4] == 0x0e)
            return FS_ERROR_UNSUPPORTED;
    }
    return FS_ERROR_UNSUPPORTED;
}
#endif

bool fs_volume_host_owned(uint8_t volume) {
    if (volume == 0) return fs_host_owned();
#if MCUJS_USB_SD_MSC
    if (volume == 1) return s_sd_state == STORAGE_HOST_OWNED;
#endif
    return false;
}

fs_result_t fs_volume_begin_host_access(uint8_t volume) {
    if (volume == 0) return fs_begin_host_access();
#if MCUJS_USB_SD_MSC
    if (volume == 1) {
        if (has_sd_files() || s_directories[1]) return FS_ERROR_BUSY;
        if (s_sd_state != STORAGE_DEVICE_OWNED)
            return s_sd_state == STORAGE_FAULT ? FS_ERROR_IO : FS_ERROR_BUSY;
        if (disk_status(1) & STA_NOINIT) {
            s_sd_mounted = false;
            if (disk_initialize(1) & STA_NOINIT) return FS_ERROR_NO_MEDIA;
        }
        uint32_t start, sectors;
        fs_result_t geometry = sd_usb_geometry(&start, &sectors);
        if (geometry != FS_OK) {
            if (geometry == FS_ERROR_IO || geometry == FS_ERROR_NO_MEDIA) s_sd_mounted = false;
            return geometry;
        }
        fs_result_t ready = path_ready("/sd");
        if (ready != FS_OK) return ready;
        /* Fail closed if native auto-detection selected a different volume. */
        if (s_sd_fatfs.volbase != start) return FS_ERROR_UNSUPPORTED;
        ready = sd_disk_result(disk_ioctl(1, CTRL_SYNC, NULL));
        if (ready != FS_OK) { s_sd_mounted = false; return ready; }
        s_sd_state = STORAGE_CLAIMING_HOST;
        if (f_mount(NULL, "1:", 0) != FR_OK) {
            s_sd_state = STORAGE_FAULT;
            return FS_ERROR_IO;
        }
        s_sd_mounted = false;
        s_sd_fault = FS_OK;
        s_sd_usb_start = start;
        s_sd_usb_sectors = sectors;
        s_sd_state = STORAGE_HOST_OWNED;
        return FS_OK;
    }
#endif
    return FS_ERROR_UNSUPPORTED;
}

fs_result_t fs_volume_end_host_access(uint8_t volume) {
    if (volume == 0) return fs_end_host_access();
#if MCUJS_USB_SD_MSC
    if (volume == 1) {
        if (!fs_volume_host_owned(volume)) return FS_ERROR_BUSY;
        /* The adapter confirms sync before accepting eject. */
        if (s_sd_fault != FS_OK) return s_sd_fault;
        s_sd_state = STORAGE_RELEASING_HOST;
        /* Host may have edited its boot sector. Revalidate before native
         * autodetection, and never expand beyond the volume just released. */
        uint32_t start, sectors;
        fs_result_t geometry = sd_usb_geometry(&start, &sectors);
        if (geometry != FS_OK || start != s_sd_usb_start || sectors > s_sd_usb_sectors) {
            s_sd_mounted = false;
            s_sd_state = STORAGE_FAULT;
            return geometry != FS_OK ? geometry : FS_ERROR_UNSUPPORTED;
        }
        FRESULT fr = f_mount(&s_sd_fatfs, "1:", 1);
        s_sd_mounted = fr == FR_OK;
        s_sd_state = fr == FR_OK ? STORAGE_DEVICE_OWNED : STORAGE_FAULT;
        return fresult_to_fs(fr); /* Never format, even after a host write. */
    }
#endif
    return FS_ERROR_UNSUPPORTED;
}

fs_result_t fs_volume_msc_status(uint8_t volume) {
    if (!fs_volume_host_owned(volume)) return FS_ERROR_BUSY;
#if MCUJS_USB_SD_MSC
    if (volume == 1) {
        if (s_sd_fault != FS_OK) return s_sd_fault;
        /* Cheap lease state only. Each driver read/write validates CID and
         * CRC; probing here duplicates wire transactions for every USB chunk.
         * Explicit readiness probes and sync/eject use msc_sync below. */
        return (disk_status(1) & STA_NOINIT) ? sd_host_result(RES_NOTRDY) : FS_OK;
    }
#endif
    return volume == 0 ? FS_OK : FS_ERROR_UNSUPPORTED;
}

fs_result_t fs_volume_msc_sync(uint8_t volume) {
    if (volume == 0) return fs_msc_sync();
    fs_result_t result = fs_volume_msc_status(volume);
    if (result != FS_OK) return result;
#if MCUJS_USB_SD_MSC
    if (volume == 1) return sd_host_result(disk_ioctl(1, CTRL_SYNC, NULL));
#endif
    return FS_ERROR_UNSUPPORTED;
}

fs_result_t fs_volume_capacity(uint8_t volume, uint32_t *sectors) {
    if (!sectors) return FS_ERROR_INVALID;
    *sectors = 0;
    fs_result_t status = fs_volume_msc_status(volume);
    if (status != FS_OK) return status;
    if (volume == 0) { *sectors = fs_get_total_sectors(); return FS_OK; }
#if MCUJS_USB_SD_MSC
    if (volume == 1) {
        *sectors = s_sd_usb_sectors;
        return FS_OK;
    }
#endif
    return FS_ERROR_UNSUPPORTED;
}

bool fs_volume_writable(uint8_t volume) {
    if (fs_volume_msc_status(volume) != FS_OK) return false;
#if MCUJS_USB_SD_MSC
    if (volume == 1) return !(disk_status(1) & STA_PROTECT);
#endif
    return volume == 0;
}

/* TinyUSB supplies a sector plus a byte offset. Keep RMW private to one call;
 * overflow-safe bounds and no persistent scratch/cache across media changes. */
static fs_result_t volume_transfer(uint8_t volume, uint32_t sector, uint32_t offset,
                                    void *buffer, uint32_t size, bool write) {
    if (!buffer || offset >= FS_SECTOR_SIZE || size > FS_SECTOR_SIZE - offset)
        return FS_ERROR_INVALID;
    uint32_t count;
    fs_result_t result = fs_volume_capacity(volume, &count);
    if (result != FS_OK) return result;
    if (sector >= count) return FS_ERROR_INVALID;
    if (volume == 0) return write ? fs_write_sector(sector, offset, buffer, size)
                                  : fs_read_sector(sector, offset, buffer, size);
#if MCUJS_USB_SD_MSC
    if (volume == 1) {
        if (write && (disk_status(1) & STA_PROTECT)) return FS_ERROR_READ_ONLY;
        if (!size) return FS_OK;
        /* Validated start + count fits the card and uint32_t; sector < count. */
        sector += s_sd_usb_start;
        BYTE scratch[FS_SECTOR_SIZE];
        if (!write || offset || size != FS_SECTOR_SIZE) {
            result = sd_host_result(disk_read(1, scratch, sector, 1));
            if (result != FS_OK) return result; /* No write on failed RMW read. */
        }
        if (!write) { memcpy(buffer, scratch + offset, size); return FS_OK; }
        if (!offset && size == FS_SECTOR_SIZE)
            return sd_host_result(disk_write(1, buffer, sector, 1));
        memcpy(scratch + offset, buffer, size);
        return sd_host_result(disk_write(1, scratch, sector, 1));
    }
#endif
    return FS_ERROR_UNSUPPORTED;
}

fs_result_t fs_volume_read_sector(uint8_t volume, uint32_t sector, uint32_t offset,
                                  void *buffer, uint32_t size) {
    return volume_transfer(volume, sector, offset, buffer, size, false);
}

fs_result_t fs_volume_write_sector(uint8_t volume, uint32_t sector, uint32_t offset,
                                   const void *buffer, uint32_t size) {
    return volume_transfer(volume, sector, offset, (void *)buffer, size, true);
}

/*
 * Open a file
 */
fs_result_t fs_open(fs_file_t *file, const char *path, fs_mode_t mode) {
    if (file == NULL || path == NULL) {
        return FS_ERROR_INVALID;
    }

    file->internal = NULL;
    file->is_open = false;
    char logical[FS_PATH_MAX];
    fs_result_t ready = prepare_path(path, logical);
    if (ready != FS_OK) {
        return ready;
    }
    
    if (strcmp(logical, "/") == 0 || strcmp(logical, FS_APP_ROOT) == 0 || strcmp(logical, "/sd") == 0)
        return FS_ERROR_INVALID;

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
    bool sd=is_sd_path(logical);
    BYTE fa_mode = mode_to_fatfs(mode);
    FRESULT fr = f_open(&s_fil_pool[slot], physical_path(logical), fa_mode);
    
    if (fr != FR_OK) {
        return volume_result(sd, fr);
    }
    
    /* Handle append mode */
    if (mode & FS_MODE_APPEND) {
        FRESULT seek_result=f_lseek(&s_fil_pool[slot], f_size(&s_fil_pool[slot]));
        if (seek_result != FR_OK) {
            (void)f_close(&s_fil_pool[slot]);
            return volume_result(sd, seek_result);
        }
    }
    
    s_fil_used[slot] = true;
    s_fil_sd[slot] = sd;
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
            bool sd=s_fil_sd[i];
            /* Never flush an old FIL into replacement media after an I/O fault. */
            fs_result_t ready = sd ? file_ready(file) : FS_OK;
            FRESULT fr = ready == FS_OK ? f_close(fp) : FR_DISK_ERR;
            s_fil_used[i] = false;
            file->is_open = false;
            file->internal = NULL;
            
            /* Flush diskio cache to flash after close */
            if (!sd) diskio_sync();
            return volume_result(sd, fr);
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
    fs_result_t ready = file_ready(file);
    if (ready != FS_OK) {
        return ready;
    }

    FIL *fp = (FIL *)file->internal;
    UINT br;
    
    FRESULT fr = f_read(fp, buffer, size, &br);
    
    if (bytes_read != NULL) {
        *bytes_read = br;
    }
    
    return file_result(file, fr);
}

/*
 * Write to a file
 */
fs_result_t fs_write(fs_file_t *file, const void *buffer, size_t size, size_t *bytes_written) {
    if (file == NULL || !file->is_open || buffer == NULL) {
        return FS_ERROR_INVALID;
    }
    fs_result_t ready = file_ready(file);
    if (ready != FS_OK) {
        return ready;
    }

    FIL *fp = (FIL *)file->internal;
    UINT bw;
    
    FRESULT fr = f_write(fp, buffer, size, &bw);
    
    if (bytes_written != NULL) {
        *bytes_written = bw;
    }
    
    if (fr == FR_OK && bw != size) return FS_ERROR_NO_SPACE;
    return file_result(file, fr);
}

/*
 * Seek within a file
 */
fs_result_t fs_seek(fs_file_t *file, uint32_t offset) {
    if (file == NULL || !file->is_open) {
        return FS_ERROR_INVALID;
    }
    fs_result_t ready = file_ready(file);
    if (ready != FS_OK) {
        return ready;
    }

    FIL *fp = (FIL *)file->internal;
    FRESULT fr = f_lseek(fp, offset);
    
    return file_result(file, fr);
}

/*
 * Get file size
 */
fs_result_t fs_size(fs_file_t *file, size_t *size) {
    if (file == NULL || !file->is_open || size == NULL) {
        return FS_ERROR_INVALID;
    }
    fs_result_t ready = file_ready(file);
    if (ready != FS_OK) {
        return ready;
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

    char logical[FS_PATH_MAX];
    fs_result_t ready = prepare_path(path, logical);
    if (ready != FS_OK) {
        return ready;
    }

    if (strcmp(logical, "/") == 0 || strcmp(logical, FS_APP_ROOT) == 0 || strcmp(logical, "/sd") == 0) return FS_OK;
    bool sd=is_sd_path(logical);
    FILINFO finfo;
    return volume_result(sd, f_stat(physical_path(logical), &finfo));
}

/*
 * Remove a file
 */
fs_result_t fs_remove(const char *path) {
    if (path == NULL) {
        return FS_ERROR_INVALID;
    }

    char logical[FS_PATH_MAX];
    fs_result_t ready = prepare_path(path, logical);
    if (ready != FS_OK) {
        return ready;
    }
    if (strcmp(logical, "/") == 0 || strcmp(logical, FS_APP_ROOT) == 0 || strcmp(logical, "/sd") == 0)
        return FS_ERROR_INVALID;
    bool sd=is_sd_path(logical);
    return volume_result(sd, f_unlink(physical_path(logical)));
}

/*
 * Rename a file
 */
fs_result_t fs_rename(const char *old_path, const char *new_path) {
    if (old_path == NULL || new_path == NULL) {
        return FS_ERROR_INVALID;
    }

    char old_logical[FS_PATH_MAX], new_logical[FS_PATH_MAX];
    fs_result_t ready = prepare_path(old_path, old_logical);
    if (ready != FS_OK) return ready;
    fs_result_t result = fs_normalize_path(new_path, FS_APP_ROOT, new_logical, sizeof(new_logical));
    if (result != FS_OK) return result;
    if (strcmp(old_logical, "/") == 0 || strcmp(old_logical, FS_APP_ROOT) == 0 ||
        strcmp(new_logical, "/") == 0 || strcmp(new_logical, FS_APP_ROOT) == 0 ||
        !strcmp(old_logical, "/sd") || !strcmp(new_logical, "/sd"))
        return FS_ERROR_INVALID;
    bool sd=is_sd_path(old_logical);
    if (sd != is_sd_path(new_logical)) return FS_ERROR_CROSS_DEVICE;
    return volume_result(sd, f_rename(physical_path(old_logical), physical_path(new_logical)));
}

/*
 * Create a directory
 */
fs_result_t fs_mkdir(const char *path) {
    if (path == NULL) {
        return FS_ERROR_INVALID;
    }

    char logical[FS_PATH_MAX];
    fs_result_t ready = prepare_path(path, logical);
    if (ready != FS_OK) {
        return ready;
    }
    if (strcmp(logical, "/") == 0 || strcmp(logical, FS_APP_ROOT) == 0 || strcmp(logical, "/sd") == 0)
        return FS_ERROR_INVALID;
    bool sd=is_sd_path(logical);
    return volume_result(sd, f_mkdir(physical_path(logical)));
}

/*
 * List directory contents
 */
fs_result_t fs_list_dir(const char *path, fs_dir_callback_t callback, void *user_data) {
    if (callback == NULL) {
        return FS_ERROR_INVALID;
    }

    char logical[FS_PATH_MAX];
    fs_result_t ready = prepare_path(path, logical);
    if (ready != FS_OK) {
        return ready;
    }
    
    DIR dir;
    FILINFO finfo;
    fs_entry_t entry;
    
    if (strcmp(logical, "/") == 0) {
        const fs_entry_t app = {.name = "app", .is_dir = true, .size = 0};
        bool more=callback(&app, user_data);
#if MCUJS_HAS_SD
        const fs_entry_t sd = {.name = "sd", .is_dir = true, .size = 0};
        if (more) (void)callback(&sd, user_data);
#else
        (void)more;
#endif
        return FS_OK;
    }
    bool sd=is_sd_path(logical);
    const char *dir_path = physical_path(logical);
    
    FRESULT fr = f_opendir(&dir, dir_path);
    if (fr != FR_OK) {
        return volume_result(sd, fr);
    }
    
    s_directories[sd ? 1 : 0]++;
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
    
    FRESULT closed=f_closedir(&dir);
    s_directories[sd ? 1 : 0]--;
    return volume_result(sd, fr != FR_OK ? fr : closed);
}
