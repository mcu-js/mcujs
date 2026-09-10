#include "fs.h"
#include "ff.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char observed[256], second[256];
static unsigned path_calls;
static void observe(const char *path) { path_calls++; strcpy(observed, path); }
static unsigned s_mount_calls;
static unsigned s_unmount_calls;
static unsigned s_format_calls;
static unsigned s_disk_sync_calls;
static unsigned s_media_changed_calls;
static bool s_fail_mount;
static bool s_fail_unmount;
static FATFS s_test_fs = {.csize = 2};

FRESULT f_mount(FATFS *fs, const char *path, BYTE option) {
    (void)path;
    (void)option;
    if (fs == NULL) {
        s_unmount_calls++;
        return s_fail_unmount ? FR_DISK_ERR : FR_OK;
    }
    s_mount_calls++;
    return s_fail_mount ? FR_DISK_ERR : FR_OK;
}
FRESULT f_opendir(DIR *dir, const char *path) {
    (void)dir;
    observe(path);
    return FR_OK;
}
FRESULT f_closedir(DIR *dir) {
    (void)dir;
    return FR_OK;
}
FRESULT f_mkfs(const char *path, const MKFS_PARM *options, void *work, UINT length) {
    (void)path;
    (void)options;
    (void)work;
    (void)length;
    s_format_calls++;
    return FR_OK;
}
FRESULT f_setlabel(const char *label) {
    (void)label;
    return FR_OK;
}
FRESULT f_close(FIL *file) {
    (void)file;
    return FR_OK;
}
FRESULT f_sync(FIL *file) {
    (void)file;
    return FR_OK;
}
FRESULT f_getfree(const char *path, DWORD *clusters, FATFS **fs) {
    (void)path;
    *clusters = 8;
    *fs = &s_test_fs;
    return FR_OK;
}
FRESULT f_open(FIL *file, const char *path, BYTE mode) {
    assert(strcmp(path, "/open.js") == 0);
    (void)mode;
    file->size = 4;
    file->position = 0;
    return FR_OK;
}
FRESULT f_lseek(FIL *file, FSIZE_t offset) {
    file->position = offset;
    return FR_OK;
}
FSIZE_t f_size(FIL *file) {
    return file->size;
}
FRESULT f_read(FIL *file, void *buffer, UINT size, UINT *read) {
    (void)file;
    memset(buffer, 0x5a, size);
    *read = size;
    return FR_OK;
}
FRESULT f_write(FIL *file, const void *buffer, UINT size, UINT *written) {
    (void)file;
    (void)buffer;
    *written = size;
    return FR_OK;
}
FRESULT f_stat(const char *path, FILINFO *info) {
    observe(path);
    if (info != NULL) memset(info, 0, sizeof(*info));
    return FR_OK;
}
FRESULT f_unlink(const char *path) {
    observe(path);
    return FR_OK;
}
FRESULT f_rename(const char *old_path, const char *new_path) {
    observe(old_path);
    strcpy(second, new_path);
    return FR_OK;
}
FRESULT f_mkdir(const char *path) {
    observe(path);
    return FR_OK;
}
FRESULT f_readdir(DIR *dir, FILINFO *info) {
    (void)dir;
    info->fname[0] = '\0';
    return FR_OK;
}

uint32_t diskio_get_sector_count(void) {
    return 128;
}
int diskio_read_sector(uint32_t sector, uint32_t offset, void *buffer, uint32_t size) {
    (void)sector;
    (void)offset;
    memset(buffer, 0xa5, size);
    return 0;
}
int diskio_write_sector(uint32_t sector, uint32_t offset,
                        const void *buffer, uint32_t size) {
    (void)sector;
    (void)offset;
    (void)buffer;
    (void)size;
    return 0;
}
void diskio_sync(void) {
    s_disk_sync_calls++;
}
void usb_msc_media_changed(void) {
    s_media_changed_calls++;
}
void usb_cdc_puts(const char *text) {
    (void)text;
}

static bool ignore_entry(const fs_entry_t *entry, void *context) {
    (void)entry;
    (void)context;
    return true;
}

static void expect_busy_application_surface(void) {
    fs_file_t stale = {.internal = (void *)1, .is_open = true};
    char byte = 0;
    size_t count = 0;
    assert(fs_access_status() == FS_ERROR_BUSY);
    assert(!fs_storage_ready());
    assert(fs_host_owned());
    assert(fs_init() == FS_ERROR_BUSY);
    assert(fs_format() == FS_ERROR_BUSY);
    assert(fs_sync() == FS_ERROR_BUSY);
    assert(fs_invalidate() == FS_ERROR_BUSY);
    assert(fs_open(&stale, "/busy.js", FS_MODE_READ) == FS_ERROR_BUSY);
    stale = (fs_file_t){.internal = (void *)1, .is_open = true};
    assert(fs_read(&stale, &byte, 1, &count) == FS_ERROR_BUSY);
    assert(fs_write(&stale, &byte, 1, &count) == FS_ERROR_BUSY);
    assert(fs_seek(&stale, 0) == FS_ERROR_BUSY);
    assert(fs_size(&stale, &count) == FS_ERROR_BUSY);
    assert(fs_exists("/busy.js") == FS_ERROR_BUSY);
    assert(fs_remove("/busy.js") == FS_ERROR_BUSY);
    assert(fs_rename("/busy.js", "/still-busy.js") == FS_ERROR_BUSY);
    assert(fs_mkdir("/busy") == FS_ERROR_BUSY);
    assert(fs_list_dir("/", ignore_entry, NULL) == FS_ERROR_BUSY);
}

static bool app_entry(const fs_entry_t *entry, void *context) {
    assert(!strcmp(entry->name, "app") && entry->is_dir && entry->size == 0);
    (*(unsigned *)context)++;
    return false;
}

static void test_namespace(void) {
    assert(fs_init() == FS_OK);
    assert(!strcmp(observed, "/")); /* Internal volume validation is physical. */
    fs_file_t file = {0};
    assert(fs_open(&file, "a/../open.js", FS_MODE_READ) == FS_OK);
    assert(fs_close(&file) == FS_OK);
    assert(fs_exists("/app/app/x") == FS_OK && !strcmp(observed, "/app/x"));
    assert(fs_remove("settings.json") == FS_OK && !strcmp(observed, "/settings.json"));
    assert(fs_mkdir("/app/lib") == FS_OK && !strcmp(observed, "/lib"));
    assert(fs_rename("lib/a", "/app/lib/b") == FS_OK);
    assert(!strcmp(observed, "/lib/a") && !strcmp(second, "/lib/b"));
    assert(fs_list_dir("/app", ignore_entry, NULL) == FS_OK && !strcmp(observed, "/"));
    unsigned before = path_calls, entries = 0;
    assert(fs_list_dir("/", app_entry, &entries) == FS_OK && entries == 1);
    assert(fs_exists("/") == FS_OK && fs_exists("/app") == FS_OK);
    const char *roots[] = {"/", "/app", "/app/a/.."};
    for (unsigned i = 0; i < 3; i++) {
        assert(fs_open(&file, roots[i], FS_MODE_READ) == FS_ERROR_INVALID);
        assert(fs_remove(roots[i]) == FS_ERROR_INVALID);
        assert(fs_mkdir(roots[i]) == FS_ERROR_INVALID);
        assert(fs_rename(roots[i], "x") == FS_ERROR_INVALID);
        assert(fs_rename("x", roots[i]) == FS_ERROR_INVALID);
    }
    assert(fs_exists("/index.js") == FS_ERROR_NOT_FOUND);
    assert(fs_remove("/sd/x") == FS_ERROR_NOT_FOUND);
    assert(fs_rename("x", "/lib/x") == FS_ERROR_NOT_FOUND);
    assert(fs_list_dir("", ignore_entry, NULL) == FS_ERROR_INVALID);
    assert(fs_exists("0:/x") == FS_ERROR_INVALID);
    assert(fs_remove("/app/../app") == FS_ERROR_INVALID);
    assert(fs_mkdir("../x") == FS_ERROR_INVALID);
    assert(path_calls == before && s_format_calls == 0);
    assert(fs_begin_host_access() == FS_OK);
    assert(fs_exists("/") == FS_ERROR_BUSY);
    assert(fs_exists("/app") == FS_ERROR_BUSY);
    assert(fs_remove("/app") == FS_ERROR_BUSY);
    assert(fs_mkdir("/") == FS_ERROR_BUSY);
    assert(fs_list_dir("/", app_entry, &entries) == FS_ERROR_BUSY);
    assert(entries == 1);
    puts("RP2 filesystem namespace native FatFs tests passed");
}

static void test_handoff_and_ebusy(void) {
    assert(fs_init() == FS_OK);
    assert(fs_storage_ready());
    assert(!fs_host_owned());

    fs_file_t file = {0};
    assert(fs_open(&file, "/app/open.js", FS_MODE_READ) == FS_OK);
    assert(fs_begin_host_access() == FS_ERROR_BUSY);
    assert(s_disk_sync_calls == 0);
    assert(fs_close(&file) == FS_OK);

    assert(fs_begin_host_access() == FS_OK);
    assert(s_disk_sync_calls == 2);
    assert(s_unmount_calls == 1);
    expect_busy_application_surface();

    char sector[16] = {0};
    assert(fs_read_sector(0, 0, sector, sizeof(sector)) == FS_OK);
    assert(fs_write_sector(0, 0, sector, sizeof(sector)) == FS_OK);
    assert(fs_msc_sync() == FS_OK);
    assert(s_disk_sync_calls == 3);

    assert(fs_end_host_access() == FS_OK);
    assert(fs_storage_ready());
    assert(!fs_host_owned());
    assert(s_mount_calls == 2);
    assert(s_format_calls == 0);
}

static void test_unmount_failure_faults_without_format(void) {
    assert(fs_init() == FS_OK);
    s_fail_unmount = true;
    assert(fs_begin_host_access() == FS_ERROR_IO);
    assert(fs_access_status() == FS_ERROR_IO);
    assert(!fs_storage_ready());
    assert(!fs_host_owned());
    assert(fs_open(&(fs_file_t){0}, "/no-remount.js", FS_MODE_READ) == FS_ERROR_IO);
    assert(s_format_calls == 0);
}

static void test_remount_failure_faults_without_format(void) {
    assert(fs_init() == FS_OK);
    assert(fs_begin_host_access() == FS_OK);
    s_fail_mount = true;
    assert(fs_end_host_access() == FS_ERROR_IO);
    assert(fs_access_status() == FS_ERROR_IO);
    assert(!fs_storage_ready());
    assert(!fs_host_owned());
    assert(fs_open(&(fs_file_t){0}, "/no-autoformat.js", FS_MODE_READ) == FS_ERROR_IO);
    assert(s_format_calls == 0);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    if (strcmp(argv[1], "namespace") == 0) {
        test_namespace();
    } else if (strcmp(argv[1], "handoff") == 0) {
        test_handoff_and_ebusy();
    } else if (strcmp(argv[1], "unmount-failure") == 0) {
        test_unmount_failure_faults_without_format();
    } else if (strcmp(argv[1], "remount-failure") == 0) {
        test_remount_failure_faults_without_format();
    } else {
        fprintf(stderr, "unknown scenario: %s\n", argv[1]);
        return 2;
    }
    puts("filesystem ownership test passed");
    return 0;
}
