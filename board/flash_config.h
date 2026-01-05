/*
 * mcujs - Flash Configuration
 * 
 * Common flash size definitions for all boards
 */

#ifndef MCUJS_FLASH_CONFIG_H
#define MCUJS_FLASH_CONFIG_H

/* Flash sizes in bytes */
#define FLASH_SIZE_2MB   (2 * 1024 * 1024)
#define FLASH_SIZE_4MB   (4 * 1024 * 1024)
#define FLASH_SIZE_8MB   (8 * 1024 * 1024)
#define FLASH_SIZE_16MB  (16 * 1024 * 1024)

/* Flash sector size (RP2040/RP2350 use 4KB sectors) */
#define FLASH_SECTOR_SIZE  4096

/* Flash page size for programming */
#define FLASH_PAGE_SIZE    256

/* 
 * Reserved space at end of flash for EEPROM emulation
 * This is a common pattern for storing small amounts of persistent data
 */
#define EEPROM_RESERVED_SIZE  (4 * 1024)

/*
 * Minimum filesystem size
 * Ensures there's enough space for basic operations
 */
#define MIN_FILESYSTEM_SIZE  (64 * 1024)

/*
 * FAT12 filesystem parameters
 * FAT12 is ideal for small flash sizes (<32MB)
 */
#define FAT_SECTOR_SIZE      512
#define FAT_SECTORS_PER_CLUSTER  1

/*
 * Calculate filesystem size based on flash size and program size
 * FS_SIZE = FLASH_SIZE - PROGRAM_SIZE - EEPROM_SIZE
 * 
 * The actual calculation is done at link time using linker symbols:
 * - __flash_binary_end: End of program code
 * - _FS_start: Start of filesystem (aligned to sector boundary)
 * - _FS_end: End of filesystem (flash end - EEPROM)
 */

#endif /* MCUJS_FLASH_CONFIG_H */
