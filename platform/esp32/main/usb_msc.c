/* Exclusive-owner USB MSC adapter for the ESP32-S3 FFAT volume. */

#include "usb_msc.h"

#include "filesystem.h"
#include "fs.h"
#include "msc_ownership.h"
#include "sdkconfig.h"
#include "tusb.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MCUJS_SCSI_SYNCHRONIZE_CACHE_10 0x35u

static mcujs_msc_ownership_t s_ownership;

static fs_result_t begin_host_access(void *context) {
    (void)context;
    return mcujs_filesystem_begin_host_access();
}

static fs_result_t end_host_access(void *context) {
    (void)context;
    return mcujs_filesystem_end_host_access();
}

static bool host_owned(void *context) {
    (void)context;
    return mcujs_filesystem_host_owned();
}

static const mcujs_msc_ownership_hooks_t s_hooks = {
    .context = NULL,
    .begin_host_access = begin_host_access,
    .end_host_access = end_host_access,
    .host_owned = host_owned,
};

void mcujs_usb_msc_init(void) {
    mcujs_msc_ownership_init(&s_ownership);
}

bool mcujs_usb_msc_expose(void) {
    return mcujs_msc_ownership_expose(&s_ownership, &s_hooks);
}

void mcujs_usb_msc_task(void) {
    mcujs_msc_ownership_task(&s_ownership, &s_hooks);
}

void mcujs_usb_msc_event(mcujs_msc_event_t event) {
    mcujs_msc_ownership_event(&s_ownership, event);
}

void tud_mount_cb(void) {
    mcujs_usb_msc_event(MCUJS_MSC_EVENT_LOAD);
}

void tud_umount_cb(void) {
    mcujs_usb_msc_event(MCUJS_MSC_EVENT_DETACH);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    mcujs_usb_msc_event(MCUJS_MSC_EVENT_SUSPEND);
}

void tud_resume_cb(void) {
    mcujs_usb_msc_event(MCUJS_MSC_EVENT_RESUME);
}

static bool begin_io(uint8_t lun) {
    if (!mcujs_msc_ownership_begin_io(&s_ownership, &s_hooks)) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }
    return true;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id, "MCUJS   ", 8);
    memcpy(product_id, "Runtime Disk    ", 16);
    memcpy(product_rev, "0.2 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    if (!mcujs_msc_ownership_media_ready(&s_ownership) ||
        !mcujs_filesystem_host_owned()) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
    (void)lun;
    *block_count = fs_get_total_sectors();
    uint32_t sector_size = mcujs_filesystem_sector_size();
    *block_size = (uint16_t)(sector_size == 0 ? CONFIG_WL_SECTOR_SIZE : sector_size);
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;
    if (load_eject) {
        mcujs_usb_msc_event(start ? MCUJS_MSC_EVENT_LOAD : MCUJS_MSC_EVENT_EJECT);
    }
    return true;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return mcujs_msc_ownership_media_ready(&s_ownership) &&
           mcujs_filesystem_host_owned();
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
    if (!begin_io(lun)) {
        return -1;
    }
    fs_result_t result = fs_read_sector(lba, offset, buffer, bufsize);
    mcujs_msc_ownership_end_io(&s_ownership);
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
    mcujs_msc_ownership_end_io(&s_ownership);
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
            return 0;
        case MCUJS_SCSI_SYNCHRONIZE_CACHE_10:
            if (fs_msc_sync() == FS_OK) return 0;
            tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x02);
            return -1;
        default:
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
            return -1;
    }
}
