/*
 * mcujs - Filesystem Interface
 * 
 * FAT filesystem abstraction over flash storage
 */

#ifndef MCUJS_FS_H
#define MCUJS_FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Filesystem sector size (FAT standard) */
#define FS_SECTOR_SIZE  512

/* Result codes */
typedef enum {
    FS_OK = 0,
    FS_ERROR,
    FS_ERROR_NOT_FOUND,
    FS_ERROR_EXISTS,
    FS_ERROR_NO_SPACE,
    FS_ERROR_INVALID,
    FS_ERROR_IO,
} fs_result_t;

/* File open modes */
typedef enum {
    FS_MODE_READ = 0x01,
    FS_MODE_WRITE = 0x02,
    FS_MODE_CREATE = 0x04,
    FS_MODE_APPEND = 0x08,
    FS_MODE_TRUNCATE = 0x10,
} fs_mode_t;

/* File handle */
typedef struct {
    void *internal;
    bool is_open;
} fs_file_t;

/* Directory entry */
typedef struct {
    char name[256];
    uint32_t size;
    bool is_dir;
} fs_entry_t;

/*
 * Initialize filesystem
 * Mounts or formats the FAT filesystem
 */
fs_result_t fs_init(void);

/*
 * Sync filesystem to flash
 */
fs_result_t fs_sync(void);

/*
 * Invalidate filesystem cache
 * Remounts FatFs to pick up changes made via USB MSC
 * Call this before reading files after USB writes
 */
fs_result_t fs_invalidate(void);

/*
 * Notify host that filesystem has changed
 * Signals USB MSC to tell host to re-read directory
 */
void fs_notify_host(void);

/*
 * Get total number of sectors
 */
uint32_t fs_get_total_sectors(void);

/*
 * Get free space in bytes
 */
uint32_t fs_get_free_space(void);

/*
 * Low-level sector access for USB MSC
 */
fs_result_t fs_read_sector(uint32_t sector, uint32_t offset, 
                           void *buffer, uint32_t size);
fs_result_t fs_write_sector(uint32_t sector, uint32_t offset,
                            const void *buffer, uint32_t size);

/*
 * File operations
 */
fs_result_t fs_open(fs_file_t *file, const char *path, fs_mode_t mode);
fs_result_t fs_close(fs_file_t *file);
fs_result_t fs_read(fs_file_t *file, void *buffer, size_t size, size_t *bytes_read);
fs_result_t fs_write(fs_file_t *file, const void *buffer, size_t size, size_t *bytes_written);
fs_result_t fs_seek(fs_file_t *file, uint32_t offset);
fs_result_t fs_size(fs_file_t *file, size_t *size);

/*
 * File/directory management
 */
fs_result_t fs_exists(const char *path);
fs_result_t fs_remove(const char *path);
fs_result_t fs_rename(const char *old_path, const char *new_path);
fs_result_t fs_mkdir(const char *path);

/*
 * Directory listing callback
 * Called for each entry in a directory
 * Return true to continue iteration, false to stop
 */
typedef bool (*fs_dir_callback_t)(const fs_entry_t *entry, void *user_data);

/*
 * List directory contents
 * Calls callback for each entry in the directory
 */
fs_result_t fs_list_dir(const char *path, fs_dir_callback_t callback, void *user_data);

#endif /* MCUJS_FS_H */
