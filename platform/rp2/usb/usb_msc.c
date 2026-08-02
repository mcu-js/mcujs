/* Exclusive-owner USB MSC adapter for RP2 flash storage. */

#include "usb_msc.h"

#include "msc_ownership.h"
#include "tusb.h"

#include <string.h>

#define MCUJS_SCSI_SYNCHRONIZE_CACHE_10 0x35u
#define SCSI_CMD_MODE_SENSE_6 0x1au
#define SCSI_CMD_MODE_SENSE_10 0x5au

static mcujs_msc_ownership_t s_ownership;
static bool s_ejected;

typedef enum {
    MEDIA_STATE_IDLE = 0,
    MEDIA_STATE_NO_MEDIA,
    MEDIA_STATE_RESET,
    MEDIA_STATE_CHANGED,
} media_change_state_t;

static media_change_state_t s_media_state;

static fs_result_t begin_host_access(void *context) {
    (void)context;
    return fs_begin_host_access();
}

static fs_result_t end_host_access(void *context) {
    (void)context;
    return fs_end_host_access();
}

static bool host_owned(void *context) {
    (void)context;
    return fs_host_owned();
}

static const mcujs_msc_ownership_hooks_t s_hooks = {
    .context = NULL,
    .begin_host_access = begin_host_access,
    .end_host_access = end_host_access,
    .host_owned = host_owned,
};

void usb_msc_init(void) {
    mcujs_msc_ownership_init(&s_ownership);
    s_ejected = false;
    s_media_state = MEDIA_STATE_IDLE;
}

bool usb_msc_expose(void) {
    bool ready = mcujs_msc_ownership_expose(&s_ownership, &s_hooks);
    s_ejected = !ready;
    return ready;
}

void usb_msc_task(void) {
    mcujs_msc_ownership_task(&s_ownership, &s_hooks);
}

void usb_msc_event(mcujs_msc_event_t event) {
    if (event == MCUJS_MSC_EVENT_LOAD) {
        s_ejected = false;
    } else if (event == MCUJS_MSC_EVENT_EJECT ||
               event == MCUJS_MSC_EVENT_DETACH) {
        s_ejected = true;
    }
    mcujs_msc_ownership_event(&s_ownership, event);
}

bool usb_msc_ejected(void) {
    return s_ejected;
}

void usb_msc_reset_ejected(void) {
    usb_msc_event(MCUJS_MSC_EVENT_LOAD);
}

void usb_msc_media_changed(void) {
    if (s_media_state == MEDIA_STATE_IDLE) {
        s_media_state = MEDIA_STATE_NO_MEDIA;
    }
}

#if CFG_TUD_MSC > 0

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
    memcpy(product_id, "Flash Storage   ", 16);
    memcpy(product_rev, "0.2 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    if (!mcujs_msc_ownership_media_ready(&s_ownership) || !fs_host_owned()) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    switch (s_media_state) {
        case MEDIA_STATE_NO_MEDIA:
            tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
            s_media_state = MEDIA_STATE_RESET;
            return false;
        case MEDIA_STATE_RESET:
            tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x29, 0x00);
            s_media_state = MEDIA_STATE_CHANGED;
            return false;
        case MEDIA_STATE_CHANGED:
            tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00);
            s_media_state = MEDIA_STATE_IDLE;
            return false;
        case MEDIA_STATE_IDLE:
        default:
            return true;
    }
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
    (void)lun;
    *block_size = FS_SECTOR_SIZE;
    *block_count = fs_get_total_sectors();
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;
    if (load_eject) {
        usb_msc_event(start ? MCUJS_MSC_EVENT_LOAD : MCUJS_MSC_EVENT_EJECT);
    }
    return true;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return mcujs_msc_ownership_media_ready(&s_ownership) && fs_host_owned();
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
    switch (scsi_cmd[0]) {
        case SCSI_CMD_MODE_SENSE_6: {
            if (buffer == NULL || bufsize < 4) break;
            uint8_t *response = buffer;
            memset(response, 0, 4);
            response[0] = 3;
            return 4;
        }
        case SCSI_CMD_MODE_SENSE_10: {
            if (buffer == NULL || bufsize < 8) break;
            uint8_t *response = buffer;
            memset(response, 0, 8);
            response[1] = 6;
            return 8;
        }
        case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
            return 0;
        case MCUJS_SCSI_SYNCHRONIZE_CACHE_10:
            if (fs_msc_sync() == FS_OK) return 0;
            tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x02);
            return -1;
        default:
            break;
    }
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

void tud_msc_write10_complete_cb(uint8_t lun) {
    if (fs_msc_sync() != FS_OK) {
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x02);
    }
}

#endif /* CFG_TUD_MSC > 0 */
