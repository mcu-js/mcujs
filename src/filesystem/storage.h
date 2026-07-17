/*
 * mcujs - Platform storage interface
 *
 * Direct sector operations used by the shared FatFs wrapper and USB MSC
 * transport. Platform backends provide these functions.
 */

#ifndef MCUJS_STORAGE_H
#define MCUJS_STORAGE_H

#include <stdint.h>

uint32_t diskio_get_sector_count(void);
int diskio_read_sector(uint32_t sector, uint32_t offset,
                       void *buffer, uint32_t size);
int diskio_write_sector(uint32_t sector, uint32_t offset,
                        const void *buffer, uint32_t size);
void diskio_sync(void);

#endif /* MCUJS_STORAGE_H */
