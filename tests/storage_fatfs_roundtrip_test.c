/* Native composition test: TWO independent real FatFs R0.16 instances.
 * Device: fs.c -> FatFs -> memory disk; host: FatFs -> RP2 MSC -> same disk.
 * Only hardware/block transport is simulated. Never opens a physical device.
 */
#include "fs.h"
#include "ff.h"
#include "diskio.h"
#include "usb_msc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTORS 16384u
static BYTE disks[2][SECTORS * 512u];
static FATFS host_fs[2];
static bool absent, fail_sync, fail_write, fixtures = true;
static unsigned host_reads[2], host_writes[2];
static const char sentinel[] = "pre-existing file: preserve these bytes\r\n";
static const char app_source[] = "module.exports = 'copied at USB root, not app/app';\n";
static BYTE asset[8193];

/* Public API of the independently linked host FatFs copy. */
FRESULT host_f_mount(FATFS *, const TCHAR *, BYTE);
FRESULT host_f_open(FIL *, const TCHAR *, BYTE);
FRESULT host_f_close(FIL *);
FRESULT host_f_write(FIL *, const void *, UINT, UINT *);
FRESULT host_f_read(FIL *, void *, UINT, UINT *);
FRESULT host_f_mkdir(const TCHAR *);
FRESULT host_f_getlabel(const TCHAR *, TCHAR *, DWORD *);
FRESULT __real_f_mkfs(const TCHAR *, const MKFS_PARM *, void *, UINT);
FRESULT __real_f_setlabel(const TCHAR *);
FRESULT __wrap_f_mkfs(const TCHAR *p, const MKFS_PARM *o, void *b, UINT n) {
    assert(fixtures && "firmware must not format a host-modified or SD volume");
    return __real_f_mkfs(p, o, b, n);
}
FRESULT __wrap_f_setlabel(const TCHAR *p) {
    assert(fixtures && "handoff must not relabel media");
    return __real_f_setlabel(p);
}

bool tud_msc_test_unit_ready_cb(uint8_t);
bool tud_msc_start_stop_cb(uint8_t, uint8_t, bool, bool);
void tud_msc_capacity_cb(uint8_t, uint32_t *, uint16_t *);
int32_t tud_msc_read10_cb(uint8_t, uint32_t, uint32_t, void *, uint32_t);
int32_t tud_msc_write10_cb(uint8_t, uint32_t, uint32_t, uint8_t *, uint32_t);
int32_t tud_msc_scsi_cb(uint8_t, const uint8_t[16], void *, uint16_t);
void tud_msc_set_sense(uint8_t l, uint8_t k, uint8_t a, uint8_t q) {
    (void)l; (void)k; (void)a; (void)q;
}
void usb_cdc_puts(const char *s) { (void)s; }

DSTATUS disk_status(BYTE d) { return d > 1 || (d == 1 && absent) ? STA_NOINIT : 0; }
DSTATUS disk_initialize(BYTE d) { return disk_status(d); }
DRESULT disk_read(BYTE d, BYTE *b, LBA_t s, UINT n) {
    if (disk_status(d)) return RES_NOTRDY;
    if (!b || !n || s >= SECTORS || n > SECTORS - s) return RES_PARERR;
    memcpy(b, disks[d] + s * 512u, n * 512u);
    return RES_OK;
}
DRESULT disk_write(BYTE d, const BYTE *b, LBA_t s, UINT n) {
    if (disk_status(d)) return RES_NOTRDY;
    if (!b || !n || s >= SECTORS || n > SECTORS - s) return RES_PARERR;
    if (d == 1 && fail_write) return RES_ERROR;
    memcpy(disks[d] + s * 512u, b, n * 512u);
    return RES_OK;
}
DRESULT disk_ioctl(BYTE d, BYTE c, void *b) {
    if (disk_status(d)) return RES_NOTRDY;
    switch (c) {
        case CTRL_SYNC: return d == 1 && fail_sync ? RES_ERROR : RES_OK;
        case GET_SECTOR_COUNT: *(LBA_t *)b = SECTORS; return RES_OK;
        case GET_SECTOR_SIZE: *(WORD *)b = 512; return RES_OK;
        case GET_BLOCK_SIZE: *(DWORD *)b = 1; return RES_OK;
        default: return RES_PARERR;
    }
}
uint32_t diskio_get_sector_count(void) { return SECTORS; }
void diskio_sync(void) {}
int diskio_read_sector(uint32_t s, uint32_t o, void *b, uint32_t n) {
    if (!b || s >= SECTORS || o >= 512 || n > 512-o) return -1;
    memcpy(b, disks[0] + s * 512u + o, n); return 0;
}
int diskio_write_sector(uint32_t s, uint32_t o, const void *b, uint32_t n) {
    if (!b || s >= SECTORS || o >= 512 || n > 512-o) return -1;
    memcpy(disks[0] + s * 512u + o, b, n); return 0;
}

/* Host's ONLY disk path goes through the production MSC callbacks. */
DSTATUS host_disk_status(BYTE d) { return tud_msc_test_unit_ready_cb(d) ? 0 : STA_NOINIT; }
DSTATUS host_disk_initialize(BYTE d) { return host_disk_status(d); }
DRESULT host_disk_read(BYTE d, BYTE *b, LBA_t s, UINT n) {
    for (UINT i = 0; i < n; i++) {
        if (tud_msc_read10_cb(d, s+i, 0, b+i*512u, 512) != 512) return RES_ERROR;
        host_reads[d]++;
    }
    return RES_OK;
}
DRESULT host_disk_write(BYTE d, const BYTE *b, LBA_t s, UINT n) {
    for (UINT i = 0; i < n; i++) {
        if (tud_msc_write10_cb(d, s+i, 0, (BYTE *)b+i*512u, 512) != 512) return RES_ERROR;
        host_writes[d]++;
    }
    return RES_OK;
}
DRESULT host_disk_ioctl(BYTE d, BYTE c, void *b) {
    if (c == CTRL_SYNC) {
        const uint8_t cmd[16] = {0x35};
        return tud_msc_scsi_cb(d, cmd, NULL, 0) == 0 ? RES_OK : RES_ERROR;
    }
    uint32_t count; uint16_t size;
    tud_msc_capacity_cb(d, &count, &size);
    if (!count) return RES_NOTRDY;
    if (c == GET_SECTOR_COUNT) *(LBA_t *)b = count;
    else if (c == GET_SECTOR_SIZE) *(WORD *)b = size;
    else if (c == GET_BLOCK_SIZE) *(DWORD *)b = 1;
    else return RES_PARERR;
    return RES_OK;
}

static void setup(void) {
    BYTE work[512];
    MKFS_PARM options = {.fmt = FM_FAT | FM_SFD, .n_fat = 2, .n_root = 512};
    for (unsigned d = 0; d < 2; d++) {
        FATFS fs; FIL f; UINT n;
        char drive[] = "0:", path[] = "0:/KEEP.TXT";
        drive[0] += d; path[0] += d;
        assert(f_mkfs(drive, &options, work, sizeof(work)) == FR_OK);
        assert(f_mount(&fs, drive, 1) == FR_OK);
        assert(f_setlabel(d ? "1:KEEP_SD" : "0:MCUJS") == FR_OK);
        assert(f_open(&f, path, FA_WRITE | FA_CREATE_NEW) == FR_OK);
        assert(f_write(&f, sentinel, sizeof(sentinel), &n) == FR_OK && n == sizeof(sentinel));
        assert(f_close(&f) == FR_OK);
        assert(f_mount(NULL, drive, 0) == FR_OK);
    }
    fixtures = false;
    for (unsigned i = 0; i < sizeof(asset); i++) asset[i] = (BYTE)(i * 31u + i / 257u);
    assert(fs_init() == FS_OK);
    usb_msc_init();
}
static void host_put(const char *path, const void *data, UINT length) {
    FIL f; UINT n;
    assert(host_f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK);
    assert(host_f_write(&f, data, length, &n) == FR_OK && n == length);
    assert(host_f_close(&f) == FR_OK);
}
static void host_check(const char *path, const void *data, UINT length) {
    BYTE *b = malloc(length + 1); FIL f; UINT n;
    assert(b && host_f_open(&f, path, FA_READ) == FR_OK);
    assert(host_f_read(&f, b, length + 1, &n) == FR_OK && n == length);
    assert(!memcmp(b, data, length));
    assert(host_f_close(&f) == FR_OK); free(b);
}
static void device_check(const char *path, const void *data, size_t length) {
    BYTE *b = malloc(length + 1); fs_file_t f = {0}; size_t n;
    assert(b && fs_open(&f, path, FS_MODE_READ) == FS_OK);
    assert(fs_read(&f, b, length + 1, &n) == FS_OK && n == length);
    assert(!memcmp(b, data, length));
    assert(fs_close(&f) == FS_OK); free(b);
}
static void mount_host(unsigned d) {
    char drive[] = "0:"; drive[0] += d;
    assert(host_f_mount(&host_fs[d], drive, 1) == FR_OK);
}
static void eject(unsigned d) {
    char drive[] = "0:"; drive[0] += d;
    assert(host_disk_ioctl(d, CTRL_SYNC, NULL) == RES_OK);
    assert(host_f_mount(NULL, drive, 0) == FR_OK);
    assert(tud_msc_start_stop_cb(d, 0, false, true));
    assert(!tud_msc_test_unit_ready_cb(d)); /* BEFORE supervisor. */
    usb_msc_task();
}
static void roundtrip(void) {
    assert(usb_msc_expose());
    mount_host(0); mount_host(1);
    assert(fs_exists("/app/KEEP.TXT") == FS_ERROR_BUSY);
    assert(fs_exists("/sd/KEEP.TXT") == FS_ERROR_BUSY);
    host_put("0:/index.js", app_source, sizeof(app_source)-1);
    assert(host_f_mkdir("1:/music") == FR_OK);
    host_put("1:/music/long asset name.bin", asset, sizeof(asset));
    eject(1);
    device_check("/sd/music/long asset name.bin", asset, sizeof(asset));
    device_check("/sd/KEEP.TXT", sentinel, sizeof(sentinel));
    assert(fs_exists("/app/index.js") == FS_ERROR_BUSY);
    eject(0);
    device_check("/app/index.js", app_source, sizeof(app_source)-1);
    assert(fs_exists("/app/app/index.js") == FS_ERROR_NOT_FOUND);
    device_check("/app/KEEP.TXT", sentinel, sizeof(sentinel));
    for (unsigned round = 0; round < 3; round++) {
        /* Device writes, then host reads from fresh independent FAT caches. */
        fs_file_t f = {0}; size_t n;
        asset[0] = (BYTE)round;
        assert(fs_open(&f, "/sd/reply.bin", FS_MODE_WRITE | FS_MODE_CREATE | FS_MODE_TRUNCATE) == FS_OK);
        assert(fs_write(&f, asset, sizeof(asset), &n) == FS_OK && n == sizeof(asset));
        /* The outstanding SD handle must not block the other volume. */
        assert(tud_msc_start_stop_cb(0, 0, true, true));
        assert(tud_msc_start_stop_cb(1, 0, true, true));
        usb_msc_task();
        assert(tud_msc_test_unit_ready_cb(0));
        assert(!tud_msc_test_unit_ready_cb(1));
        assert(fs_close(&f) == FS_OK);
        usb_msc_task();
        mount_host(0); mount_host(1);
        host_check("1:/reply.bin", asset, sizeof(asset));
        host_check("0:/index.js", app_source, sizeof(app_source)-1);
        host_check("0:/KEEP.TXT", sentinel, sizeof(sentinel));
        host_check("1:/KEEP.TXT", sentinel, sizeof(sentinel));
        char label[34];
        assert(host_f_getlabel("0:", label, NULL) == FR_OK && !strcmp(label, "MCUJS"));
        assert(host_f_getlabel("1:", label, NULL) == FR_OK && !strcmp(label, "KEEP_SD"));
        eject(0); eject(1);
    }
    assert(host_reads[0] && host_reads[1] && host_writes[0] && host_writes[1]);
    printf("real FatFs copy/eject/device-read + device-write/load/host-read: PASS (app R/W %u/%u; SD %u/%u)\n",
           host_reads[0], host_writes[0], host_reads[1], host_writes[1]);
}
static void fault(const char *scenario) {
    if (!strcmp(scenario, "absent")) absent = true;
    if (!strcmp(scenario, "unsupported")) memset(disks[1], 0, 512);
    BYTE *before = malloc(sizeof(disks[1])); assert(before);
    memcpy(before, disks[1], sizeof(disks[1]));
    assert(usb_msc_expose());
    if (absent || !strcmp(scenario, "unsupported")) {
        assert(!tud_msc_test_unit_ready_cb(1));
        assert(fs_exists("/sd/KEEP.TXT") == (absent ? FS_ERROR_NO_MEDIA : FS_ERROR_UNSUPPORTED));
        assert(!memcmp(before, disks[1], sizeof(disks[1])));
        /* No retries on every task tick; insertion needs a fresh load. */
        absent = false;
        if (!strcmp(scenario, "absent")) {
            for (unsigned i = 0; i < 4; i++) usb_msc_task();
            assert(!tud_msc_test_unit_ready_cb(1));
            usb_msc_event(MCUJS_MSC_EVENT_LOAD); usb_msc_task();
            assert(tud_msc_test_unit_ready_cb(1));
        }
    } else {
        assert(tud_msc_test_unit_ready_cb(1));
        if (!strcmp(scenario, "sync-fault")) {
            fail_sync = true;
            assert(!tud_msc_start_stop_cb(1, 0, false, true));
        } else if (!strcmp(scenario, "write-fault")) {
            fail_write = true;
            assert(tud_msc_write10_cb(1, 7, 3, asset, 17) == -1);
        } else {
            assert(!strcmp(scenario, "remount-fault"));
            BYTE bad_boot[512] = {0};
            assert(tud_msc_write10_cb(1, 0, 0, bad_boot, sizeof(bad_boot)) == 512);
            memcpy(before, disks[1], sizeof(disks[1]));
            assert(tud_msc_start_stop_cb(1, 0, false, true));
        }
        usb_msc_task();
        assert(!tud_msc_test_unit_ready_cb(1));
        assert(fs_exists("/sd/KEEP.TXT") != FS_OK);
        assert(!memcmp(before, disks[1], sizeof(disks[1])));
        fail_sync = fail_write = false;
        usb_msc_event(MCUJS_MSC_EVENT_DETACH);
        usb_msc_event(MCUJS_MSC_EVENT_LOAD);
        usb_msc_task();
        assert(!tud_msc_test_unit_ready_cb(1)); /* No automatic fault recovery. */
        assert(!memcmp(before, disks[1], sizeof(disks[1])));
    }
    eject(0); device_check("/app/KEEP.TXT", sentinel, sizeof(sentinel));
    free(before);
    printf("real FatFs %s: PASS (peer available, SD preserved, no format/relabel)\n", scenario);
}
int main(int argc, char **argv) {
    assert(argc == 2); setup();
    if (!strcmp(argv[1], "roundtrip")) roundtrip(); else fault(argv[1]);
    return 0;
}
