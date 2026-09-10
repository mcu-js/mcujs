#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef TEST_ESP_PATHS
#include <dirent.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
static char observed[256], second[256];
static unsigned calls;
static fs_result_t access_result = FS_OK;
static fs_result_t ensure_initialized(void) { return access_result; }
static int test_stat(const char *path, struct stat *stats) {
    calls++; strcpy(observed, path); memset(stats, 0, sizeof(*stats)); return 0;
}
static int test_unlink(const char *path) { calls++; strcpy(observed, path); return 0; }
static int test_mkdir(const char *path, mode_t mode) { (void)mode; return test_unlink(path); }
static int test_rename(const char *old, const char *new) {
    strcpy(second, new); return test_unlink(old);
}
static FILE *test_fopen(const char *path, const char *mode) {
    (void)mode; test_unlink(path); errno = ENOENT; return NULL;
}
static DIR *test_opendir(const char *path) { test_unlink(path); errno = ENOENT; return NULL; }
#define stat(...) test_stat(__VA_ARGS__)
#define unlink test_unlink
#define rmdir test_unlink
#define mkdir test_mkdir
#define rename test_rename
#define fopen test_fopen
#define opendir test_opendir
#define MCUJS_FS_BASE_PATH "/mcujs"
#define MCUJS_FS_PATH_MAX 192
static atomic_uint s_open_files;
typedef struct { FILE *stream; } esp_fs_file_t;
#include "esp-path-functions.inc"
static bool app_entry(const fs_entry_t *entry, void *data) {
    assert(!strcmp(entry->name, "app") && entry->is_dir && entry->size == 0);
    (*(unsigned *)data)++; return false;
}
static void test_esp(void) {
    assert(fs_open(&(fs_file_t){0}, "/app/index.js", FS_MODE_READ) == FS_ERROR_NOT_FOUND);
    assert(!strcmp(observed, "/mcujs/index.js"));
    assert(fs_exists("a/../settings.json") == FS_OK);
    assert(!strcmp(observed, "/mcujs/settings.json"));
    assert(fs_exists("/app/app/x") == FS_OK);
    assert(!strcmp(observed, "/mcujs/app/x"));
    assert(fs_remove("/app/settings.json") == FS_OK);
    assert(!strcmp(observed, "/mcujs/settings.json"));
    assert(fs_mkdir("lib") == FS_OK && !strcmp(observed, "/mcujs/lib"));
    assert(fs_rename("lib/a", "/app/lib/b") == FS_OK);
    assert(!strcmp(observed, "/mcujs/lib/a") && !strcmp(second, "/mcujs/lib/b"));
    unsigned before = calls, entries = 0;
    assert(fs_exists("/") == FS_OK && fs_exists("/app") == FS_OK);
    assert(fs_list_dir("/", app_entry, &entries) == FS_OK && entries == 1);
    const char *roots[] = {"/", "/app", "/app/a/.."};
    for (unsigned i = 0; i < 3; i++) {
        assert(fs_open(&(fs_file_t){0}, roots[i], FS_MODE_READ) == FS_ERROR_INVALID);
        assert(fs_remove(roots[i]) == FS_ERROR_INVALID);
        assert(fs_mkdir(roots[i]) == FS_ERROR_INVALID);
        assert(fs_rename(roots[i], "x") == FS_ERROR_INVALID);
        assert(fs_rename("x", roots[i]) == FS_ERROR_INVALID);
    }
    assert(fs_remove("/sd/x") == FS_ERROR_NOT_FOUND);
    assert(fs_rename("x", "/sd/x") == FS_ERROR_NOT_FOUND);
    assert(fs_mkdir("/lib") == FS_ERROR_NOT_FOUND);
    assert(fs_exists("/app/../app") == FS_ERROR_INVALID);
    assert(calls == before);
    access_result = FS_ERROR_BUSY;
    assert(fs_exists("/") == FS_ERROR_BUSY);
    assert(fs_list_dir("/", app_entry, &entries) == FS_ERROR_BUSY);
    assert(fs_remove("/app") == FS_ERROR_BUSY);
    assert(fs_mkdir("/") == FS_ERROR_BUSY);
    assert(fs_rename("/app", "x") == FS_ERROR_BUSY);
    assert(fs_open(&(fs_file_t){0}, "/", FS_MODE_READ) == FS_ERROR_BUSY);
    assert(calls == before);
    puts("ESP32 filesystem namespace native source-seam tests passed");
}
#endif

static void normalized(const char *path, const char *base, const char *expected) {
    char out[FS_PATH_MAX];
    assert(fs_normalize_path(path, base, out, sizeof(out)) == FS_OK);
    assert(strcmp(out, expected) == 0);
}
int main(void) {
    normalized("index.js", NULL, "/app/index.js");
    normalized("/", NULL, "/");
    normalized("//app//./lib/../index.js", NULL, "/app/index.js");
    normalized("../b", "/app/lib", "/app/b");
    normalized("a/../b", "/app", "/app/b");
    normalized("/app/app/x", NULL, "/app/app/x");
    char out[FS_PATH_MAX] = "unchanged";
    const char *invalid[] = {"", "../x", "/app/../app", "/app/a/../../app", "0:/x", "a:b", "a\\b", "/.."};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(*invalid); i++) {
        assert(fs_normalize_path(invalid[i], NULL, out, sizeof(out)) == FS_ERROR_INVALID);
        assert(!strcmp(out, "unchanged"));
    }
    const char *missing[] = {"/index.js", "/lib/x", "/sd/x", "/application/x"};
    for (unsigned i = 0; i < sizeof(missing)/sizeof(*missing); i++)
        assert(fs_normalize_path(missing[i], NULL, out, sizeof(out)) == FS_ERROR_NOT_FOUND);
    assert(fs_normalize_path(NULL, NULL, out, sizeof(out)) == FS_ERROR_INVALID);
    assert(fs_normalize_path("x", NULL, out, 4) == FS_ERROR_INVALID);
    assert(fs_normalize_path("x", "relative", out, sizeof(out)) == FS_ERROR_INVALID);
    char long_path[FS_PATH_MAX + 1];
    memset(long_path, 'a', sizeof(long_path)); long_path[FS_PATH_MAX] = 0;
    assert(fs_normalize_path(long_path, NULL, out, sizeof(out)) == FS_ERROR_INVALID);
    char boundary[FS_PATH_MAX];
    memset(boundary, 'a', sizeof(boundary)); memcpy(boundary, "/app/", 5);
    boundary[FS_PATH_MAX - 1] = 0;
    normalized(boundary, NULL, boundary);
    strcpy(out, "/app/lib");
    assert(fs_normalize_path("../x", out, out, sizeof(out)) == FS_OK);
    assert(!strcmp(out, "/app/x"));
    puts("shared filesystem path normalization tests passed");
#ifdef TEST_ESP_PATHS
    test_esp();
#endif
    return 0;
}
