#include "fs.h"
#include "msc_ownership.h"
#include "tusb.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(MCUJS_TEST_ESP32)
#include "usb_msc.h"
#define backend_init mcujs_usb_msc_init
#define backend_expose mcujs_usb_msc_expose
#define backend_task mcujs_usb_msc_task
#define backend_event mcujs_usb_msc_event
#else
#include "usb_msc.h"
#define backend_init usb_msc_init
#define backend_expose usb_msc_expose
#define backend_task usb_msc_task
#define backend_event usb_msc_event
#endif

static bool s_host_owned;
static fs_result_t s_begin_result = FS_OK;
static fs_result_t s_end_result = FS_OK;
static fs_result_t s_sync_result = FS_OK;
static unsigned s_begin_calls;
static unsigned s_end_calls;
static unsigned s_sync_calls;
static unsigned s_read_calls;
static unsigned s_write_calls;
static unsigned s_sense_calls;
static uint8_t s_last_sense;

static fs_result_t begin_host_access(void) {
    s_begin_calls++;
    if (s_begin_result == FS_OK) s_host_owned = true;
    return s_begin_result;
}
static fs_result_t end_host_access(void) {
    s_end_calls++;
    if (s_end_result == FS_OK) s_host_owned = false;
    return s_end_result;
}

#if defined(MCUJS_TEST_ESP32)
fs_result_t mcujs_filesystem_begin_host_access(void) { return begin_host_access(); }
fs_result_t mcujs_filesystem_end_host_access(void) { return end_host_access(); }
bool mcujs_filesystem_host_owned(void) { return s_host_owned; }
uint32_t mcujs_filesystem_sector_size(void) { return 4096; }
#else
fs_result_t fs_begin_host_access(void) { return begin_host_access(); }
fs_result_t fs_end_host_access(void) { return end_host_access(); }
bool fs_host_owned(void) { return s_host_owned; }
#endif

fs_result_t fs_msc_sync(void) {
    s_sync_calls++;
    return s_sync_result;
}
fs_result_t fs_sync(void) { return fs_msc_sync(); }
uint32_t fs_get_total_sectors(void) { return 64; }
fs_result_t fs_read_sector(uint32_t sector, uint32_t offset,
                           void *buffer, uint32_t size) {
    (void)sector;
    (void)offset;
    memset(buffer, 0x5a, size);
    s_read_calls++;
    return FS_OK;
}
fs_result_t fs_write_sector(uint32_t sector, uint32_t offset,
                            const void *buffer, uint32_t size) {
    (void)sector;
    (void)offset;
    (void)buffer;
    (void)size;
    s_write_calls++;
    return FS_OK;
}
void tud_msc_set_sense(uint8_t lun, uint8_t sense_key, uint8_t add_sense_code,
                       uint8_t add_sense_qualifier) {
    (void)lun;
    (void)add_sense_code;
    (void)add_sense_qualifier;
    s_sense_calls++;
    s_last_sense = sense_key;
}

bool tud_msc_test_unit_ready_cb(uint8_t lun);
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size);
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject);
bool tud_msc_is_writable_cb(uint8_t lun);
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize);
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize);
int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16],
                        void *buffer, uint16_t bufsize);

static void test_backend_handoff_and_callbacks(void) {
    backend_init();
    assert(backend_expose());
    assert(s_host_owned);
    assert(s_begin_calls == 1);
    assert(tud_msc_test_unit_ready_cb(0));
    assert(tud_msc_is_writable_cb(0));

    uint32_t blocks = 0;
    uint16_t block_size = 0;
    tud_msc_capacity_cb(0, &blocks, &block_size);
    assert(blocks == 64);
#if defined(MCUJS_TEST_ESP32)
    assert(block_size == 4096);
#else
    assert(block_size == FS_SECTOR_SIZE);
#endif

    uint8_t data[16] = {0};
    assert(tud_msc_read10_cb(0, 0, 0, data, sizeof(data)) == (int32_t)sizeof(data));
    assert(tud_msc_write10_cb(0, 0, 0, data, sizeof(data)) == (int32_t)sizeof(data));
    assert(s_read_calls == 1);
    assert(s_write_calls == 1);

    uint8_t sync_command[16] = {0x35};
    assert(tud_msc_scsi_cb(0, sync_command, NULL, 0) == 0);
    assert(s_sync_calls == 1);

    backend_event(MCUJS_MSC_EVENT_RESET);
    backend_event(MCUJS_MSC_EVENT_SUSPEND);
    backend_event(MCUJS_MSC_EVENT_RESUME);
    backend_task();
    assert(s_host_owned);
    assert(s_end_calls == 0);
    assert(tud_msc_test_unit_ready_cb(0));

    assert(tud_msc_start_stop_cb(0, 0, false, true));
    assert(!tud_msc_test_unit_ready_cb(0));
    assert(s_last_sense == SCSI_SENSE_NOT_READY);
    backend_task();
    assert(!s_host_owned);
    assert(s_end_calls == 1);
    assert(!tud_msc_is_writable_cb(0));

    assert(tud_msc_start_stop_cb(0, 0, true, true));
    backend_task();
    assert(s_host_owned);
    assert(s_begin_calls == 2);

    backend_event(MCUJS_MSC_EVENT_DETACH);
    backend_task();
    assert(!s_host_owned);
    assert(s_end_calls == 2);
}

static void test_transition_failure_stays_unready(void) {
    s_begin_result = FS_ERROR_IO;
    backend_init();
    assert(!backend_expose());
    assert(!s_host_owned);
    assert(!tud_msc_test_unit_ready_cb(0));
    assert(!tud_msc_is_writable_cb(0));
}

int main(void) {
    test_backend_handoff_and_callbacks();
    test_transition_failure_stays_unready();
#if defined(MCUJS_TEST_ESP32)
    puts("ESP32 MSC backend test passed");
#else
    puts("RP2 MSC backend test passed");
#endif
    return 0;
}
