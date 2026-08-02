#ifndef MCUJS_TEST_TUSB_H
#define MCUJS_TEST_TUSB_H

#include <stdbool.h>
#include <stdint.h>

#define CFG_TUD_MSC 1
#define SCSI_SENSE_NOT_READY 0x02
#define SCSI_SENSE_MEDIUM_ERROR 0x03
#define SCSI_SENSE_ILLEGAL_REQUEST 0x05
#define SCSI_SENSE_UNIT_ATTENTION 0x06
#define SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL 0x1e

void tud_msc_set_sense(uint8_t lun, uint8_t sense_key, uint8_t add_sense_code,
                       uint8_t add_sense_qualifier);

#endif /* MCUJS_TEST_TUSB_H */
