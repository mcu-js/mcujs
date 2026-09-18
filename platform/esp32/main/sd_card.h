#ifndef MCUJS_ESP_SD_CARD_H
#define MCUJS_ESP_SD_CARD_H
#include "fs.h"
#define SD_CARD_BASE_PATH "/mcujs-sd"
/* SPI boards: call before display add-device. Bus/power live until reset,
 * including absent/faulted cards. A display must remove only its own device.
 * SDMMC boards use their independent controller. No hot-swap/reinit. */
bool sd_card_prepare(void);
fs_result_t sd_card_mount(void);
fs_result_t sd_card_status(void);
fs_result_t sd_card_media_status(void);
fs_result_t sd_card_sync(void);
/* Caller owns the per-volume lease and excludes all files/directories/I/O
 * before export/import. Export validates the FAT extent before native mount;
 * import must not expand/retarget the released lease. Neither formats media. */
fs_result_t sd_card_export(uint32_t *start, uint32_t *sectors);
fs_result_t sd_card_import(uint32_t start, uint32_t sectors);
fs_result_t sd_card_transfer(uint32_t start, uint32_t sectors, uint32_t sector,
    uint32_t offset, void *buffer, uint32_t size, bool write);
#endif
