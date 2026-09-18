/* Real ESP transport callbacks + production ownership state machine;
 * only the physical/filesystem ABI is replaced with independent byte media. */
#include "fs.h"
#include "usb_msc.h"
#include "tusb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static uint8_t media[2][16384];
static bool host[2], busy[2];
static fs_result_t write_error[2], sync_error[2];
static unsigned calls[2], releases[2];
static uint8_t sense_lun, sense_asc;
static unsigned block(uint8_t v) { assert(v < 2); return v ? 512 : 4096; }
uint32_t mcujs_filesystem_sector_size(void) { return 4096; }
fs_result_t fs_volume_begin_host_access(uint8_t v) { assert(v < 2); if (busy[v]) return FS_ERROR_BUSY; host[v] = true; return FS_OK; }
fs_result_t fs_volume_end_host_access(uint8_t v) { assert(v < 2); host[v] = false; releases[v]++; return FS_OK; }
bool fs_volume_host_owned(uint8_t v) { assert(v < 2); return host[v]; }
fs_result_t fs_volume_msc_status(uint8_t v) { assert(v < 2); return host[v] ? FS_OK : FS_ERROR_BUSY; }
fs_result_t fs_volume_msc_sync(uint8_t v) { assert(v < 2 && host[v]); return sync_error[v]; }
fs_result_t fs_volume_capacity(uint8_t v, uint32_t *n) { *n = sizeof(media[v]) / block(v); return FS_OK; }
bool fs_volume_writable(uint8_t v) { assert(v < 2); return host[v]; }
fs_result_t fs_volume_read_sector(uint8_t v, uint32_t s, uint32_t o, void *b, uint32_t n) {
    unsigned bs = block(v); assert(host[v] && o + n <= bs && s < sizeof(media[v])/bs);
    calls[v]++; memcpy(b, media[v] + s*bs + o, n); return FS_OK;
}
fs_result_t fs_volume_write_sector(uint8_t v, uint32_t s, uint32_t o, const void *b, uint32_t n) {
    unsigned bs = block(v); assert(host[v] && o + n <= bs && s < sizeof(media[v])/bs);
    calls[v]++; if (write_error[v]) return write_error[v];
    memcpy(media[v] + s*bs + o, b, n); return FS_OK;
}
void tud_msc_set_sense(uint8_t lun, uint8_t key, uint8_t asc, uint8_t qual) {
    (void)key; (void)qual; sense_lun = lun; sense_asc = asc;
}
uint8_t tud_msc_get_maxlun_cb(void);
void tud_msc_inquiry_cb(uint8_t, uint8_t *, uint8_t *, uint8_t *);
bool tud_msc_test_unit_ready_cb(uint8_t);
bool tud_msc_is_writable_cb(uint8_t);
void tud_msc_capacity_cb(uint8_t, uint32_t *, uint16_t *);
bool tud_msc_start_stop_cb(uint8_t, uint8_t, bool, bool);
bool tud_msc_prevent_allow_medium_removal_cb(uint8_t, uint8_t, uint8_t);
int32_t tud_msc_read10_cb(uint8_t, uint32_t, uint32_t, void *, uint32_t);
int32_t tud_msc_write10_cb(uint8_t, uint32_t, uint32_t, uint8_t *, uint32_t);
int32_t tud_msc_scsi_cb(uint8_t, const uint8_t *, void *, uint16_t);
void tud_msc_write10_complete_cb(uint8_t);
void tud_umount_cb(void);
void tud_suspend_cb(bool);
static void reset(void) {
    memset(host, 0, sizeof(host)); memset(busy, 0, sizeof(busy));
    memset(write_error, 0, sizeof(write_error)); memset(sync_error, 0, sizeof(sync_error));
    mcujs_usb_msc_init();
}
int main(void) {
    reset(); busy[0] = true;
    assert(!mcujs_usb_msc_expose());
    assert(!host[0] && host[1] && tud_msc_test_unit_ready_cb(1));
    busy[0] = false; mcujs_usb_msc_task();
    assert(tud_msc_get_maxlun_cb() == 2 && host[0]);
    uint8_t input[9000], output[9000], cmd[16] = {0x35};
    for (unsigned i = 0; i < sizeof(input); i++) input[i] = (uint8_t)(i * 37);
    for (uint8_t v = 0; v < 2; v++) {
        uint32_t n; uint16_t bs;
        tud_msc_capacity_cb(v, &n, &bs); assert(bs == block(v) && n == 16384 / bs);
        memset(media[v], 0xa7, sizeof(media[v]));
        assert(tud_msc_write10_cb(v, 0, 13, input, sizeof(input)) == sizeof(input));
        assert(media[v][12] == 0xa7 && media[v][9013] == 0xa7);
        assert(tud_msc_read10_cb(v, 0, 13, output, sizeof(output)) == sizeof(output));
        assert(!memcmp(input, output, sizeof(input)));
        unsigned before = calls[v];
        assert(tud_msc_write10_cb(v, UINT32_MAX, 0, input, 1) == -1);
        assert(tud_msc_read10_cb(v, 0, UINT32_MAX, output, 1) == -1);
        assert(tud_msc_read10_cb(v, n - 1, bs - 1, output, 2) == -1);
        assert(tud_msc_read10_cb(v, 0, 0, output, UINT32_MAX) == -1);
        assert(calls[v] == before);
        assert(tud_msc_prevent_allow_medium_removal_cb(v, 1, 0));
        assert(!tud_msc_start_stop_cb(v, 0, false, true));
        assert(tud_msc_prevent_allow_medium_removal_cb(v, 0, 0));
        assert(tud_msc_start_stop_cb(v, 0, false, true));
        assert(!tud_msc_test_unit_ready_cb(v));
        tud_msc_capacity_cb(v, &n, &bs); assert(n == 0 && bs == 0);
        assert(!tud_msc_is_writable_cb(v));
        assert(tud_msc_read10_cb(v, 0, 0, output, 1) == -1);
        assert(!tud_msc_start_stop_cb(v, 0, true, true)); /* release pending */
        mcujs_usb_msc_task(); assert(!host[v]);
        busy[v] = true;
        assert(tud_msc_start_stop_cb(v, 0, true, true)); mcujs_usb_msc_task(); assert(!host[v]);
        assert(tud_msc_start_stop_cb(v, 0, false, true)); /* cancel pending load */
        busy[v] = false; mcujs_usb_msc_task(); assert(!host[v]);
        assert(tud_msc_start_stop_cb(v, 0, true, true)); mcujs_usb_msc_task(); assert(host[v]);
    }
    for (unsigned invalid = 2; invalid <= 255; invalid += 253) {
        uint8_t v = invalid, a[8], b[16], c[4]; uint32_t n = 5; uint16_t bs = 5;
        assert(!tud_msc_test_unit_ready_cb(v) && !tud_msc_is_writable_cb(v));
        tud_msc_capacity_cb(v, &n, &bs); assert(!n && !bs);
        tud_msc_inquiry_cb(v, a, b, c); assert(a[0] == ' ');
        assert(!tud_msc_start_stop_cb(v, 0, true, true));
        assert(!tud_msc_prevent_allow_medium_removal_cb(v, 1, 0));
        assert(tud_msc_read10_cb(v, 0, 0, output, 1) == -1);
        assert(tud_msc_write10_cb(v, 0, 0, input, 1) == -1);
        assert(tud_msc_scsi_cb(v, cmd, NULL, 0) == -1);
        tud_msc_write10_complete_cb(v); assert(sense_lun == v && sense_asc == 0x25);
    }
    tud_umount_cb(); tud_suspend_cb(false); mcujs_usb_msc_event(MCUJS_MSC_EVENT_RESET);
    mcujs_usb_msc_task(); assert(host[0] && host[1]);
    write_error[1] = FS_ERROR_IO;
    assert(tud_msc_write10_cb(1, 0, 0, input, 1) == -1);
    assert(!tud_msc_test_unit_ready_cb(1) && tud_msc_test_unit_ready_cb(0));
    mcujs_usb_msc_task(); assert(host[1]); /* failed write retains host fence */
    assert(!tud_msc_start_stop_cb(1, 0, false, true));
    reset(); assert(mcujs_usb_msc_expose()); sync_error[0] = FS_ERROR_IO;
    assert(tud_msc_write10_cb(0, 0, 0, input, 1) == -1);
    assert(!tud_msc_is_writable_cb(0) && tud_msc_test_unit_ready_cb(1));
    reset(); assert(mcujs_usb_msc_expose()); sync_error[1] = FS_ERROR_IO;
    assert(tud_msc_scsi_cb(1, cmd, NULL, 0) == -1);
    assert(!tud_msc_is_writable_cb(1) && tud_msc_test_unit_ready_cb(0));
    puts("ESP32 dual MSC physical callback ABI harness: PASS");
}
