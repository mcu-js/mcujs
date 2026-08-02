/* MCU.js ESP32-S3 partition-backed filesystem. */

#include "fs.h"
#include "filesystem.h"

#include "esp_err.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "diskio_impl.h"
#include "diskio_wl.h"
#include "ff.h"
#include "wear_levelling.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MCUJS_FS_BASE_PATH "/mcujs"
#define MCUJS_FS_PARTITION_LABEL "ffat"
#define MCUJS_FS_PARTITION_OFFSET 0x450000u
#define MCUJS_FS_PARTITION_SIZE 0x3b0000u
#define MCUJS_FS_PATH_MAX 192
#define MCUJS_FS_VOLUME_LABEL "MCUJS"
#define MCUJS_FS_VOLUME_LABEL_UTF8_MAX (11u * 3u + 1u)

typedef struct {
    FILE *stream;
} esp_fs_file_t;

static const esp_vfs_fat_mount_config_t s_mount_config = {
    .format_if_mount_failed = false,
    .max_files = 8,
    .allocation_unit_size = 4096,
    .disk_status_check_enable = false,
    .use_one_fat = false,
};

static const esp_partition_t *s_partition;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static BYTE s_pdrv = 0xff;
static FATFS *s_fatfs;
static bool s_initialized;
static TaskHandle_t s_device_task;
static atomic_uint s_open_files;

typedef enum {
    STORAGE_UNINITIALIZED = 0,
    STORAGE_DEVICE_OWNED,
    STORAGE_CLAIMING_HOST,
    STORAGE_HOST_OWNED,
    STORAGE_RELEASING_HOST,
    STORAGE_FAULT,
} storage_state_t;

static atomic_int s_storage_state = STORAGE_UNINITIALIZED;

fs_result_t fs_access_status(void) {
    switch ((storage_state_t)atomic_load(&s_storage_state)) {
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

bool fs_storage_ready(void) {
    return fs_access_status() == FS_OK;
}

static bool is_device_task(void) {
    return s_device_task == NULL || xTaskGetCurrentTaskHandle() == s_device_task;
}

static const esp_partition_t *find_valid_partition(void) {
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, MCUJS_FS_PARTITION_LABEL);
    if (partition == NULL || partition->address != MCUJS_FS_PARTITION_OFFSET ||
        partition->size != MCUJS_FS_PARTITION_SIZE ||
        partition->address + partition->size != 0x800000u) {
        return NULL;
    }
    return partition;
}

bool mcujs_filesystem_partition_is_erased(void) {
    const esp_partition_t *partition = find_valid_partition();
    if (partition == NULL) {
        return false;
    }

    uint8_t buffer[256];
    for (size_t offset = 0; offset < partition->size; offset += sizeof(buffer)) {
        size_t size = partition->size - offset;
        if (size > sizeof(buffer)) {
            size = sizeof(buffer);
        }
        if (esp_partition_read(partition, offset, buffer, size) != ESP_OK) {
            return false;
        }
        for (size_t index = 0; index < size; index++) {
            if (buffer[index] != 0xff) {
                return false;
            }
        }
    }
    return true;
}

static fs_result_t errno_to_fs(int error) {
    switch (error) {
        case 0:
            return FS_OK;
        case ENOENT:
            return FS_ERROR_NOT_FOUND;
        case EEXIST:
            return FS_ERROR_EXISTS;
        case ENOSPC:
            return FS_ERROR_NO_SPACE;
        case EINVAL:
        case ENAMETOOLONG:
        case EACCES:
        case EPERM:
            return FS_ERROR_INVALID;
        default:
            return FS_ERROR_IO;
    }
}

static bool path_has_parent_segment(const char *path) {
    const char *cursor = path;
    while (*cursor != '\0') {
        while (*cursor == '/') {
            cursor++;
        }
        const char *segment = cursor;
        while (*cursor != '\0' && *cursor != '/') {
            cursor++;
        }
        if ((cursor - segment) == 2 && segment[0] == '.' && segment[1] == '.') {
            return true;
        }
    }
    return false;
}

static fs_result_t translate_path(const char *path, char *translated, size_t translated_size) {
    if (path == NULL || translated == NULL || translated_size == 0 ||
        path[0] == '\0' || strchr(path, '\\') != NULL || path_has_parent_segment(path)) {
        return FS_ERROR_INVALID;
    }

    int written;
    if (strcmp(path, "/") == 0) {
        written = snprintf(translated, translated_size, "%s", MCUJS_FS_BASE_PATH);
    } else if (path[0] == '/') {
        written = snprintf(translated, translated_size, "%s%s", MCUJS_FS_BASE_PATH, path);
    } else {
        written = snprintf(translated, translated_size, "%s/%s", MCUJS_FS_BASE_PATH, path);
    }

    if (written < 0 || (size_t)written >= translated_size) {
        return FS_ERROR_INVALID;
    }
    return FS_OK;
}

static fs_result_t ensure_initialized(void) {
    storage_state_t state = (storage_state_t)atomic_load(&s_storage_state);
    if (state == STORAGE_HOST_OWNED || state == STORAGE_CLAIMING_HOST ||
        state == STORAGE_RELEASING_HOST) {
        return FS_ERROR_BUSY;
    }
    if (state == STORAGE_FAULT) {
        return FS_ERROR_IO;
    }
    if (!is_device_task()) {
        return FS_ERROR_BUSY;
    }
    return s_initialized ? FS_OK : fs_init();
}

static void drive_name(BYTE pdrv, char drive[3]) {
    drive[0] = (char)('0' + pdrv);
    drive[1] = ':';
    drive[2] = '\0';
}

static void ensure_volume_label(const char drive[3]) {
    char current[MCUJS_FS_VOLUME_LABEL_UTF8_MAX] = {0};
    DWORD serial = 0;
    if (f_getlabel(drive, current, &serial) != FR_OK) {
        return;
    }
    if (current[0] != '\0') {
        return;
    }

    char requested[3 + sizeof(MCUJS_FS_VOLUME_LABEL)];
    int written = snprintf(requested, sizeof(requested), "%s%s",
                           drive, MCUJS_FS_VOLUME_LABEL);
    if (written >= 0 && (size_t)written < sizeof(requested)) {
        /* A full FAT12/16 fixed root can reject a volume-label entry. Label
         * migration is cosmetic and must never disable a healthy filesystem. */
        (void)f_setlabel(requested);
    }
}

static fs_result_t attach_fatfs(void) {
    if (s_wl_handle == WL_INVALID_HANDLE) {
        return FS_ERROR_IO;
    }

    BYTE pdrv = 0xff;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK ||
        ff_diskio_register_wl_partition(pdrv, s_wl_handle) != ESP_OK) {
        return FS_ERROR_IO;
    }

    char drive[3];
    drive_name(pdrv, drive);
    esp_vfs_fat_conf_t config = {
        .base_path = MCUJS_FS_BASE_PATH,
        .fat_drive = drive,
        .max_files = s_mount_config.max_files,
    };
    FATFS *fatfs = NULL;
    esp_err_t error = esp_vfs_fat_register_cfg(&config, &fatfs);
    if (error != ESP_OK) {
        ff_diskio_clear_pdrv_wl(s_wl_handle);
        ff_diskio_unregister(pdrv);
        return FS_ERROR_IO;
    }

    FRESULT mount_result = f_mount(fatfs, drive, 1);
    if (mount_result != FR_OK) {
        esp_vfs_fat_unregister_path(MCUJS_FS_BASE_PATH);
        ff_diskio_clear_pdrv_wl(s_wl_handle);
        ff_diskio_unregister(pdrv);
        return FS_ERROR_IO;
    }
    ensure_volume_label(drive);

    s_pdrv = pdrv;
    s_fatfs = fatfs;
    s_initialized = true;
    return FS_OK;
}

static fs_result_t detach_fatfs(void) {
    if (!s_initialized) {
        return FS_OK;
    }
    if (!is_device_task() || atomic_load(&s_open_files) != 0) {
        return FS_ERROR_BUSY;
    }

    char drive[3];
    drive_name(s_pdrv, drive);
    if (f_mount(NULL, drive, 0) != FR_OK) {
        return FS_ERROR_IO;
    }

    esp_err_t vfs_result = esp_vfs_fat_unregister_path(MCUJS_FS_BASE_PATH);
    ff_diskio_clear_pdrv_wl(s_wl_handle);
    ff_diskio_unregister(s_pdrv);
    s_pdrv = 0xff;
    s_fatfs = NULL;
    s_initialized = false;
    return vfs_result == ESP_OK ? FS_OK : FS_ERROR_IO;
}

static fs_result_t release_wl(void) {
    if (s_wl_handle == WL_INVALID_HANDLE) {
        return FS_OK;
    }
    if (wl_unmount(s_wl_handle) != ESP_OK) {
        return FS_ERROR_IO;
    }
    s_wl_handle = WL_INVALID_HANDLE;
    return FS_OK;
}

fs_result_t fs_init(void) {
    storage_state_t state = (storage_state_t)atomic_load(&s_storage_state);
    if (state == STORAGE_HOST_OWNED || state == STORAGE_CLAIMING_HOST ||
        state == STORAGE_RELEASING_HOST) {
        return FS_ERROR_BUSY;
    }
    if (!is_device_task()) {
        return FS_ERROR_BUSY;
    }
    if (s_initialized) {
        atomic_store(&s_storage_state, STORAGE_DEVICE_OWNED);
        return FS_OK;
    }

    if (s_wl_handle != WL_INVALID_HANDLE) {
        if (release_wl() != FS_OK) {
            atomic_store(&s_storage_state, STORAGE_FAULT);
            return FS_ERROR_IO;
        }
    }
    s_partition = find_valid_partition();
    if (s_partition == NULL || wl_mount(s_partition, &s_wl_handle) != ESP_OK) {
        s_wl_handle = WL_INVALID_HANDLE;
        atomic_store(&s_storage_state, STORAGE_FAULT);
        return FS_ERROR_INVALID;
    }

    fs_result_t result = attach_fatfs();
    if (result != FS_OK) {
        atomic_store(&s_storage_state,
                     release_wl() == FS_OK ? STORAGE_UNINITIALIZED : STORAGE_FAULT);
        return result;
    }

    if (s_device_task == NULL) {
        s_device_task = xTaskGetCurrentTaskHandle();
    }
    atomic_store(&s_storage_state, STORAGE_DEVICE_OWNED);
    return FS_OK;
}

fs_result_t fs_format(void) {
    storage_state_t state = (storage_state_t)atomic_load(&s_storage_state);
    if (state == STORAGE_HOST_OWNED || state == STORAGE_CLAIMING_HOST ||
        state == STORAGE_RELEASING_HOST) {
        return FS_ERROR_BUSY;
    }
    if (!is_device_task()) {
        return FS_ERROR_BUSY;
    }

    if (s_initialized) {
        fs_result_t detach_result = detach_fatfs();
        if (detach_result == FS_ERROR_BUSY) {
            return detach_result;
        }
        if (detach_result != FS_OK) {
            atomic_store(&s_storage_state, STORAGE_FAULT);
            return FS_ERROR_IO;
        }
    }
    if (release_wl() != FS_OK) {
        atomic_store(&s_storage_state, STORAGE_FAULT);
        return FS_ERROR_IO;
    }

    s_partition = find_valid_partition();
    if (s_partition == NULL || wl_mount(s_partition, &s_wl_handle) != ESP_OK) {
        s_wl_handle = WL_INVALID_HANDLE;
        atomic_store(&s_storage_state, STORAGE_FAULT);
        return FS_ERROR_INVALID;
    }

    BYTE pdrv = 0xff;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK ||
        ff_diskio_register_wl_partition(pdrv, s_wl_handle) != ESP_OK) {
        (void)release_wl();
        atomic_store(&s_storage_state, STORAGE_FAULT);
        return FS_ERROR_IO;
    }

    char drive[3];
    drive_name(pdrv, drive);
    static BYTE work_buffer[4096];
    const MKFS_PARM options = {
        .fmt = FM_ANY | FM_SFD,
        .n_fat = 2,
        .align = 0,
        .n_root = 0,
        .au_size = s_mount_config.allocation_unit_size,
    };
    FRESULT format_result = f_mkfs(drive, &options, work_buffer, sizeof(work_buffer));
    ff_diskio_clear_pdrv_wl(s_wl_handle);
    ff_diskio_unregister(pdrv);
    if (release_wl() != FS_OK) {
        atomic_store(&s_storage_state, STORAGE_FAULT);
        return FS_ERROR_IO;
    }
    atomic_store(&s_storage_state, STORAGE_UNINITIALIZED);
    if (format_result != FR_OK) {
        return FS_ERROR_IO;
    }
    return fs_init();
}

fs_result_t fs_sync(void) {
    return ensure_initialized();
}

fs_result_t fs_invalidate(void) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    atomic_store(&s_storage_state, STORAGE_RELEASING_HOST);
    fs_result_t result = detach_fatfs();
    if (result == FS_ERROR_BUSY) {
        atomic_store(&s_storage_state, STORAGE_DEVICE_OWNED);
        return result;
    }
    if (result == FS_OK) {
        result = attach_fatfs();
    }
    atomic_store(&s_storage_state,
                 result == FS_OK ? STORAGE_DEVICE_OWNED : STORAGE_FAULT);
    return result;
}

fs_result_t mcujs_filesystem_begin_host_access(void) {
    if ((storage_state_t)atomic_load(&s_storage_state) != STORAGE_DEVICE_OWNED ||
        !s_initialized || s_wl_handle == WL_INVALID_HANDLE || !is_device_task() ||
        atomic_load(&s_open_files) != 0) {
        return FS_ERROR_BUSY;
    }

    atomic_store(&s_storage_state, STORAGE_CLAIMING_HOST);
    fs_result_t result = detach_fatfs();
    atomic_store(&s_storage_state,
                 result == FS_OK ? STORAGE_HOST_OWNED : STORAGE_FAULT);
    return result;
}

fs_result_t mcujs_filesystem_end_host_access(void) {
    if ((storage_state_t)atomic_load(&s_storage_state) != STORAGE_HOST_OWNED ||
        s_wl_handle == WL_INVALID_HANDLE || !is_device_task()) {
        return FS_ERROR_BUSY;
    }

    atomic_store(&s_storage_state, STORAGE_RELEASING_HOST);
    fs_result_t result = attach_fatfs();
    atomic_store(&s_storage_state,
                 result == FS_OK ? STORAGE_DEVICE_OWNED : STORAGE_FAULT);
    return result;
}

fs_result_t fs_begin_host_access(void) {
    return mcujs_filesystem_begin_host_access();
}

fs_result_t fs_end_host_access(void) {
    return mcujs_filesystem_end_host_access();
}

fs_result_t fs_msc_sync(void) {
    return mcujs_filesystem_host_owned() ? FS_OK : FS_ERROR_BUSY;
}

bool mcujs_filesystem_host_owned(void) {
    return (storage_state_t)atomic_load(&s_storage_state) == STORAGE_HOST_OWNED;
}

uint32_t mcujs_filesystem_sector_size(void) {
    return s_wl_handle == WL_INVALID_HANDLE ? 0u : (uint32_t)wl_sector_size(s_wl_handle);
}

void fs_notify_host(void) {
    /* The MSC state machine controls host cache notifications. */
}

uint32_t fs_get_total_sectors(void) {
    uint32_t sector_size = mcujs_filesystem_sector_size();
    return sector_size == 0 ? 0u : (uint32_t)(wl_size(s_wl_handle) / sector_size);
}

uint32_t fs_get_free_space(void) {
    if (ensure_initialized() != FS_OK) {
        return 0;
    }
    uint64_t free_bytes = 0;
    if (esp_vfs_fat_info(MCUJS_FS_BASE_PATH, NULL, &free_bytes) != ESP_OK) {
        return 0;
    }
    return free_bytes > UINT32_MAX ? UINT32_MAX : (uint32_t)free_bytes;
}

bool fs_host_owned(void) {
    return mcujs_filesystem_host_owned();
}

fs_result_t fs_read_sector(uint32_t sector, uint32_t offset,
                           void *buffer, uint32_t size) {
    if (!mcujs_filesystem_host_owned() || buffer == NULL) {
        return FS_ERROR_BUSY;
    }
    uint32_t sector_size = mcujs_filesystem_sector_size();
    uint32_t sector_count = fs_get_total_sectors();
    if (sector_size == 0 || offset >= sector_size || sector >= sector_count) {
        return FS_ERROR_INVALID;
    }
    uint64_t available = (uint64_t)(sector_count - sector) * sector_size - offset;
    if (size > available) {
        return FS_ERROR_INVALID;
    }
    size_t address = (size_t)sector * sector_size + offset;
    return wl_read(s_wl_handle, address, buffer, size) == ESP_OK ? FS_OK : FS_ERROR_IO;
}

fs_result_t fs_write_sector(uint32_t sector, uint32_t offset,
                            const void *buffer, uint32_t size) {
    if (!mcujs_filesystem_host_owned() || buffer == NULL) {
        return FS_ERROR_BUSY;
    }
    uint32_t sector_size = mcujs_filesystem_sector_size();
    uint32_t sector_count = fs_get_total_sectors();
    if (sector_size == 0 || offset >= sector_size || sector >= sector_count) {
        return FS_ERROR_INVALID;
    }
    uint64_t available = (uint64_t)(sector_count - sector) * sector_size - offset;
    if (size > available) {
        return FS_ERROR_INVALID;
    }

    const uint8_t *source = buffer;
    uint8_t *scratch = NULL;
    uint32_t remaining = size;
    while (remaining != 0) {
        uint32_t chunk = sector_size - offset;
        if (chunk > remaining) {
            chunk = remaining;
        }
        size_t address = (size_t)sector * sector_size;
        const void *write_buffer = source;
        if (offset != 0 || chunk != sector_size) {
            if (scratch == NULL) {
                scratch = malloc(sector_size);
                if (scratch == NULL) {
                    return FS_ERROR;
                }
            }
            if (wl_read(s_wl_handle, address, scratch, sector_size) != ESP_OK) {
                free(scratch);
                return FS_ERROR_IO;
            }
            memcpy(scratch + offset, source, chunk);
            write_buffer = scratch;
        }
        if (wl_erase_range(s_wl_handle, address, sector_size) != ESP_OK ||
            wl_write(s_wl_handle, address, write_buffer, sector_size) != ESP_OK) {
            free(scratch);
            return FS_ERROR_IO;
        }
        source += chunk;
        remaining -= chunk;
        sector++;
        offset = 0;
    }
    free(scratch);
    return FS_OK;
}

fs_result_t fs_open(fs_file_t *file, const char *path, fs_mode_t mode) {
    if (file == NULL) {
        return FS_ERROR_INVALID;
    }
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }

    char translated[MCUJS_FS_PATH_MAX];
    fs_result_t path_result = translate_path(path, translated, sizeof(translated));
    if (path_result != FS_OK) {
        return path_result;
    }

    const char *open_mode = "rb";
    if (mode & FS_MODE_APPEND) {
        open_mode = "ab+";
    } else if (mode & FS_MODE_TRUNCATE) {
        open_mode = "wb+";
    } else if ((mode & FS_MODE_WRITE) && (mode & FS_MODE_READ)) {
        open_mode = "rb+";
    } else if (mode & FS_MODE_WRITE) {
        open_mode = "rb+";
    }

    errno = 0;
    FILE *stream = fopen(translated, open_mode);
    if (stream == NULL && (mode & FS_MODE_CREATE) && !(mode & FS_MODE_APPEND) &&
        !(mode & FS_MODE_TRUNCATE) && errno == ENOENT) {
        stream = fopen(translated, "wb+");
    }
    if (stream == NULL) {
        return errno_to_fs(errno);
    }

    esp_fs_file_t *internal = malloc(sizeof(*internal));
    if (internal == NULL) {
        fclose(stream);
        return FS_ERROR;
    }
    internal->stream = stream;
    file->internal = internal;
    file->is_open = true;
    atomic_fetch_add(&s_open_files, 1);
    return FS_OK;
}

fs_result_t fs_close(fs_file_t *file) {
    if (file == NULL || !file->is_open || file->internal == NULL) {
        return FS_ERROR_INVALID;
    }
    esp_fs_file_t *internal = file->internal;
    int result = fflush(internal->stream);
    if (result == 0) {
        result = fsync(fileno(internal->stream));
    }
    int close_result = fclose(internal->stream);
    free(internal);
    file->internal = NULL;
    file->is_open = false;
    atomic_fetch_sub(&s_open_files, 1);
    return result == 0 && close_result == 0 ? FS_OK : FS_ERROR_IO;
}

fs_result_t fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    if (file == NULL || !file->is_open || file->internal == NULL || buffer == NULL) {
        return FS_ERROR_INVALID;
    }
    esp_fs_file_t *internal = file->internal;
    errno = 0;
    size_t count = fread(buffer, 1, size, internal->stream);
    if (bytes_read != NULL) {
        *bytes_read = count;
    }
    return ferror(internal->stream) ? errno_to_fs(errno) : FS_OK;
}

fs_result_t fs_write(fs_file_t *file, const void *buffer, size_t size,
                     size_t *bytes_written) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    if (file == NULL || !file->is_open || file->internal == NULL || buffer == NULL) {
        return FS_ERROR_INVALID;
    }
    esp_fs_file_t *internal = file->internal;
    errno = 0;
    size_t count = fwrite(buffer, 1, size, internal->stream);
    if (bytes_written != NULL) {
        *bytes_written = count;
    }
    if (count != size) {
        return errno == ENOSPC ? FS_ERROR_NO_SPACE : FS_ERROR_IO;
    }
    return FS_OK;
}

fs_result_t fs_seek(fs_file_t *file, uint32_t offset) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    if (file == NULL || !file->is_open || file->internal == NULL) {
        return FS_ERROR_INVALID;
    }
    esp_fs_file_t *internal = file->internal;
    return fseek(internal->stream, (long)offset, SEEK_SET) == 0 ? FS_OK : errno_to_fs(errno);
}

fs_result_t fs_size(fs_file_t *file, size_t *size) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    if (file == NULL || !file->is_open || file->internal == NULL || size == NULL) {
        return FS_ERROR_INVALID;
    }
    esp_fs_file_t *internal = file->internal;
    struct stat stats;
    if (fstat(fileno(internal->stream), &stats) != 0) {
        return errno_to_fs(errno);
    }
    *size = (size_t)stats.st_size;
    return FS_OK;
}

fs_result_t fs_exists(const char *path) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    char translated[MCUJS_FS_PATH_MAX];
    fs_result_t path_result = translate_path(path, translated, sizeof(translated));
    if (path_result != FS_OK) {
        return path_result;
    }
    struct stat stats;
    return stat(translated, &stats) == 0 ? FS_OK : errno_to_fs(errno);
}

fs_result_t fs_remove(const char *path) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    char translated[MCUJS_FS_PATH_MAX];
    fs_result_t path_result = translate_path(path, translated, sizeof(translated));
    if (path_result != FS_OK || strcmp(translated, MCUJS_FS_BASE_PATH) == 0) {
        return FS_ERROR_INVALID;
    }
    struct stat stats;
    if (stat(translated, &stats) != 0) {
        return errno_to_fs(errno);
    }
    int result = S_ISDIR(stats.st_mode) ? rmdir(translated) : unlink(translated);
    return result == 0 ? FS_OK : errno_to_fs(errno);
}

fs_result_t fs_rename(const char *old_path, const char *new_path) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    char old_translated[MCUJS_FS_PATH_MAX];
    char new_translated[MCUJS_FS_PATH_MAX];
    fs_result_t old_result = translate_path(old_path, old_translated, sizeof(old_translated));
    fs_result_t new_result = translate_path(new_path, new_translated, sizeof(new_translated));
    if (old_result != FS_OK || new_result != FS_OK ||
        strcmp(old_translated, MCUJS_FS_BASE_PATH) == 0 ||
        strcmp(new_translated, MCUJS_FS_BASE_PATH) == 0) {
        return FS_ERROR_INVALID;
    }
    return rename(old_translated, new_translated) == 0 ? FS_OK : errno_to_fs(errno);
}

fs_result_t fs_mkdir(const char *path) {
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    char translated[MCUJS_FS_PATH_MAX];
    fs_result_t path_result = translate_path(path, translated, sizeof(translated));
    if (path_result != FS_OK || strcmp(translated, MCUJS_FS_BASE_PATH) == 0) {
        return FS_ERROR_INVALID;
    }
    return mkdir(translated, 0777) == 0 ? FS_OK : errno_to_fs(errno);
}

fs_result_t fs_list_dir(const char *path, fs_dir_callback_t callback, void *user_data) {
    if (callback == NULL) {
        return FS_ERROR_INVALID;
    }
    fs_result_t ready = ensure_initialized();
    if (ready != FS_OK) {
        return ready;
    }
    char translated[MCUJS_FS_PATH_MAX];
    fs_result_t path_result = translate_path(path, translated, sizeof(translated));
    if (path_result != FS_OK) {
        return path_result;
    }

    DIR *directory = opendir(translated);
    if (directory == NULL) {
        return errno_to_fs(errno);
    }

    fs_result_t result = FS_OK;
    struct dirent *item;
    errno = 0;
    while ((item = readdir(directory)) != NULL) {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }

        fs_entry_t entry = {0};
        strncpy(entry.name, item->d_name, sizeof(entry.name) - 1);

        char child[MCUJS_FS_PATH_MAX];
        int child_length = snprintf(child, sizeof(child), "%s/%s", translated, item->d_name);
        if (child_length < 0 || (size_t)child_length >= sizeof(child)) {
            result = FS_ERROR_INVALID;
            break;
        }
        struct stat stats;
        if (stat(child, &stats) != 0) {
            result = errno_to_fs(errno);
            break;
        }
        entry.size = (uint32_t)stats.st_size;
        entry.is_dir = S_ISDIR(stats.st_mode);
        if (!callback(&entry, user_data)) {
            break;
        }
        errno = 0;
    }
    if (item == NULL && errno != 0 && result == FS_OK) {
        result = errno_to_fs(errno);
    }

    if (closedir(directory) != 0 && result == FS_OK) {
        result = errno_to_fs(errno);
    }
    return result;
}
