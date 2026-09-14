/* Exclusive-owner USB MSC: app flash and, on explicitly enabled builds, SD. */
#include "usb_msc.h"
#include "msc_ownership.h"
#include "tusb.h"
#include <string.h>

#define MCUJS_SCSI_SYNCHRONIZE_CACHE_10 0x35u
#define SCSI_CMD_MODE_SENSE_6 0x1au
#define SCSI_CMD_MODE_SENSE_10 0x5au
#if MCUJS_USB_SD_MSC
#define VOLUME_COUNT 2
#else
#define VOLUME_COUNT 1
#endif

typedef struct {
    mcujs_msc_ownership_t ownership;
    fs_result_t error;
    bool ejected;
    bool prevent_removal;
    unsigned media_state;
} volume_t;
static volume_t s_volumes[VOLUME_COUNT];
static bool s_started;

static uint8_t volume_id(void *context) { return (uint8_t)((volume_t *)context - s_volumes); }
static fs_result_t begin_host_access(void *context) {
    volume_t *volume = context;
    volume->error = fs_volume_begin_host_access(volume_id(context));
    return volume->error;
}
static fs_result_t end_host_access(void *context) {
    volume_t *volume = context;
    volume->error = fs_volume_end_host_access(volume_id(context));
    return volume->error;
}
static bool host_owned(void *context) { return fs_volume_host_owned(volume_id(context)); }
static mcujs_msc_ownership_hooks_t hooks(uint8_t lun) {
    return (mcujs_msc_ownership_hooks_t){ &s_volumes[lun], begin_host_access, end_host_access, host_owned };
}

void usb_msc_init(void) {
    s_started = false;
    memset(s_volumes, 0, sizeof(s_volumes));
    for (unsigned i = 0; i < VOLUME_COUNT; i++) mcujs_msc_ownership_init(&s_volumes[i].ownership);
}

void usb_msc_task(void) {
    if (!s_started) return;
    for (uint8_t i = 0; i < VOLUME_COUNT; i++) {
        volume_t *volume = &s_volumes[i];
        mcujs_msc_ownership_hooks_t h = hooks(i);
        mcujs_msc_ownership_task(&volume->ownership, &h);
        /* Busy handles may drain; absent/failed media requires an explicit new
         * load (or reconnect), not repeated SD initialization on every tick. */
        if (volume->error != FS_OK && volume->error != FS_ERROR_BUSY) {
            int expected = MCUJS_MSC_OWNER_REQUEST_HOST;
            (void)atomic_compare_exchange_strong(&volume->ownership.owner_request,
                &expected, MCUJS_MSC_OWNER_REQUEST_NONE);
        }
    }
}

bool usb_msc_expose(void) {
    s_started = true;
    usb_msc_event(MCUJS_MSC_EVENT_LOAD);
    usb_msc_task();
    /* An absent SD must not prevent app export or trigger whole-device retry. */
    return mcujs_msc_ownership_media_ready(&s_volumes[0].ownership);
}

void usb_msc_event(mcujs_msc_event_t event) {
    if (!s_started) return;
    for (uint8_t i = 0; i < VOLUME_COUNT; i++) {
        volume_t *volume = &s_volumes[i];
        /* TinyUSB unmount can mean bus reset/deconfiguration, not safe eject.
         * Keep ownership fenced on detach; never infer host cache flush. */
        if (event == MCUJS_MSC_EVENT_DETACH || event == MCUJS_MSC_EVENT_EJECT) continue;
        if (event == MCUJS_MSC_EVENT_LOAD) {
            if (fs_volume_host_owned(i) || atomic_load(&volume->ownership.owner_request) == MCUJS_MSC_OWNER_REQUEST_DEVICE) continue;
            volume->ejected = false;
            volume->error = FS_OK;
        }
        mcujs_msc_ownership_event(&volume->ownership, event);
    }
}

bool usb_msc_ejected(void) {
    /* Preserve the legacy app-volume query; this is not an all-LUN barrier. */
    return s_volumes[0].ejected;
}
void usb_msc_reset_ejected(void) { usb_msc_event(MCUJS_MSC_EVENT_LOAD); }
void usb_msc_media_changed(void) { s_volumes[0].media_state = 1; }

#if CFG_TUD_MSC > 0
/* This adapter returns each complete callback chunk. With a 512-byte endpoint
 * buffer TinyUSB supplies offset zero for complete sectors; partial chunks
 * must stay within one sector. Reject other ranges before touching storage. */
_Static_assert(CFG_TUD_MSC_EP_BUFSIZE == FS_SECTOR_SIZE, "RP2 MSC requires a 512-byte endpoint buffer");
/* TinyUSB expects a LUN COUNT here; it subtracts one for GET_MAX_LUN. */
uint8_t tud_msc_get_maxlun_cb(void) { return VOLUME_COUNT; }

static bool valid_lun(uint8_t lun) {
    if (lun < VOLUME_COUNT) return true;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x25, 0);
    return false;
}
static bool result_sense(uint8_t lun, fs_result_t result, bool write) {
    switch (result) {
        case FS_OK: return true;
        case FS_ERROR_READ_ONLY: tud_msc_set_sense(lun, 0x07, 0x27, 0); break;
        case FS_ERROR_INVALID: tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0); break;
        case FS_ERROR_UNSUPPORTED: tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x30, 0); break;
        case FS_ERROR_BUSY: tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x04, 0x01); break;
        case FS_ERROR_NO_MEDIA: tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0); break;
        default: tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, write ? 0x0c : 0x11, write ? 2 : 0); break;
    }
    return false;
}
static bool ready(uint8_t lun) {
    if (!valid_lun(lun)) return false;
    volume_t *volume = &s_volumes[lun];
    if (!mcujs_msc_ownership_media_ready(&volume->ownership))
        return result_sense(lun, volume->ejected ? FS_ERROR_NO_MEDIA :
            volume->error == FS_OK ? FS_ERROR_BUSY : volume->error, false);
    return result_sense(lun, fs_volume_msc_status(lun), false);
}
static bool begin_io(uint8_t lun) {
    if (!ready(lun)) return false;
    mcujs_msc_ownership_hooks_t h = hooks(lun);
    if (!mcujs_msc_ownership_begin_io(&s_volumes[lun].ownership, &h))
        return result_sense(lun, FS_ERROR_BUSY, false);
    return true;
}
static bool sync_volume(uint8_t lun) {
    if (!begin_io(lun)) return false;
    fs_result_t result = fs_volume_msc_sync(lun);
    mcujs_msc_ownership_end_io(&s_volumes[lun].ownership);
    return result_sense(lun, result, true);
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    memset(vendor_id, ' ', 8); memset(product_id, ' ', 16); memset(product_rev, ' ', 4);
    if (!valid_lun(lun)) return;
    memcpy(vendor_id, "MCUJS   ", 8);
    memcpy(product_id, lun == 0 ? "Flash Storage   " : "SD Storage      ", 16);
    memcpy(product_rev, "0.2 ", 4);
}
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    if (!ready(lun)) return false;
    unsigned state = s_volumes[lun].media_state;
    if (!state) return true;
    s_volumes[lun].media_state = state == 3 ? 0 : state + 1;
    tud_msc_set_sense(lun, state == 1 ? SCSI_SENSE_NOT_READY : SCSI_SENSE_UNIT_ATTENTION,
        state == 1 ? 0x3a : state == 2 ? 0x29 : 0x28, 0);
    return false;
}
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    *block_count = 0; *block_size = FS_SECTOR_SIZE;
    if (!ready(lun)) return;
    (void)result_sense(lun, fs_volume_capacity(lun, block_count), false);
}
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void)power_condition;
    if (!valid_lun(lun) || !s_started) return false;
    if (!load_eject) return start ? ready(lun) : sync_volume(lun);
    volume_t *volume = &s_volumes[lun];
    if (start) {
        if (atomic_load(&volume->ownership.owner_request) == MCUJS_MSC_OWNER_REQUEST_DEVICE)
            return result_sense(lun, FS_ERROR_BUSY, false);
        volume->ejected = false;
        volume->error = FS_OK;
        mcujs_msc_ownership_event(&volume->ownership, MCUJS_MSC_EVENT_LOAD);
    } else {
        if (volume->prevent_removal) {
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x53, 2);
            return false;
        }
        if (volume->ejected) return true;
        /* A pending LOAD with local handles has not exposed any blocks yet.
         * EJECT must cancel that intent rather than leaving a future claim. */
        if (fs_volume_host_owned(lun) && !sync_volume(lun)) return false;
        volume->ejected = true;
        mcujs_msc_ownership_event(&volume->ownership, MCUJS_MSC_EVENT_EJECT);
    }
    return true;
}
bool tud_msc_is_writable_cb(uint8_t lun) {
    return ready(lun) && fs_volume_writable(lun);
}
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    if (!begin_io(lun)) return -1;
    fs_result_t result = fs_volume_read_sector(lun, lba, offset, buffer, bufsize);
    mcujs_msc_ownership_end_io(&s_volumes[lun].ownership);
    return result_sense(lun, result, false) ? (int32_t)bufsize : -1;
}
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    if (!begin_io(lun)) return -1;
    fs_result_t result = fs_volume_write_sector(lun, lba, offset, buffer, bufsize);
    mcujs_msc_ownership_end_io(&s_volumes[lun].ownership);
    return result_sense(lun, result, true) ? (int32_t)bufsize : -1;
}
/* TinyUSB handles PREVENT/ALLOW internally and invokes this dedicated hook. */
bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun, uint8_t prohibit_removal, uint8_t control) {
    (void)control;
    if (!valid_lun(lun)) return false;
    /* Only non-persistent prevent/allow is supported (SPC values 0 and 1). */
    if (prohibit_removal > 1) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x24, 0);
        return false;
    }
    s_volumes[lun].prevent_removal = prohibit_removal != 0;
    return true;
}
int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void *buffer, uint16_t bufsize) {
    if (!valid_lun(lun)) return -1;
    switch (scsi_cmd[0]) {
        case SCSI_CMD_MODE_SENSE_6:
        case SCSI_CMD_MODE_SENSE_10: {
            bool ten = scsi_cmd[0] == SCSI_CMD_MODE_SENSE_10;
            unsigned size = ten ? 8 : 4;
            if (!buffer || bufsize < size) break;
            if (!ready(lun)) return -1;
            uint8_t *response = buffer;
            memset(response, 0, size);
            response[ten ? 1 : 0] = size - (ten ? 2 : 1);
            response[ten ? 3 : 2] = fs_volume_writable(lun) ? 0 : 0x80;
            return (int32_t)size;
        }
        case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
            return tud_msc_prevent_allow_medium_removal_cb(lun, scsi_cmd[4], scsi_cmd[5]) ? 0 : -1;
        case MCUJS_SCSI_SYNCHRONIZE_CACHE_10:
            return sync_volume(lun) ? 0 : -1;
        default: break;
    }
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0);
    return -1;
}
void tud_msc_write10_complete_cb(uint8_t lun) {
    /* Void completion cannot fail the already-issued CSW; sector writes are
     * synchronous and report their failures in write10, not only here. */
    (void)sync_volume(lun);
}
#endif
