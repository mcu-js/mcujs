/* Exclusive-owner USB MSC adapter for the ESP32-S3 FFAT volume. */

#include "usb_msc.h"

#include "filesystem.h"
#include "fs.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "tusb.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MCUJS_SCSI_SYNCHRONIZE_CACHE_10 0x35u

static const char *TAG = "mcujs_usb_msc";
static atomic_bool s_media_ready;
static atomic_uint s_io_inflight;

typedef enum {
    MSC_OWNER_REQUEST_NONE = 0,
    MSC_OWNER_REQUEST_DEVICE,
    MSC_OWNER_REQUEST_HOST,
} msc_owner_request_t;

static atomic_int s_owner_request;

void mcujs_usb_msc_init(void) {
    atomic_store(&s_media_ready, false);
    atomic_store(&s_owner_request, MSC_OWNER_REQUEST_NONE);
    atomic_store(&s_io_inflight, 0);
}

bool mcujs_usb_msc_expose(void) {
    if (mcujs_filesystem_host_owned()) {
        atomic_store(&s_media_ready, true);
        return true;
    }
    if (mcujs_filesystem_begin_host_access() != FS_OK) {
        atomic_store(&s_media_ready, false);
        return false;
    }
    atomic_store(&s_media_ready, true);
    return true;
}

void mcujs_usb_msc_task(void) {
    msc_owner_request_t request = (msc_owner_request_t)atomic_load(&s_owner_request);
    if (request == MSC_OWNER_REQUEST_DEVICE) {
        atomic_store(&s_media_ready, false);
        if (atomic_load(&s_io_inflight) != 0) {
            return;
        }
        if (mcujs_filesystem_host_owned()) {
            fs_result_t result = mcujs_filesystem_end_host_access();
            if (result != FS_OK) {
                ESP_LOGE(TAG, "Failed to return storage to device ownership: %d", result);
                return;
            }
        }
        int expected = MSC_OWNER_REQUEST_DEVICE;
        (void)atomic_compare_exchange_strong(
            &s_owner_request, &expected, MSC_OWNER_REQUEST_NONE);
        return;
    }

    if (request == MSC_OWNER_REQUEST_HOST) {
        if (!mcujs_filesystem_host_owned() && !mcujs_usb_msc_expose()) {
            /* Keep the request pending. A synchronous local file operation may
             * still own a handle; retry after it closes. */
            return;
        }
        atomic_store(&s_media_ready, true);
        int expected = MSC_OWNER_REQUEST_HOST;
        (void)atomic_compare_exchange_strong(
            &s_owner_request, &expected, MSC_OWNER_REQUEST_NONE);
    }
}

static bool begin_io(uint8_t lun) {
    if (!atomic_load(&s_media_ready) ||
        atomic_load(&s_owner_request) == MSC_OWNER_REQUEST_DEVICE) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    atomic_fetch_add(&s_io_inflight, 1);
    if (!atomic_load(&s_media_ready) ||
        atomic_load(&s_owner_request) == MSC_OWNER_REQUEST_DEVICE ||
        !mcujs_filesystem_host_owned()) {
        atomic_fetch_sub(&s_io_inflight, 1);
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }
    return true;
}

static void end_io(void) {
    atomic_fetch_sub(&s_io_inflight, 1);
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id, "MCUJS   ", 8);
    memcpy(product_id, "Runtime Disk    ", 16);
    memcpy(product_rev, "0.2 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    if (!atomic_load(&s_media_ready) ||
        atomic_load(&s_owner_request) == MSC_OWNER_REQUEST_DEVICE ||
        !mcujs_filesystem_host_owned()) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
    (void)lun;
    /* Geometry is stable once FFAT initializes, even while TEST UNIT READY
     * reports no media during an ownership transition. Returning zero here
     * lets some hosts cache a permanent 0-byte disk after boot. */
    *block_count = fs_get_total_sectors();
    uint32_t sector_size = mcujs_filesystem_sector_size();
    *block_size = (uint16_t)(sector_size == 0 ? CONFIG_WL_SECTOR_SIZE : sector_size);
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;
    if (load_eject && !start) {
        atomic_store(&s_media_ready, false);
        atomic_store(&s_owner_request, MSC_OWNER_REQUEST_DEVICE);
    } else if (load_eject && start) {
        atomic_store(&s_owner_request, MSC_OWNER_REQUEST_HOST);
    }
    return true;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return atomic_load(&s_media_ready) &&
           atomic_load(&s_owner_request) != MSC_OWNER_REQUEST_DEVICE &&
           mcujs_filesystem_host_owned();
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
    if (!begin_io(lun)) {
        return -1;
    }
    fs_result_t result = fs_read_sector(lba, offset, buffer, bufsize);
    end_io();
    if (result != FS_OK) {
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x00);
        return -1;
    }
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
    if (!begin_io(lun)) {
        return -1;
    }
    fs_result_t result = fs_write_sector(lba, offset, buffer, bufsize);
    end_io();
    if (result != FS_OK) {
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x02);
        return -1;
    }
    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16],
                        void *buffer, uint16_t bufsize) {
    (void)buffer;
    (void)bufsize;
    switch (scsi_cmd[0]) {
        case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        case MCUJS_SCSI_SYNCHRONIZE_CACHE_10:
            return 0;
        default:
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
            return -1;
    }
}
