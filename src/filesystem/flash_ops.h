/*
 * mcujs - Flash Operations
 * 
 * Low-level flash read/write/erase operations
 */

#ifndef MCUJS_FLASH_OPS_H
#define MCUJS_FLASH_OPS_H

#include <stdint.h>
#include <stddef.h>

/*
 * Read from flash (XIP)
 * Flash is memory-mapped, so this is just a memcpy
 */
void flash_ops_read(uint32_t addr, void *buffer, size_t size);

/*
 * Write to flash
 * Address must be erased first
 * Size must be multiple of FLASH_PAGE_SIZE (256)
 */
void flash_ops_write(uint32_t addr, const void *buffer, size_t size);

/*
 * Erase flash sector
 * Address must be aligned to FLASH_SECTOR_SIZE (4096)
 * Size must be multiple of FLASH_SECTOR_SIZE
 */
void flash_ops_erase(uint32_t addr, size_t size);

#endif /* MCUJS_FLASH_OPS_H */
