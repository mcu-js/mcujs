/*
 * mcujs - FatFs Disk I/O Driver
 * 
 * Flash storage driver for ChaN's FatFs library
 * Implements disk_* functions required by FatFs
 * 
 * Flash requires erase-before-write (4KB blocks) but FAT sectors are 512 bytes.
 * Uses a sector cache with read-modify-write for flash erase blocks.
 */

#include "ff.h"
#include "diskio.h"
#include "flash_config.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Flash configuration */
#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE 4096  /* RP2040 flash erase block size */
#endif

#define SECTOR_SIZE 512  /* FAT sector size */

/* Filesystem flash layout
 * - Filesystem starts after firmware end (aligned to erase boundary)
 * - Ends at flash end minus EEPROM reservation
 */
extern char __flash_binary_end[];

/* Calculated filesystem parameters */
static uint32_t s_fs_start_addr = 0;
static uint32_t s_fs_size = 0;
static uint32_t s_total_sectors;
static DSTATUS s_disk_status = STA_NOINIT;

static uint32_t align_up(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static void diskio_refresh_geometry(void) {
    uint32_t flash_end = XIP_BASE + PICO_FLASH_SIZE_BYTES;
    uint32_t fs_end = flash_end - EEPROM_RESERVED_SIZE;
    uint32_t fs_start = align_up((uint32_t)(uintptr_t)__flash_binary_end, FLASH_SECTOR_SIZE);

    s_fs_start_addr = fs_start;

    if (fs_end <= fs_start || (fs_end - fs_start) < MIN_FILESYSTEM_SIZE) {
        s_fs_size = 0;
        s_total_sectors = 0;
        return;
    }

    s_fs_size = fs_end - fs_start;
    s_total_sectors = s_fs_size / SECTOR_SIZE;
}

/* Sector cache for read-modify-write operations */
#define CACHE_SECTORS 4
static uint8_t s_sector_cache[CACHE_SECTORS][SECTOR_SIZE];
static uint32_t s_cached_sector[CACHE_SECTORS];
static bool s_cache_dirty[CACHE_SECTORS];
static uint8_t s_cache_lru[CACHE_SECTORS];

/* 4KB buffer for flash erase block read-modify-write */
static uint8_t s_erase_buffer[FLASH_SECTOR_SIZE];

/*---------------------------------------------------------------------------/
/ Cache Management
/---------------------------------------------------------------------------*/

/*
 * Initialize cache
 */
static void cache_init(void) {
    for (int i = 0; i < CACHE_SECTORS; i++) {
        s_cached_sector[i] = 0xFFFFFFFF;
        s_cache_dirty[i] = false;
        s_cache_lru[i] = i;
    }
}

/*
 * Find sector in cache, returns slot index or -1 if not found
 */
static int cache_find(uint32_t sector) {
    for (int i = 0; i < CACHE_SECTORS; i++) {
        if (s_cached_sector[i] == sector) {
            return i;
        }
    }
    return -1;
}

/*
 * Get LRU (least recently used) cache slot
 */
static int cache_get_lru(void) {
    return s_cache_lru[CACHE_SECTORS - 1];
}

/*
 * Update LRU on cache access - move slot to front
 */
static void cache_touch(int slot) {
    int pos = 0;
    for (int i = 0; i < CACHE_SECTORS; i++) {
        if (s_cache_lru[i] == slot) {
            pos = i;
            break;
        }
    }
    
    /* Move to front (most recently used) */
    for (int i = pos; i > 0; i--) {
        s_cache_lru[i] = s_cache_lru[i - 1];
    }
    s_cache_lru[0] = slot;
}

/*
 * Flush a cache slot to flash using read-modify-write
 */
static DRESULT cache_flush_slot(int slot) {
    if (!s_cache_dirty[slot]) {
        return RES_OK;
    }
    
    uint32_t sector = s_cached_sector[slot];
    uint32_t flash_offset = (s_fs_start_addr - XIP_BASE) + (sector * SECTOR_SIZE);
    
    /* Flash erase is 4KB aligned - calculate erase block boundary */
    uint32_t erase_addr = flash_offset & ~(FLASH_SECTOR_SIZE - 1);
    uint32_t sector_within_erase = (flash_offset - erase_addr) / SECTOR_SIZE;
    
    /* Read-modify-write: read the entire 4KB erase block */
    memcpy(s_erase_buffer, (void *)(XIP_BASE + erase_addr), FLASH_SECTOR_SIZE);
    
    /* Copy new sector data into buffer */
    memcpy(s_erase_buffer + (sector_within_erase * SECTOR_SIZE), 
           s_sector_cache[slot], SECTOR_SIZE);
    
    /* Disable interrupts during flash operations */
    uint32_t ints = save_and_disable_interrupts();
    
    /* Erase 4KB block and write back */
    flash_range_erase(erase_addr, FLASH_SECTOR_SIZE);
    flash_range_program(erase_addr, s_erase_buffer, FLASH_SECTOR_SIZE);
    
    restore_interrupts(ints);
    
    s_cache_dirty[slot] = false;
    return RES_OK;
}

/*
 * Flush all dirty cache entries
 */
static DRESULT cache_flush_all(void) {
    for (int i = 0; i < CACHE_SECTORS; i++) {
        if (s_cache_dirty[i]) {
            DRESULT res = cache_flush_slot(i);
            if (res != RES_OK) {
                return res;
            }
        }
    }
    return RES_OK;
}

/*
 * Load sector into cache, returns pointer to cached sector
 */
static uint8_t *cache_get_sector(uint32_t sector, bool for_write) {
    int slot = cache_find(sector);
    
    if (slot >= 0) {
        /* Cache hit */
        cache_touch(slot);
        if (for_write) {
            s_cache_dirty[slot] = true;
        }
        return s_sector_cache[slot];
    }
    
    /* Cache miss - need to load sector */
    slot = cache_get_lru();
    
    /* Flush if dirty before reusing slot */
    if (s_cache_dirty[slot]) {
        cache_flush_slot(slot);
    }
    
    /* Read from flash (XIP memory-mapped) */
    uint32_t flash_addr = s_fs_start_addr + (sector * SECTOR_SIZE);
    memcpy(s_sector_cache[slot], (void *)flash_addr, SECTOR_SIZE);
    
    s_cached_sector[slot] = sector;
    s_cache_dirty[slot] = for_write;
    cache_touch(slot);
    
    return s_sector_cache[slot];
}

/*---------------------------------------------------------------------------/
/ FatFs Disk I/O Functions
/---------------------------------------------------------------------------*/

/*
 * Get disk status
 */
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return s_disk_status;
}

/*
 * Initialize disk
 */
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    
    /* Calculate filesystem size and sector count */
    diskio_refresh_geometry();

    if (s_fs_size == 0) {
        s_disk_status = STA_NOINIT;
        return s_disk_status;
    }
    
    /* Initialize sector cache */
    cache_init();
    
    s_disk_status = 0;  /* Clear STA_NOINIT flag */
    return s_disk_status;
}

/*
 * Read sectors
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) {
        return RES_PARERR;
    }
    if (s_disk_status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    if (sector + count > s_total_sectors) {
        return RES_PARERR;
    }
    
    for (UINT i = 0; i < count; i++) {
        uint8_t *cached = cache_get_sector(sector + i, false);
        memcpy(buff + (i * SECTOR_SIZE), cached, SECTOR_SIZE);
    }
    
    return RES_OK;
}

/*
 * Write sectors
 */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) {
        return RES_PARERR;
    }
    if (s_disk_status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    if (s_disk_status & STA_PROTECT) {
        return RES_WRPRT;
    }
    if (sector + count > s_total_sectors) {
        return RES_PARERR;
    }
    
    for (UINT i = 0; i < count; i++) {
        uint8_t *cached = cache_get_sector(sector + i, true);
        memcpy(cached, buff + (i * SECTOR_SIZE), SECTOR_SIZE);
    }
    
    return RES_OK;
}

/*
 * Disk I/O control
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0) {
        return RES_PARERR;
    }
    if (s_disk_status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    
    switch (cmd) {
        case CTRL_SYNC:
            /* Flush all cached sectors to flash */
            return cache_flush_all();
            
        case GET_SECTOR_COUNT:
            /* Return total number of sectors */
            *(LBA_t *)buff = s_total_sectors;
            return RES_OK;
            
        case GET_SECTOR_SIZE:
            /* Return sector size (always 512) */
            *(WORD *)buff = SECTOR_SIZE;
            return RES_OK;
            
        case GET_BLOCK_SIZE:
            /* Return erase block size in sectors (4KB / 512 = 8) */
            *(DWORD *)buff = FLASH_SECTOR_SIZE / SECTOR_SIZE;
            return RES_OK;
            
        default:
            return RES_PARERR;
    }
}

/*---------------------------------------------------------------------------/
/ Direct Sector Access for USB MSC
/ These bypass FatFs cache and go directly to flash
/---------------------------------------------------------------------------*/

/*
 * Get total sector count (for USB MSC)
 */
uint32_t diskio_get_sector_count(void) {
    if (s_disk_status & STA_NOINIT) {
        /* Initialize if needed */
        diskio_refresh_geometry();
    }
    if (s_fs_size == 0) {
        return 0;
    }
    return s_total_sectors;
}

/*
 * Read sector directly (for USB MSC)
 * Reads from flash, not from FatFs cache
 */
int diskio_read_sector(uint32_t sector, uint32_t offset, void *buffer, uint32_t size) {
    if (sector >= s_total_sectors) {
        return -1;
    }
    if (offset + size > SECTOR_SIZE) {
        return -1;
    }
    
    /* Read directly from flash (XIP memory-mapped) */
    uint32_t flash_addr = s_fs_start_addr + (sector * SECTOR_SIZE) + offset;
    memcpy(buffer, (void *)flash_addr, size);
    
    return 0;
}

/*
 * Write sector directly (for USB MSC)
 * Writes directly to flash, invalidates FatFs cache for this sector
 */
int diskio_write_sector(uint32_t sector, uint32_t offset, const void *buffer, uint32_t size) {
    if (sector >= s_total_sectors) {
        return -1;
    }
    if (offset + size > SECTOR_SIZE) {
        return -1;
    }
    
    /* Invalidate this sector in cache if present (USB MSC is modifying it) */
    int slot = cache_find(sector);
    if (slot >= 0) {
        s_cached_sector[slot] = 0xFFFFFFFF;
        s_cache_dirty[slot] = false;
    }
    
    /* Calculate flash addresses */
    uint32_t flash_offset = (s_fs_start_addr - XIP_BASE) + (sector * SECTOR_SIZE);
    uint32_t erase_addr = flash_offset & ~(FLASH_SECTOR_SIZE - 1);
    uint32_t byte_offset_in_erase = flash_offset - erase_addr + offset;
    
    /* Read-modify-write: read entire 4KB erase block */
    memcpy(s_erase_buffer, (void *)(XIP_BASE + erase_addr), FLASH_SECTOR_SIZE);
    
    /* Modify the specific bytes */
    memcpy(s_erase_buffer + byte_offset_in_erase, buffer, size);
    
    /* Write back */
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(erase_addr, FLASH_SECTOR_SIZE);
    flash_range_program(erase_addr, s_erase_buffer, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    
    return 0;
}

/*
 * Sync direct writes (for USB MSC)
 * Forces any pending writes to complete
 */
void diskio_sync(void) {
    /* Direct writes are synchronous, but flush FatFs cache too */
    cache_flush_all();
}

/*---------------------------------------------------------------------------/
/ FatFs RTC Support
/---------------------------------------------------------------------------*/

/*
 * Get current time for FAT timestamps
 * 
 * Returns packed FAT timestamp:
 * bit[31:25] Year from 1980 (0..127)
 * bit[24:21] Month (1..12)
 * bit[20:16] Day (1..31)
 * bit[15:11] Hour (0..23)
 * bit[10:5]  Minute (0..59)
 * bit[4:0]   Second/2 (0..29)
 * 
 * Since Pico has no RTC, we return a fixed timestamp: 2024-01-01 00:00:00
 */
DWORD get_fattime(void) {
    return ((DWORD)(2024 - 1980) << 25)  /* Year = 2024 */
         | ((DWORD)1 << 21)              /* Month = 1 (January) */
         | ((DWORD)1 << 16)              /* Day = 1 */
         | ((DWORD)0 << 11)              /* Hour = 0 */
         | ((DWORD)0 << 5)               /* Minute = 0 */
         | ((DWORD)0 >> 1);              /* Second = 0 */
}
