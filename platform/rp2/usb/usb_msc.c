/*
 * mcujs - USB MSC Implementation
 * 
 * Mass Storage callbacks using TinyUSB
 * Connects to the FAT filesystem on flash
 */

#include "usb_msc.h"
#include "tusb.h"
#include "../filesystem/fs.h"

#include <string.h>

/* MSC state */
static bool s_ejected = false;

/*
 * Media change state machine for proper host notification.
 * 
 * Linux (and other OSes) require a specific sequence of SCSI sense codes
 * to properly recognize that media has changed and re-read the filesystem.
 * Each state returns a different error code that must be "consumed" by the
 * host before transitioning to the next state.
 * 
 * State sequence:
 *   IDLE -> (media_changed called) -> NO_MEDIA -> RESET -> CHANGED -> IDLE
 * 
 * References:
 *   - TinyUSB discussion #1299
 *   - SCSI Primary Commands (SPC) specification
 */
typedef enum {
    MEDIA_STATE_IDLE = 0,      /* Normal operation, no change pending */
    MEDIA_STATE_NO_MEDIA,      /* Report: Medium not present (SK=02, ASC=3A) */
    MEDIA_STATE_RESET,         /* Report: Device reset occurred (SK=06, ASC=29) */
    MEDIA_STATE_CHANGED,       /* Report: Medium changed (SK=06, ASC=28) */
} media_change_state_t;

static media_change_state_t s_media_state = MEDIA_STATE_IDLE;

void usb_msc_init(void) {
    s_ejected = false;
    s_media_state = MEDIA_STATE_IDLE;
}

bool usb_msc_ejected(void) {
    return s_ejected;
}

void usb_msc_reset_ejected(void) {
    s_ejected = false;
}

void usb_msc_media_changed(void) {
    /*
     * Start the media change state machine.
     * 
     * Note: Linux caches FAT directory data aggressively and doesn't
     * poll TEST_UNIT_READY frequently on mounted filesystems. The SCSI
     * sense codes we report are technically correct but Linux won't
     * re-read the directory until remount.
     * 
     * Alternatives considered:
     * - USB soft disconnect (tud_disconnect/tud_connect): Works but
     *   disconnects the entire composite device including CDC serial,
     *   which breaks the REPL connection.
     * 
     * Current workaround: Users must remount or replug to see files
     * written from the JavaScript runtime.
     */
    if (s_media_state == MEDIA_STATE_IDLE) {
        s_media_state = MEDIA_STATE_NO_MEDIA;
    }
}

#if CFG_TUD_MSC > 0

/*--------------------------------------------------------------------
 * TinyUSB MSC Callbacks
 *--------------------------------------------------------------------*/

/* Invoked when received SCSI_CMD_INQUIRY */
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    
    const char vid[] = "mcujs";
    const char pid[] = "Flash Storage";
    const char rev[] = "1.0";
    
    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

/* Invoked when received Test Unit Ready command */
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    
    if (s_ejected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }
    
    /*
     * Media change state machine - report different sense codes in sequence.
     * Each TEST_UNIT_READY from the host consumes one state transition.
     * This sequence convinces Linux to re-read the FAT filesystem.
     */
    switch (s_media_state) {
        case MEDIA_STATE_NO_MEDIA:
            /* Step 1: Report medium not present */
            /* SK=NOT_READY(02h), ASC=MEDIUM NOT PRESENT(3Ah), ASCQ=00h */
            tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
            s_media_state = MEDIA_STATE_RESET;
            return false;
            
        case MEDIA_STATE_RESET:
            /* Step 2: Report device reset occurred */
            /* SK=UNIT_ATTENTION(06h), ASC=POWER ON/RESET(29h), ASCQ=00h */
            tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x29, 0x00);
            s_media_state = MEDIA_STATE_CHANGED;
            return false;
            
        case MEDIA_STATE_CHANGED:
            /* Step 3: Report medium changed */
            /* SK=UNIT_ATTENTION(06h), ASC=NOT READY TO READY CHANGE(28h), ASCQ=00h */
            tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00);
            s_media_state = MEDIA_STATE_IDLE;
            return false;
            
        case MEDIA_STATE_IDLE:
        default:
            /* Normal operation - unit is ready */
            return true;
    }
}

/* Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY */
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void)lun;
    
    *block_size = FS_SECTOR_SIZE;
    *block_count = fs_get_total_sectors();
}

/* Invoked when received Start Stop Unit command */
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                            bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;
    
    if (load_eject) {
        if (start) {
            /* Load - re-mount */
            s_ejected = false;
        } else {
            /* Eject */
            fs_sync();
            s_ejected = true;
        }
    }
    
    return true;
}

/* Invoked when received READ10 command */
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           void *buffer, uint32_t bufsize) {
    (void)lun;
    
    if (s_ejected) {
        return -1;
    }
    
    /* Read from flash */
    if (fs_read_sector(lba, offset, buffer, bufsize) != FS_OK) {
        return -1;
    }
    
    return (int32_t)bufsize;
}

/* Invoked when received WRITE10 command */
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                            uint8_t *buffer, uint32_t bufsize) {
    (void)lun;
    
    if (s_ejected) {
        return -1;
    }
    
    /* Write to flash */
    if (fs_write_sector(lba, offset, buffer, bufsize) != FS_OK) {
        return -1;
    }
    
    return (int32_t)bufsize;
}

/* SCSI command codes */
#define SCSI_CMD_MODE_SENSE_6  0x1A
#define SCSI_CMD_MODE_SENSE_10 0x5A

/* Invoked when received SCSI command not handled by TinyUSB */
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                         void *buffer, uint16_t bufsize) {
    uint8_t cmd = scsi_cmd[0];
    
    /* Handle MODE_SENSE commands - report device as writable */
    if (cmd == SCSI_CMD_MODE_SENSE_6) {
        uint8_t *resp = (uint8_t *)buffer;
        memset(resp, 0, 4);
        resp[0] = 3;    /* Mode data length */
        resp[1] = 0;    /* Medium type */
        resp[2] = 0;    /* Device-specific parameter (0 = not write protected) */
        resp[3] = 0;    /* Block descriptor length */
        return 4;
    }
    
    if (cmd == SCSI_CMD_MODE_SENSE_10) {
        uint8_t *resp = (uint8_t *)buffer;
        memset(resp, 0, 8);
        resp[0] = 0;    /* Mode data length (MSB) */
        resp[1] = 6;    /* Mode data length (LSB) */
        resp[2] = 0;    /* Medium type */
        resp[3] = 0;    /* Device-specific parameter (0 = not write protected) */
        resp[4] = 0;    /* Reserved */
        resp[5] = 0;    /* Reserved */
        resp[6] = 0;    /* Block descriptor length (MSB) */
        resp[7] = 0;    /* Block descriptor length (LSB) */
        return 8;
    }
    
    /* Unsupported command */
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

/* Invoked when Write10 completes */
void tud_msc_write10_complete_cb(uint8_t lun) {
    (void)lun;
    /* Sync to flash after each write sequence */
    fs_sync();
}

/* Check if write is allowed */
bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return !s_ejected;
}

#endif /* CFG_TUD_MSC > 0 */
