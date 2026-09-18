/* Exclusive-owner MSC transport. Storage and WL geometry remain backend-owned. */
#include "usb_msc.h"
#include "filesystem.h"
#include "fs.h"
#include "tusb.h"
#include "board_config.h"
#include <limits.h>
#include <string.h>

#if MCUJS_USB_SD_MSC
#define VOLUME_COUNT 2
#else
#define VOLUME_COUNT 1
#endif

typedef struct {
    mcujs_msc_ownership_t ownership;
    _Atomic fs_result_t error;
    atomic_bool ejected, prevent_removal, fault;
    atomic_uint sense;
} volume_t;
static volume_t s_volumes[VOLUME_COUNT];
static atomic_bool s_started;
static void set_sense(uint8_t lun, uint8_t key, uint8_t asc, uint8_t qual) {
    if (lun < VOLUME_COUNT)
        atomic_store(&s_volumes[lun].sense, ((unsigned)key << 16) | ((unsigned)asc << 8) | qual);
    tud_msc_set_sense(lun, key, asc, qual);
}
static uint8_t id(void *v) { return (uint8_t)((volume_t *)v - s_volumes); }
static fs_result_t claim(void *v) {
    return ((volume_t *)v)->error = fs_volume_begin_host_access(id(v));
}
static fs_result_t release(void *v) {
    return ((volume_t *)v)->error = fs_volume_end_host_access(id(v));
}
static bool owned(void *v) { return fs_volume_host_owned(id(v)); }
static mcujs_msc_ownership_hooks_t hooks(uint8_t lun) {
    return (mcujs_msc_ownership_hooks_t){ &s_volumes[lun], claim, release, owned };
}
void mcujs_usb_msc_init(void) {
    s_started = false;
    memset(s_volumes, 0, sizeof(s_volumes));
    for (unsigned i = 0; i < VOLUME_COUNT; i++) {
        mcujs_msc_ownership_init(&s_volumes[i].ownership);
        atomic_init(&s_volumes[i].error, FS_OK);
        atomic_init(&s_volumes[i].ejected, false);
        atomic_init(&s_volumes[i].prevent_removal, false);
        atomic_init(&s_volumes[i].fault, false);
        atomic_init(&s_volumes[i].sense, 0);
    }
}
void mcujs_usb_msc_task(void) {
    if (!s_started) return;
    for (uint8_t i = 0; i < VOLUME_COUNT; i++) {
        volume_t *v = &s_volumes[i];
        if (v->fault) continue; /* Never remount uncertain host writes. */
        mcujs_msc_ownership_hooks_t h = hooks(i);
        mcujs_msc_ownership_task(&v->ownership, &h);
        if (v->error != FS_OK && v->error != FS_ERROR_BUSY) {
            atomic_store(&v->ownership.owner_request, MCUJS_MSC_OWNER_REQUEST_NONE);
            v->fault = true;
        }
    }
}
void mcujs_usb_msc_event(mcujs_msc_event_t event) {
    if (!s_started) return;
    /* DETACH/EJECT here are explicit confirmed events. Ambiguous USB
     * deconfiguration is translated to RESET at the callback boundary below. */
    for (uint8_t i = 0; i < VOLUME_COUNT; i++) {
        volume_t *v = &s_volumes[i];
        if (v->fault) continue;
        if (event == MCUJS_MSC_EVENT_LOAD && (fs_volume_host_owned(i) ||
            atomic_load(&v->ownership.owner_request) == MCUJS_MSC_OWNER_REQUEST_DEVICE)) continue;
        mcujs_msc_ownership_event(&v->ownership, event);
    }
}
bool mcujs_usb_msc_expose(void) {
    s_started = true;
    mcujs_usb_msc_event(MCUJS_MSC_EVENT_LOAD);
    mcujs_usb_msc_task();
    return mcujs_msc_ownership_media_ready(&s_volumes[0].ownership);
}
void tud_mount_cb(void) { mcujs_usb_msc_event(MCUJS_MSC_EVENT_LOAD); }
/* TinyUSB unmount can be deconfiguration/reset, not confirmed physical detach. */
void tud_umount_cb(void) { mcujs_usb_msc_event(MCUJS_MSC_EVENT_RESET); }
void tud_suspend_cb(bool remote) { (void)remote; mcujs_usb_msc_event(MCUJS_MSC_EVENT_SUSPEND); }
void tud_resume_cb(void) { mcujs_usb_msc_event(MCUJS_MSC_EVENT_RESUME); }

/* TinyUSB's GET_MAX_LUN handler subtracts one from this COUNT. */
uint8_t tud_msc_get_maxlun_cb(void) { return VOLUME_COUNT; }
static bool valid(uint8_t lun) {
    if (lun < VOLUME_COUNT) return true;
    set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x25, 0);
    return false;
}
static bool result(uint8_t lun, fs_result_t r, bool write) {
    if (r == FS_OK) return true;
    switch (r) {
        case FS_ERROR_BUSY: set_sense(lun, SCSI_SENSE_NOT_READY, 4, 1); break;
        case FS_ERROR_NO_MEDIA: set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0); break;
        case FS_ERROR_INVALID: set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0); break;
        case FS_ERROR_READ_ONLY: set_sense(lun, 7, 0x27, 0); break;
        default: set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, write ? 0x0c : 0x11, write ? 2 : 0); break;
    }
    return false;
}
static void fence(uint8_t lun, fs_result_t r) {
    volume_t *v = &s_volumes[lun];
    v->error = r;
    v->fault = true;
    atomic_store(&v->ownership.media_ready, false);
    atomic_store(&v->ownership.owner_request, MCUJS_MSC_OWNER_REQUEST_NONE);
}
static bool storage_result(uint8_t lun, fs_result_t r, bool write) {
    if (r != FS_OK && r != FS_ERROR_BUSY && r != FS_ERROR_INVALID && r != FS_ERROR_READ_ONLY)
        fence(lun, r);
    return result(lun, r, write);
}
static bool ready(uint8_t lun) {
    if (!valid(lun)) return false;
    volume_t *v = &s_volumes[lun];
    if (!mcujs_msc_ownership_media_ready(&v->ownership))
        return result(lun, v->ejected ? FS_ERROR_NO_MEDIA : v->error == FS_OK ? FS_ERROR_BUSY : v->error, false);
    return storage_result(lun, fs_volume_msc_status(lun), false);
}
static bool begin_io(uint8_t lun) {
    if (!ready(lun)) return false;
    mcujs_msc_ownership_hooks_t h = hooks(lun);
    return mcujs_msc_ownership_begin_io(&s_volumes[lun].ownership, &h) || result(lun, FS_ERROR_BUSY, false);
}
static bool sync_volume(uint8_t lun) {
    if (!begin_io(lun)) return false;
    fs_result_t r = fs_volume_msc_sync(lun);
    if (r != FS_OK) fence(lun, r);
    mcujs_msc_ownership_end_io(&s_volumes[lun].ownership);
    return storage_result(lun, r, true);
}
static uint32_t sector_size(uint8_t lun) {
    return lun == 0 ? mcujs_filesystem_sector_size() : FS_SECTOR_SIZE;
}
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor[8], uint8_t product[16], uint8_t rev[4]) {
    memset(vendor, ' ', 8); memset(product, ' ', 16); memset(rev, ' ', 4);
    if (!valid(lun)) return;
    memcpy(vendor, "MCUJS   ", 8);
    memcpy(product, lun == 0 ? "Flash Storage   " : "SD Storage      ", 16);
    memcpy(rev, "0.2 ", 4);
}
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    if (!ready(lun)) return false;
    return lun == 0 || sync_volume(lun);
}
void tud_msc_capacity_cb(uint8_t lun, uint32_t *count, uint16_t *size) {
    *count = 0; *size = 0;
    if (!ready(lun)) return;
    uint32_t geometry = sector_size(lun), sectors = 0;
    if (!geometry || geometry > UINT16_MAX) { result(lun, FS_ERROR_INVALID, false); return; }
    if (!storage_result(lun, fs_volume_capacity(lun, &sectors), false)) return;
    *count = sectors; *size = (uint16_t)geometry;
}
bool tud_msc_is_writable_cb(uint8_t lun) { return ready(lun) && fs_volume_writable(lun); }
bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun, uint8_t prevent, uint8_t control) {
    (void)control;
    if (!valid(lun)) return false;
    if (prevent > 1) { set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x24, 0); return false; }
    s_volumes[lun].prevent_removal = prevent != 0;
    return true;
}
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power, bool start, bool eject) {
    (void)power;
    if (!valid(lun) || !s_started) return false;
    if (!eject) return start ? ready(lun) : sync_volume(lun);
    volume_t *v = &s_volumes[lun];
    if (v->fault) return result(lun, v->error, false);
    if (start) {
        if (atomic_load(&v->ownership.owner_request) == MCUJS_MSC_OWNER_REQUEST_DEVICE)
            return result(lun, FS_ERROR_BUSY, false);
        v->ejected = false;
        v->error = FS_OK;
        mcujs_msc_ownership_event(&v->ownership, MCUJS_MSC_EVENT_LOAD);
    } else {
        if (v->prevent_removal) { set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x53, 2); return false; }
        if (v->ejected) return true;
        if (fs_volume_host_owned(lun) && !sync_volume(lun)) return false;
        v->ejected = true;
        mcujs_msc_ownership_event(&v->ownership, MCUJS_MSC_EVENT_EJECT);
    }
    return true;
}
static int32_t transfer(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t size, bool write) {
    if (!begin_io(lun)) return -1;
    uint32_t block = sector_size(lun), count = 0;
    fs_result_t r = fs_volume_capacity(lun, &count);
    if (r == FS_OK && (!block || offset >= block || size > INT32_MAX || (!buffer && size) ||
        lba >= count || (uint64_t)lba * block + offset + size > (uint64_t)count * block)) r = FS_ERROR_INVALID;
    if (r == FS_OK && write && !fs_volume_writable(lun)) r = FS_ERROR_READ_ONLY;
    uint8_t *p = buffer;
    uint32_t left = size;
    while (r == FS_OK && left) {
        uint32_t chunk = block - offset;
        if (chunk > left) chunk = left;
        r = write ? fs_volume_write_sector(lun, lba, offset, p, chunk) : fs_volume_read_sector(lun, lba, offset, p, chunk);
        if (write && r != FS_OK) fence(lun, r);
        p += chunk; left -= chunk; lba++; offset = 0;
    }
    /* Fail this WRITE10, not a later void completion, if durability fails. */
    if (r == FS_OK && write) {
        r = fs_volume_msc_sync(lun);
        if (r != FS_OK) fence(lun, r);
    }
    mcujs_msc_ownership_end_io(&s_volumes[lun].ownership);
    return storage_result(lun, r, write) ? (int32_t)size : -1;
}
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t size) {
    return transfer(lun, lba, offset, buffer, size, false);
}
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t size) {
    return transfer(lun, lba, offset, buffer, size, true);
}
int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t cmd[16], void *buffer, uint16_t size) {
    if (!valid(lun)) return -1;
    switch (cmd[0]) {
        case 0x35: return sync_volume(lun) ? 0 : -1;
        case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
            return tud_msc_prevent_allow_medium_removal_cb(lun, cmd[4], cmd[5]) ? 0 : -1;
        case 0x1a:
        case 0x5a: {
            bool ten = cmd[0] == 0x5a;
            unsigned n = ten ? 8 : 4;
            if (!buffer || size < n || !ready(lun)) return -1;
            uint8_t *out = buffer;
            memset(out, 0, n);
            out[ten ? 1 : 0] = n - (ten ? 2 : 1);
            out[ten ? 3 : 2] = fs_volume_writable(lun) ? 0 : 0x80;
            return (int32_t)n;
        }
        default: set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0); return -1;
    }
}
void tud_msc_write10_complete_cb(uint8_t lun) { (void)valid(lun); }
void tud_msc_read10_complete_cb(uint8_t lun) { (void)valid(lun); }
void tud_msc_scsi_complete_cb(uint8_t lun, const uint8_t cmd[16]) { (void)cmd; (void)valid(lun); }
int32_t tud_msc_request_sense_cb(uint8_t lun, void *buffer, uint16_t size) {
    /* The pinned stack prefills a global sense; replace it with this LUN's. */
    bool known = valid(lun);
    if (!buffer || size < 18) return -1;
    unsigned sense = known ? atomic_exchange(&s_volumes[lun].sense, 0) : 0x052500;
    uint8_t *out = buffer;
    memset(out, 0, 18);
    out[0] = 0x70; out[2] = (sense >> 16) & 15; out[7] = 10;
    out[12] = (sense >> 8) & 255; out[13] = sense & 255;
    return 18;
}
