#include "fs.h"
#include "ff.h"
#include "diskio.h"

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
static FRESULT sd_mount_result = FR_OK;
static FRESULT io_result = FR_OK;
static FRESULT write_result = FR_OK, close_result = FR_OK;
static bool short_write;
static unsigned sd_mounts, sd_unmounts, sd_syncs, sd_reads, sd_writes, closes, sd_initializes;
static bool sd_unmount_fail;
static DRESULT sd_sync_result = RES_OK, sd_read_result = RES_OK, sd_write_result = RES_OK;
static DSTATUS sd_status;
static BYTE sd_sectors[128][512], sd_mbr[512];
static uint32_t sd_card_count = 256, sd_start, sd_length = 128;
static LBA_t sd_last_read, sd_last_write;
static BYTE *sd_sector(LBA_t sector) {
    assert(sector < sd_card_count);
    if (sector == sd_start + sd_length - 1) return sd_sectors[127];
    if (!sd_start) { assert(sector < 128); return sd_sectors[sector]; }
    if (!sector) return sd_mbr;
    if (sector == sd_start) return sd_sectors[0];
    if (sector == sd_start + 1) return sd_sectors[1];
    assert(!"unexpected physical read/write (including raw card-end probes)");
    return NULL;
}
#ifdef MCUJS_TEST_DUAL_MSC
static void put16(BYTE *b, uint16_t n) { b[0] = n; b[1] = n >> 8; }
static void put32(BYTE *b, uint32_t n) { put16(b, n); put16(b+2, n >> 16); }
static void sd_fixture(bool partitioned) {
    memset(sd_sectors, 0, sizeof(sd_sectors));
    memset(sd_mbr, 0, sizeof(sd_mbr));
    sd_start = partitioned ? 8192 : 0;
    sd_length = partitioned ? 62527486 : 128;
    sd_card_count = partitioned ? 62535680 : 256;
    BYTE *b = sd_sectors[0];
    b[0] = 0xeb; b[2] = 0x90;
    put16(b+11, 512); b[13] = partitioned ? 64 : 1;
    put16(b+14, partitioned ? 32 : 1); b[16] = 2; b[21] = 0xf8;
    put16(b+510, 0xaa55);
    if (partitioned) {
        put32(b+32, sd_length); put32(b+36, 7631); put32(b+44, 2);
        put16(b+48, 1); put16(b+50, 6); put32(b+28, sd_start);
        memcpy(b+82, "FAT32   ", 8);
        sd_mbr[446+4] = 0x0c;
        put32(sd_mbr+446+8, sd_start); put32(sd_mbr+446+12, sd_length);
        put16(sd_mbr+510, 0xaa55);
    } else {
        put16(b+17, 16); put16(b+19, sd_length); put16(b+22, 1);
        memcpy(b+54, "FAT12   ", 8);
    }
}
#endif
DSTATUS disk_status(BYTE drive) { assert(drive == 1); return sd_status; }
DSTATUS disk_initialize(BYTE drive) { sd_initializes++; return disk_status(drive); }
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer) {
    assert(drive == 1);
    if (cmd == CTRL_SYNC) { sd_syncs++; return sd_sync_result; }
    assert(cmd == GET_SECTOR_COUNT); *(LBA_t *)buffer = sd_card_count; return RES_OK;
}
DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count) {
    assert(drive == 1 && count == 1);
    sd_reads++; sd_last_read = sector;
    if (sd_read_result == RES_OK) memcpy(buffer, sd_sector(sector), 512);
    return sd_read_result;
}
DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count) {
    assert(drive == 1 && count == 1);
    sd_writes++; sd_last_write = sector;
    if (sd_write_result == RES_OK) memcpy(sd_sector(sector), buffer, 512);
    return sd_write_result;
}
static bool last_open_sd;
static bool fail_dir_read;


FRESULT f_mount(FATFS *fs, const char *path, BYTE option) {
    (void)option;
    if (!strcmp(path, "1:")) {
        if (fs) fs->volbase = sd_start;
        if (fs) sd_mounts++;
        if (!fs) sd_unmounts++;
        return fs ? sd_mount_result : sd_unmount_fail ? FR_DISK_ERR : FR_OK;
    }
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
    closes++;
    return close_result;
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
    last_open_sd = !strcmp(path, "1:/asset.json");
    assert(last_open_sd || !strcmp(path, "/open.js"));
    observe(path);
    if (io_result != FR_OK) return io_result;
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
    *read = io_result == FR_OK ? size : 0;
    return io_result;
}
FRESULT f_write(FIL *file, const void *buffer, UINT size, UINT *written) {
    (void)file;
    (void)buffer;
    *written = write_result != FR_OK ? 0 : short_write && size ? size - 1 : size;
    return write_result;
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
    return fail_dir_read ? FR_DISK_ERR : FR_OK;
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
#ifndef MCUJS_TEST_DUAL_MSC
void usb_msc_media_changed(void) {
    s_media_changed_calls++;
}
#else
#include "usb_msc.h"
#include "tusb.h"
bool tud_msc_test_unit_ready_cb(uint8_t lun);
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power, bool start, bool eject);
extern bool tud_msc_prevent_allow_medium_removal_cb(uint8_t, uint8_t, uint8_t) __attribute__((weak));
uint8_t tud_msc_get_maxlun_cb(void);
void tud_msc_capacity_cb(uint8_t, uint32_t *, uint16_t *);
bool tud_msc_is_writable_cb(uint8_t);
int32_t tud_msc_read10_cb(uint8_t, uint32_t, uint32_t, void *, uint32_t);
int32_t tud_msc_write10_cb(uint8_t, uint32_t, uint32_t, uint8_t *, uint32_t);
int32_t tud_msc_scsi_cb(uint8_t, const uint8_t[16], void *, uint16_t);
static uint8_t sense_key, sense_asc;
void tud_msc_set_sense(uint8_t lun, uint8_t key, uint8_t asc, uint8_t ascq) {
    (void)lun; (void)ascq; sense_key = key; sense_asc = asc;
}
static void test_dual_msc(void) {
    sd_fixture(false);
    (void)s_media_changed_calls;
    assert(fs_init() == FS_OK);
    usb_msc_init();
    assert(usb_msc_expose());
    assert(tud_msc_test_unit_ready_cb(0));
    assert(tud_msc_test_unit_ready_cb(1));
    /* Exercise real MSC callbacks AND real filesystem ownership. */
    assert(fs_exists("/app/open.js") == FS_ERROR_BUSY);
    assert(fs_exists("/sd/asset.json") == FS_ERROR_BUSY);
    assert(tud_msc_get_maxlun_cb() == 2); /* COUNT, not highest LUN. */
    /* Pinned TinyUSB dispatches PREVENT to this weak hook, NOT scsi_cb. */
    if (tud_msc_prevent_allow_medium_removal_cb)
        assert(tud_msc_prevent_allow_medium_removal_cb(1, 1, 0));
    assert(!tud_msc_start_stop_cb(1, 0, false, true));
    assert(sense_key == SCSI_SENSE_ILLEGAL_REQUEST && sense_asc == 0x53);
    assert(tud_msc_prevent_allow_medium_removal_cb(1, 0, 0));
    assert(tud_msc_start_stop_cb(1, 0, false, true));
    usb_msc_task();
    assert(fs_exists("/sd/asset.json") == FS_OK);
    assert(fs_exists("/app/open.js") == FS_ERROR_BUSY);
    assert(s_format_calls == 0);
    /* LOAD then EJECT before the supervisor runs must cancel the load. */
    fs_file_t file = {0};
    assert(fs_open(&file, "/sd/asset.json", FS_MODE_READ) == FS_OK);
    assert(tud_msc_start_stop_cb(1, 0, true, true));
    usb_msc_task(); /* Busy open handle: retry, do not drop accepted load. */
    assert(!tud_msc_test_unit_ready_cb(1));
    assert(tud_msc_start_stop_cb(1, 0, false, true));
    assert(fs_close(&file) == FS_OK);
    usb_msc_task();
    assert(!fs_volume_host_owned(1));
    assert(fs_exists("/sd/asset.json") == FS_OK);
    assert(tud_msc_start_stop_cb(1, 0, true, true));
    usb_msc_task();
    assert(tud_msc_test_unit_ready_cb(1));
    /* EJECT readiness drops immediately; LOAD cannot overtake remount. */
    assert(tud_msc_start_stop_cb(1, 0, false, true));
    assert(!tud_msc_start_stop_cb(1, 0, true, true));
    assert(!tud_msc_test_unit_ready_cb(1));
    usb_msc_task();
    assert(fs_exists("/sd/asset.json") == FS_OK);
    assert(tud_msc_start_stop_cb(1, 0, true, true));
    usb_msc_task();
    uint8_t data[512], original[512], command[16] = {0x35};
    memset(sd_sectors, 0x36, sizeof(sd_sectors));
    memcpy(original, sd_sectors[0], 512);
    memset(data, 0xa5, sizeof(data));
    assert(tud_msc_write10_cb(1, 0, 17, data, 23) == 23);
    assert(!memcmp(sd_sectors[0], original, 17));
    assert(!memcmp(sd_sectors[0]+17, data, 23));
    assert(!memcmp(sd_sectors[0]+40, original+40, 472));
    assert(!memcmp(sd_sectors[1], original, 512));
    assert(tud_msc_read10_cb(1, 0, 17, data, 23) == 23);
    unsigned writes = sd_writes;
    assert(tud_msc_write10_cb(1, 0, 511, data, 2) == -1);
    assert(tud_msc_write10_cb(1, 0, UINT32_MAX, data, 2) == -1);
    assert(tud_msc_write10_cb(1, UINT32_MAX, 0, data, 1) == -1);
    assert(tud_msc_write10_cb(1, 128, 0, data, 1) == -1);
    assert(tud_msc_write10_cb(1, 0, 0, NULL, 1) == -1);
    assert(sd_writes == writes);
    for (unsigned lun = 2; lun <= 255; lun += 253) {
        uint32_t count = 99; uint16_t size = 99;
        assert(!tud_msc_test_unit_ready_cb(lun));
        assert(!tud_msc_is_writable_cb(lun));
        tud_msc_capacity_cb(lun, &count, &size);
        assert(count == 0);
        assert(tud_msc_read10_cb(lun, 0, 0, data, 1) == -1);
        assert(tud_msc_write10_cb(lun, 0, 0, data, 1) == -1);
        assert(!tud_msc_start_stop_cb(lun, 0, true, true));
        assert(!tud_msc_prevent_allow_medium_removal_cb(lun, 1, 0));
        assert(tud_msc_scsi_cb(lun, command, NULL, 0) == -1);
        assert(sense_key == SCSI_SENSE_ILLEGAL_REQUEST && sense_asc == 0x25);
    }
    sd_status = STA_PROTECT;
    assert(!tud_msc_is_writable_cb(1));
    assert(tud_msc_write10_cb(1, 0, 0, data, 1) == -1);
    assert(sense_key == 7 && sense_asc == 0x27 && sd_writes == writes);
    assert(tud_msc_read10_cb(1, 0, 0, data, 1) == 1);
    sd_status = 0;
    assert(tud_msc_start_stop_cb(1, 0, false, false)); /* STOP sync, no eject */
    assert(fs_volume_host_owned(1));
    usb_msc_event(MCUJS_MSC_EVENT_DETACH);
    usb_msc_event(MCUJS_MSC_EVENT_RESET);
    usb_msc_event(MCUJS_MSC_EVENT_SUSPEND);
    usb_msc_task();
    assert(tud_msc_test_unit_ready_cb(1));
    /* Failed RMW read cannot overwrite even one byte or retry the lease. */
    memcpy(original, sd_sectors[0], 512);
    sd_read_result = RES_ERROR;
    assert(tud_msc_write10_cb(1, 0, 13, data, 1) == -1);
    assert(sd_writes == writes && !memcmp(original, sd_sectors[0], 512));
    sd_read_result = RES_OK;
    assert(!tud_msc_test_unit_ready_cb(1));
    assert(tud_msc_scsi_cb(1, command, NULL, 0) == -1);
    assert(!tud_msc_start_stop_cb(1, 0, false, true));
    assert(fs_exists("/sd/asset.json") == FS_ERROR_BUSY);
    assert(tud_msc_test_unit_ready_cb(0)); /* Peer remains healthy. */
    assert(s_format_calls == 0);
    puts("dual RP2 MSC / filesystem ownership: PASS");
}
/* One driver read already checks CID and CRC. Readiness/capacity queries
 * must not add CTRL_SYNC transactions to every 512-byte data callback. */
static void test_sd_io_checks(bool sync_fault) {
    sd_fixture(true);
    assert(fs_init() == FS_OK);
    usb_msc_init();
    assert(usb_msc_expose());
    uint8_t data[512], command[16] = {0x35};
    uint32_t count;
    unsigned syncs = sd_syncs, reads = sd_reads;
    assert(tud_msc_read10_cb(1, 1, 0, data, sizeof(data)) == 512);
    assert(sd_reads == reads + 1 && sd_syncs == syncs);
    assert(fs_volume_capacity(1, &count) == FS_OK && count == sd_length);
    assert(tud_msc_is_writable_cb(1));
    assert(sd_syncs == syncs);
    assert(tud_msc_test_unit_ready_cb(1));
    assert(sd_syncs == ++syncs);
    assert(tud_msc_scsi_cb(1, command, NULL, 0) == 0);
    assert(sd_syncs == ++syncs);
    assert(tud_msc_start_stop_cb(1, 0, false, false));
    assert(sd_syncs == ++syncs);
    assert(tud_msc_start_stop_cb(1, 0, true, false));
    assert(sd_syncs == ++syncs);
    if (sync_fault) {
        sd_sync_result = RES_ERROR;
        assert(!tud_msc_test_unit_ready_cb(1));
        assert(sd_syncs == ++syncs);
        sd_sync_result = RES_OK;
    } else {
        sd_read_result = RES_NOTRDY;
        assert(tud_msc_read10_cb(1, 1, 0, data, sizeof(data)) == -1);
        sd_read_result = RES_OK;
    }
    /* Failure remains fenced even if the next transaction would succeed. */
    reads = sd_reads;
    assert(tud_msc_read10_cb(1, 1, 0, data, sizeof(data)) == -1);
    assert(!tud_msc_test_unit_ready_cb(1));
    assert(!tud_msc_start_stop_cb(1, 0, false, true));
    assert(sd_reads == reads && sd_syncs == syncs);
    assert(tud_msc_test_unit_ready_cb(0));
    assert(fs_volume_host_owned(1) && s_format_calls == 0 && sd_writes == 0);
    puts("SD data-path sync budget, explicit probes, fault fence and peer isolation: PASS");
}
static void test_sd_bounds(void) {
    assert(fs_init() == FS_OK);
    for (unsigned layout = 0; layout < 9; layout++) {
        /* Retire the preceding fixture's native mount before replacing media. */
        io_result = FR_DISK_ERR;
        assert(fs_open(&(fs_file_t){0}, "/sd/asset.json", FS_MODE_READ) == FS_ERROR_IO);
        io_result = FR_OK;
        /* FAT32/16/12, each MBR and superfloppy, plus legacy FAT12 VBR. */
        sd_fixture(layout < 2);
        bool mbr = (!(layout % 2) && layout < 6) || layout >= 7;
        if (layout >= 2 && layout < 4) {
            sd_length = 8192;
            put16(sd_sectors[0]+19, 8192); put16(sd_sectors[0]+22, 32);
            memcpy(sd_sectors[0]+54, "FAT16   ", 8);
        }
        sd_start = layout == 8 ? UINT32_MAX - 128 : mbr ? 8192 : 0;
        sd_card_count = layout == 8 ? UINT32_MAX : sd_start + sd_length + 128;
        if (mbr) {
            sd_mbr[450] = layout < 2 ? 0x0c : layout < 4 ? 0x06 : 0x01;
            put32(sd_mbr+454, sd_start);
            put32(sd_mbr+458, sd_length + (layout == 8 ? 0 : 64));
            put16(sd_mbr+510, 0xaa55);
            if (layout == 7) { /* FAT in the fourth primary entry. */
                memcpy(sd_mbr+494, sd_mbr+446, 16);
                memset(sd_mbr+446, 0, 16);
            }
        }
        if (!layout) {
            sd_card_count = 62535680;
            put32(sd_mbr+458, 62527486);
        }
        if (layout == 6) put16(sd_sectors[0]+510, 0); /* DOS-era FAT12. */
        BYTE boot[512], mbr_before[512], data[512];
        memcpy(boot, sd_sectors[0], 512); memcpy(mbr_before, sd_mbr, 512);
        unsigned reads = sd_reads, writes = sd_writes;
        assert(fs_volume_begin_host_access(1) == FS_OK);
        assert(sd_initializes == 0); /* Never reset an initialized card. */
        assert(sd_reads == reads + (mbr ? 2 : 1) && sd_writes == writes);
        assert(!memcmp(boot, sd_sectors[0], 512) && !memcmp(mbr_before, sd_mbr, 512));
        uint32_t count = 0;
        assert(fs_volume_capacity(1, &count) == FS_OK && count == sd_length);
        if (!layout) assert(count == 62527486);
        const uint32_t sectors[] = {0, 1, count-1};
        for (unsigned i = 0; i < 3; i++) {
            uint32_t sector = sectors[i];
            assert(fs_volume_read_sector(1, sector, 0, data, 512) == FS_OK);
            assert(sd_last_read == sd_start + sector);
            assert(fs_volume_write_sector(1, sector, 0, data, 512) == FS_OK);
            assert(sd_last_write == sd_start + sector);
            BYTE original[512]; memcpy(original, sd_sector(sd_start + sector), 512);
            BYTE byte = 0xa7;
            assert(fs_volume_write_sector(1, sector, 511, &byte, 1) == FS_OK);
            assert(sd_last_read == sd_start + sector && sd_last_write == sd_start + sector);
            assert(!memcmp(original, sd_sector(sd_start + sector), 511));
            assert(fs_volume_read_sector(1, sector, 511, &byte, 1) == FS_OK && byte == 0xa7);
            assert(fs_volume_write_sector(1, sector, 0, original, 512) == FS_OK);
        }
        if (!layout) assert(sd_last_write == 62535677);
        if (layout == 8) assert(sd_last_write == UINT32_MAX - 1);
        reads = sd_reads; writes = sd_writes;
        const struct { uint32_t sector, offset, size; } invalid[] = {
            {count, 0, 1}, {count+1, 0, 512}, {UINT32_MAX, 0, 1},
            {0, 512, 0}, {0, UINT32_MAX, 1}, {0, 511, 2}, {0, 1, UINT32_MAX}
        };
        for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
            assert(fs_volume_read_sector(1, invalid[i].sector, invalid[i].offset, data, invalid[i].size) == FS_ERROR_INVALID);
            assert(fs_volume_write_sector(1, invalid[i].sector, invalid[i].offset, data, invalid[i].size) == FS_ERROR_INVALID);
        }
        assert(fs_volume_read_sector(1, 0, 0, NULL, 1) == FS_ERROR_INVALID);
        assert(fs_volume_write_sector(1, 0, 0, NULL, 1) == FS_ERROR_INVALID);
        assert(fs_volume_read_sector(1, count-1, 0, data, 0) == FS_OK);
        assert(fs_volume_write_sector(1, count-1, 0, data, 0) == FS_OK);
        assert(sd_reads == reads && sd_writes == writes);
        /* Host metadata edits cannot expand the live lease. */
        memset(sd_sectors[0]+19, 0xff, 2);
        assert(fs_volume_capacity(1, &count) == FS_OK && count == sd_length);
        memcpy(sd_sectors[0], boot, 512);
        assert(fs_exists("/app/open.js") == FS_OK);
        assert(fs_exists("/sd/asset.json") == FS_ERROR_BUSY);
        assert(fs_volume_end_host_access(1) == FS_OK);
        assert(fs_exists("/sd/asset.json") == FS_OK);
        count = 99;
        assert(fs_volume_capacity(1, &count) == FS_ERROR_BUSY && count == 0);
        assert(!memcmp(mbr_before, sd_mbr, 512));
    }
    assert(s_format_calls == 0);
    puts("SD FAT12/16/32 MBR/SFD bounds, R/W endpoints, overflow, lease refresh: PASS");
}
static void test_sd_malformed(void) {
    assert(fs_init() == FS_OK);
    const struct { bool mbr; unsigned offset, width; uint32_t value; } bad[] = {
        {false, 11, 2, 1024}, {false, 13, 1, 0}, {false, 13, 1, 3},
        {false, 14, 2, 0}, {false, 16, 1, 0}, {false, 16, 1, 3},
        {false, 17, 2, 16}, {false, 19, 2, 128}, {false, 32, 4, 0},
        {false, 32, 4, 62527487}, {false, 32, 4, UINT32_MAX},
        {false, 36, 4, 0}, {false, 36, 4, 1}, {false, 36, 4, UINT32_MAX},
        {false, 42, 2, 1}, {false, 44, 4, 1}, {false, 44, 4, UINT32_MAX},
        {false, 48, 2, 32}, {false, 50, 2, 32}, {false, 510, 2, 0},
        {false, 82, 1, 0}, /* R0.16 must recognize the same FAT32 header. */
        {false, 32, 4, 127}, {false, 32, 4, 128},
        {true, 510, 2, 0}, {true, 454, 4, 0}, {true, 454, 4, UINT32_MAX},
        {true, 458, 4, 0}, {true, 458, 4, UINT32_MAX}, {true, 458, 4, 127},
        {true, 454, 4, 62535679}, /* Reject before VBR read at physical end. */
    };
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
        sd_fixture(true);
        BYTE *b = (bad[i].mbr ? sd_mbr : sd_sectors[0]) + bad[i].offset;
        if (bad[i].width == 4) put32(b, bad[i].value);
        else if (bad[i].width == 2) put16(b, bad[i].value);
        else *b = bad[i].value;
        unsigned reads = sd_reads;
        assert(fs_volume_begin_host_access(1) == FS_ERROR_UNSUPPORTED);
        assert(sd_reads == reads + (bad[i].mbr ? 1 : 2));
        assert(!fs_volume_host_owned(1) && sd_mounts == 0 && sd_writes == 0);
        uint32_t count = 99;
        assert(fs_volume_capacity(1, &count) == FS_ERROR_BUSY && count == 0);
    }
    sd_fixture(false);
    put32(sd_sectors[0]+32, 256); /* Conflicting 16/32-bit BPB totals. */
    assert(fs_volume_begin_host_access(1) == FS_ERROR_UNSUPPORTED);
    sd_fixture(true);
    sd_read_result = RES_ERROR;
    assert(fs_volume_begin_host_access(1) == FS_ERROR_IO);
    sd_read_result = RES_NOTRDY;
    assert(fs_volume_begin_host_access(1) == FS_ERROR_NO_MEDIA);
    sd_read_result = RES_OK; sd_status = STA_NOINIT;
    assert(fs_volume_begin_host_access(1) == FS_ERROR_NO_MEDIA);
    sd_status = 0;
    assert(sd_mounts == 0 && sd_writes == 0);
    assert(fs_exists("/app/open.js") == FS_OK && s_format_calls == 0);
    puts("SD malformed geometry rejected before mount/export: PASS");
}
#endif
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

#if MCUJS_HAS_SD
static bool mounts_entry(const fs_entry_t *entry, void *context) {
    unsigned *index = context;
    assert(!strcmp(entry->name, (*index)++ == 0 ? "app" : "sd"));
    return true;
}
static void test_sd_mount(void) {
    assert(fs_init() == FS_OK);
    unsigned count=0;
    assert(fs_list_dir("/", mounts_entry, &count) == FS_OK && count == 2);
    char normalized[FS_PATH_MAX];
    assert(fs_normalize_path("/sd/x/../asset.json", NULL, normalized, sizeof(normalized)) == FS_OK);
    assert(!strcmp(normalized, "/sd/asset.json"));
    assert(fs_normalize_path("./asset.json", "/sd", normalized, sizeof(normalized)) == FS_OK);
    assert(!strcmp(normalized, "/sd/asset.json"));
    assert(fs_normalize_path("/sd/../app/x", NULL, normalized, sizeof(normalized)) == FS_ERROR_INVALID);
    assert(fs_normalize_path("../asset.json", "/sd", normalized, sizeof(normalized)) == FS_ERROR_INVALID);
    fs_file_t app={0},sd={0}; char b[4]; size_t n=0;
    assert(fs_open(&app,"open.js",FS_MODE_READ)==FS_OK && !last_open_sd);
    assert(fs_close(&app)==FS_OK);
    assert(fs_open(&sd,"/sd/asset.json",FS_MODE_READ)==FS_OK && last_open_sd);
    assert(sd_mounts==1 && s_format_calls==0);
    assert(fs_begin_host_access()==FS_OK); /* SD handle must not block app MSC. */
    assert(fs_read(&sd,b,4,&n)==FS_OK && n==4);
    assert(fs_open(&app,"/app/open.js",FS_MODE_READ)==FS_ERROR_BUSY);
    assert(fs_close(&sd)==FS_OK);
    assert(fs_open(&sd,"//./sd/asset.json",FS_MODE_READ)==FS_OK);
    assert(fs_close(&sd)==FS_OK);
    assert(fs_open(&sd,"/sd/asset.json",FS_MODE_READ)==FS_OK);
    assert(fs_end_host_access()==FS_OK); /* Must preserve outstanding SD handle. */
    assert(fs_read(&sd,b,4,&n)==FS_OK);
    assert(fs_close(&sd)==FS_OK);
    assert(fs_rename("/sd/a","/app/a") != FS_OK);
    assert(fs_remove("/sd")==FS_ERROR_INVALID);
    assert(fs_mkdir("/sd")==FS_ERROR_INVALID);
    assert(fs_open(&sd,"/sd",FS_MODE_READ)==FS_ERROR_INVALID);
    io_result=FR_DISK_ERR;
    assert(fs_open(&sd,"/sd/asset.json",FS_MODE_READ)==FS_ERROR_IO);
    io_result=FR_OK;sd_mount_result=FR_NO_FILESYSTEM;
    assert(fs_open(&sd,"/sd/asset.json",FS_MODE_READ)==FS_ERROR_UNSUPPORTED);
    assert(s_format_calls==0);
    sd_mount_result=FR_NOT_READY;
    assert(fs_open(&sd,"/sd/asset.json",FS_MODE_READ)==FS_ERROR_NO_MEDIA);
    assert(fs_open(&app,"/app/open.js",FS_MODE_READ)==FS_OK);
    assert(fs_close(&app)==FS_OK);
    sd_mount_result=FR_OK;
    assert(fs_open(&sd,"/sd/asset.json",FS_MODE_READ)==FS_OK);
    io_result=FR_DISK_ERR;
    assert(fs_read(&sd,b,4,&n)==FS_ERROR_IO);
    io_result=FR_OK;
    assert(fs_read(&sd,b,4,&n)!=FS_OK); /* Failed live handle cannot silently switch cards. */
    assert(fs_close(&sd)==FS_ERROR_IO);
    assert(fs_open(&sd,"/sd/asset.json",FS_MODE_READ)==FS_OK);
    assert(fs_close(&sd)==FS_OK);
    fail_dir_read=true;
    assert(fs_list_dir("/sd",ignore_entry,NULL)==FS_ERROR_IO);
    assert(s_format_calls==0);
    puts("SD mount separation, recovery and no-format tests passed");
}

static void test_sd_write_failures(void) {
    assert(fs_init() == FS_OK);
    const struct {
        FRESULT write, close;
        bool short_count;
        fs_result_t expected_write, expected_close;
    } cases[] = {
        {FR_OK, FR_OK, false, FS_OK, FS_OK},
        {FR_OK, FR_OK, true, FS_ERROR_NO_SPACE, FS_OK},
        {FR_WRITE_PROTECTED, FR_OK, false, FS_ERROR_READ_ONLY, FS_OK},
        {FR_DISK_ERR, FR_DISK_ERR, false, FS_ERROR_IO, FS_ERROR_IO},
        {FR_OK, FR_DISK_ERR, false, FS_OK, FS_ERROR_IO},
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(*cases); i++) {
        fs_file_t file = {0}, peer = {0}, app = {0}, retry = {0};
        size_t written = 99, size;
        assert(fs_open(&file, "/sd/asset.json", FS_MODE_WRITE | FS_MODE_CREATE | FS_MODE_TRUNCATE) == FS_OK);
        assert(fs_open(&peer, "/sd/asset.json", FS_MODE_READ) == FS_OK);
        unsigned mounts_before = sd_mounts;
        write_result = cases[i].write;
        close_result = cases[i].close;
        short_write = cases[i].short_count;
        assert(fs_write(&file, "data", 4, &written) == cases[i].expected_write);
        assert(written == (write_result != FR_OK ? 0u : short_write ? 3u : 4u));
        assert(fs_close(&file) == cases[i].expected_close);
        assert(!file.is_open && file.internal == NULL);
        bool invalidated = write_result == FR_DISK_ERR || close_result == FR_DISK_ERR;
        write_result = close_result = FR_OK;
        short_write = false;
        if (invalidated) {
            assert(fs_size(&peer, &size) == FS_ERROR_IO);
            assert(fs_open(&retry, "/sd/asset.json", FS_MODE_READ) == FS_ERROR_IO);
            assert(sd_mounts == mounts_before); /* Wait for every stale handle. */
        } else {
            assert(fs_size(&peer, &size) == FS_OK);
        }
        assert(fs_open(&app, "/app/open.js", FS_MODE_READ) == FS_OK);
        assert(fs_close(&app) == FS_OK);
        assert(fs_close(&peer) == (invalidated ? FS_ERROR_IO : FS_OK));
        assert(fs_open(&retry, "/sd/asset.json", FS_MODE_WRITE | FS_MODE_CREATE | FS_MODE_TRUNCATE) == FS_OK);
        assert(sd_mounts == mounts_before + (invalidated ? 1u : 0u));
        assert(fs_write(&retry, "data", 4, &written) == FS_OK && written == 4);
        assert(fs_close(&retry) == FS_OK);
        assert(s_format_calls == 0);
    }
    puts("SD write/short-write/close faults, peer invalidation and retry: PASS");
}
#endif

int main(int argc, char **argv) {
    assert(argc == 2);
#ifdef MCUJS_TEST_DUAL_MSC
    if (!strcmp(argv[1], "sd-read-checks")) { test_sd_io_checks(false); return 0; }
    if (!strcmp(argv[1], "sd-sync-checks")) { test_sd_io_checks(true); return 0; }
    if (!strcmp(argv[1], "sd-malformed")) { test_sd_malformed(); return 0; }
    if (!strcmp(argv[1], "sd-bounds")) { test_sd_bounds(); return 0; }
    if (!strcmp(argv[1], "dual")) { test_dual_msc(); return 0; }
#endif
#if MCUJS_HAS_SD
    if (!strcmp(argv[1], "sd")) { test_sd_mount(); return 0; }
    if (!strcmp(argv[1], "sd-write")) { test_sd_write_failures(); return 0; }
#endif
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
