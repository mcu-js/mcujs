/* MCU.js ESP32-S3 partition-backed filesystem. */

#include "fs.h"
#include "filesystem.h"

#include "esp_err.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MCUJS_FS_BASE_PATH "/mcujs"
#define MCUJS_FS_PARTITION_LABEL "ffat"
#define MCUJS_FS_PARTITION_OFFSET 0x450000u
#define MCUJS_FS_PARTITION_SIZE 0x3b0000u
#define MCUJS_FS_PATH_MAX 192

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
static bool s_initialized;

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
    return s_initialized ? FS_OK : fs_init();
}

fs_result_t fs_init(void) {
    if (s_initialized) {
        return FS_OK;
    }

    s_partition = find_valid_partition();
    if (s_partition == NULL) {
        return FS_ERROR_INVALID;
    }

    esp_err_t error = esp_vfs_fat_spiflash_mount_rw_wl(
        MCUJS_FS_BASE_PATH, MCUJS_FS_PARTITION_LABEL, &s_mount_config, &s_wl_handle);
    if (error != ESP_OK) {
        if (s_wl_handle != WL_INVALID_HANDLE) {
            wl_unmount(s_wl_handle);
        }
        s_wl_handle = WL_INVALID_HANDLE;
        return FS_ERROR_IO;
    }

    s_initialized = true;
    return FS_OK;
}

fs_result_t fs_format(void) {
    esp_err_t error = esp_vfs_fat_spiflash_format_rw_wl(
        MCUJS_FS_BASE_PATH, MCUJS_FS_PARTITION_LABEL);
    if (error != ESP_OK) {
        return FS_ERROR_IO;
    }
    return s_initialized ? FS_OK : fs_init();
}

fs_result_t fs_sync(void) {
    return s_initialized ? FS_OK : FS_ERROR;
}

fs_result_t fs_invalidate(void) {
    if (!s_initialized) {
        return FS_OK;
    }
    if (esp_vfs_fat_spiflash_unmount_rw_wl(MCUJS_FS_BASE_PATH, s_wl_handle) != ESP_OK) {
        return FS_ERROR_IO;
    }
    s_initialized = false;
    s_wl_handle = WL_INVALID_HANDLE;
    return fs_init();
}

void fs_notify_host(void) {
    /* Runtime MSC is introduced in Milestone 4. */
}

uint32_t fs_get_total_sectors(void) {
    if (ensure_initialized() != FS_OK) {
        return 0;
    }
    return (uint32_t)(wl_size(s_wl_handle) / FS_SECTOR_SIZE);
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

fs_result_t fs_read_sector(uint32_t sector, uint32_t offset,
                           void *buffer, uint32_t size) {
    if (buffer == NULL || ensure_initialized() != FS_OK || offset >= FS_SECTOR_SIZE ||
        size > FS_SECTOR_SIZE - offset || sector >= fs_get_total_sectors()) {
        return FS_ERROR_INVALID;
    }
    size_t address = (size_t)sector * FS_SECTOR_SIZE + offset;
    if (address > wl_size(s_wl_handle) || size > wl_size(s_wl_handle) - address) {
        return FS_ERROR_INVALID;
    }
    return wl_read(s_wl_handle, address, buffer, size) == ESP_OK ? FS_OK : FS_ERROR_IO;
}

fs_result_t fs_write_sector(uint32_t sector, uint32_t offset,
                            const void *buffer, uint32_t size) {
    (void)sector;
    (void)offset;
    (void)buffer;
    (void)size;
    /* Raw writes while FatFs is mounted would corrupt its cache. MSC ownership
     * and unmount/remount coordination belong to Milestone 4. */
    return FS_ERROR_INVALID;
}

fs_result_t fs_open(fs_file_t *file, const char *path, fs_mode_t mode) {
    if (file == NULL || ensure_initialized() != FS_OK) {
        return FS_ERROR_INVALID;
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
    return result == 0 && close_result == 0 ? FS_OK : FS_ERROR_IO;
}

fs_result_t fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read) {
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
    if (file == NULL || !file->is_open || file->internal == NULL) {
        return FS_ERROR_INVALID;
    }
    esp_fs_file_t *internal = file->internal;
    return fseek(internal->stream, (long)offset, SEEK_SET) == 0 ? FS_OK : errno_to_fs(errno);
}

fs_result_t fs_size(fs_file_t *file, size_t *size) {
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
    if (ensure_initialized() != FS_OK) {
        return FS_ERROR;
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
    if (ensure_initialized() != FS_OK) {
        return FS_ERROR;
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
    if (ensure_initialized() != FS_OK) {
        return FS_ERROR;
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
    if (ensure_initialized() != FS_OK) {
        return FS_ERROR;
    }
    char translated[MCUJS_FS_PATH_MAX];
    fs_result_t path_result = translate_path(path, translated, sizeof(translated));
    if (path_result != FS_OK || strcmp(translated, MCUJS_FS_BASE_PATH) == 0) {
        return FS_ERROR_INVALID;
    }
    return mkdir(translated, 0777) == 0 ? FS_OK : errno_to_fs(errno);
}

fs_result_t fs_list_dir(const char *path, fs_dir_callback_t callback, void *user_data) {
    if (callback == NULL || ensure_initialized() != FS_OK) {
        return FS_ERROR_INVALID;
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
