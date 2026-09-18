/* ESP-IDF SD transport/VFS adapter. Media is never formatted or relabelled.
 * SPI bus lifetime belongs here, not to the display device. SDMMC uses its
 * separate controller (Waveshare V2 does not wire card D3 to any MCU pin). */
#include "sd_card.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#if MCUJS_SD_SDMMC
#include "driver/sdmmc_host.h"
#else
#include "driver/sdspi_host.h"
#endif
#include "sdmmc_cmd.h"
#include "diskio_impl.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

static bool bus_ready, mounted;
static sdmmc_card_t card;
static BYTE drive = 0xff;
static FATFS *mounted_fs;
static atomic_int card_result = FS_ERROR_NO_MEDIA;
#if !MCUJS_SD_SDMMC
static sdspi_dev_handle_t card_device;
#if MCUJS_SD_SPI_BUS == 2
#define SD_SPI_HOST SPI2_HOST
#elif MCUJS_SD_SPI_BUS == 3
#define SD_SPI_HOST SPI3_HOST
#else
#error "SD SPI bus must be ESP host number 2 or 3"
#endif
#endif

static DSTATUS card_status(BYTE d) {
    return (MCUJS_SD_READONLY ? STA_PROTECT : 0) |
        ((d == drive && atomic_load(&card_result) == FS_OK) ? 0 : STA_NOINIT);
}
static DRESULT card_read(BYTE d, BYTE *buffer, DWORD sector, UINT count) {
    if (d != drive || atomic_load(&card_result) != FS_OK) return RES_NOTRDY;
    if (!buffer || !count || sector >= card.csd.capacity ||
        count > card.csd.capacity - sector) return RES_PARERR;
    while (count) {
        UINT chunk = count > 8 ? 8 : count;
        if (sdmmc_read_sectors(&card, buffer, sector, chunk) != ESP_OK) {
            atomic_store(&card_result, FS_ERROR_IO);
            return RES_ERROR;
        }
        buffer += chunk * 512;
        sector += chunk;
        count -= chunk;
    }
    return RES_OK;
}
static DRESULT card_write(BYTE d, const BYTE *buffer, DWORD sector, UINT count) {
#if MCUJS_SD_READONLY
    (void)d; (void)buffer; (void)sector; (void)count;
    return RES_WRPRT;
#else
    if (d != drive || atomic_load(&card_result) != FS_OK) return RES_NOTRDY;
    if (!buffer || !count || sector >= card.csd.capacity ||
        count > card.csd.capacity - sector) return RES_PARERR;
    while (count) {
        UINT chunk = count > 8 ? 8 : count;
        if (sdmmc_write_sectors(&card, buffer, sector, chunk) != ESP_OK) {
            atomic_store(&card_result, FS_ERROR_IO);
            return RES_ERROR;
        }
        buffer += chunk * 512;
        sector += chunk;
        count -= chunk;
    }
    return RES_OK;
#endif
}
fs_result_t sd_card_sync(void) {
    if (atomic_load(&card_result) != FS_OK) return atomic_load(&card_result);
    /* IDF sector writes wait for completion. CMD13 checks ready/status before
     * sync/eject; an error permanently fences this card until board reset. */
    if (sdmmc_get_status(&card) != ESP_OK) atomic_store(&card_result, FS_ERROR_IO);
    return atomic_load(&card_result);
}
static DRESULT card_ioctl(BYTE d, BYTE command, void *buffer) {
    if (d != drive || atomic_load(&card_result) != FS_OK) return RES_NOTRDY;
    switch (command) {
        case CTRL_SYNC: return sd_card_sync() == FS_OK ? RES_OK : RES_ERROR;
        case GET_SECTOR_COUNT:
            if (!buffer) return RES_PARERR;
            *(DWORD *)buffer = card.csd.capacity; return RES_OK;
        case GET_SECTOR_SIZE:
            if (!buffer) return RES_PARERR;
            *(WORD *)buffer = card.csd.sector_size; return RES_OK;
        case GET_BLOCK_SIZE:
            if (!buffer) return RES_PARERR;
            *(DWORD *)buffer = 1; return RES_OK;
        default: return RES_WRPRT; /* No TRIM, erase or automatic repair. */
    }
}
static const ff_diskio_impl_t card_disk = {
    .init = card_status, .status = card_status, .read = card_read,
    .write = card_write, .ioctl = card_ioctl,
};

bool sd_card_prepare(void) {
    if (bus_ready) return true;
#if MCUJS_SD_SDMMC
    /* Board V2: CLK39 CMD41 D0=40, always-on 3V3; independent of EPD SPI2. */
    bus_ready = true; /* No repeated initialization, including missing cards. */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = MCUJS_SD_BAUD_HZ / 1000;
    host.command_timeout_ms = 1000;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = MCUJS_SD_SDMMC_CLK_PIN;
    slot.cmd = MCUJS_SD_SDMMC_CMD_PIN;
    slot.d0 = MCUJS_SD_SDMMC_D0_PIN;
    slot.cd = MCUJS_SD_CARD_DETECT_PIN;
    esp_err_t error = sdmmc_host_init();
    if (error == ESP_OK) error = sdmmc_host_init_slot(host.slot, &slot);
#else
    spi_bus_config_t bus = {
        .mosi_io_num = MCUJS_SD_MOSI_PIN, .miso_io_num = MCUJS_SD_MISO_PIN,
        .sclk_io_num = MCUJS_SD_SCK_PIN,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
#ifdef ESP_PLATFORM
        .data4_io_num = -1, .data5_io_num = -1, .data6_io_num = -1, .data7_io_num = -1,
#endif
        .max_transfer_sz = 4096,
    };
    /* Never attach to/free a foreign bus or toggle its CS pins. */
    if (spi_bus_initialize(SD_SPI_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
    gpio_config_t pins = {
        .pin_bit_mask = (1ULL << MCUJS_SD_CS_PIN), .mode = GPIO_MODE_OUTPUT,
    };
    bool pins_ok = gpio_set_level(MCUJS_SD_CS_PIN, 1) == ESP_OK;
#if MCUJS_SD_SHARED_CS_PIN >= 0
    pins.pin_bit_mask |= 1ULL << MCUJS_SD_SHARED_CS_PIN;
    pins_ok = gpio_set_level(MCUJS_SD_SHARED_CS_PIN, 1) == ESP_OK && pins_ok;
#endif
#if MCUJS_SD_POWER_PIN >= 0
    pins.pin_bit_mask |= 1ULL << MCUJS_SD_POWER_PIN;
    pins_ok = gpio_set_level(MCUJS_SD_POWER_PIN, MCUJS_SD_POWER_ACTIVE_HIGH) == ESP_OK && pins_ok;
#endif
    if (!pins_ok || gpio_config(&pins) != ESP_OK) {
        (void)spi_bus_free(SD_SPI_HOST);
        return false;
    }
    /* Keep power and bus alive even on failure: an unpowered card can block
     * the shared display bus. Display close removes only its own device. */
    vTaskDelay(pdMS_TO_TICKS(100));
    bus_ready = true;
    esp_err_t error = sdspi_host_init();
    bool attached = false;
    sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
    device.host_id = SD_SPI_HOST;
    device.gpio_cs = MCUJS_SD_CS_PIN;
    device.gpio_cd = MCUJS_SD_CARD_DETECT_PIN;
    if (error == ESP_OK) {
        error = sdspi_host_init_device(&device, &card_device);
        attached = error == ESP_OK;
    }
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = card_device;
    host.max_freq_khz = MCUJS_SD_SPI_BAUD_HZ / 1000;
    host.command_timeout_ms = 1000;
#endif
    if (error == ESP_OK) error = sdmmc_card_init(&host, &card);
    if (error == ESP_OK && card.csd.sector_size != 512) error = ESP_ERR_NOT_SUPPORTED;
    if (error != ESP_OK) {
        atomic_store(&card_result, error == ESP_ERR_TIMEOUT ? FS_ERROR_NO_MEDIA :
            error == ESP_ERR_NOT_SUPPORTED ? FS_ERROR_UNSUPPORTED : FS_ERROR_IO);
#if !MCUJS_SD_SDMMC
        if (attached) (void)sdspi_host_remove_device(card_device);
        (void)gpio_set_level(MCUJS_SD_CS_PIN, 1);
#endif
    } else atomic_store(&card_result, FS_OK);
    return true;
}

fs_result_t sd_card_status(void) {
    fs_result_t result = atomic_load(&card_result);
    return result != FS_OK ? result : mounted ? FS_OK : FS_ERROR_IO;
}
fs_result_t sd_card_mount(void) {
    if (!sd_card_prepare()) return FS_ERROR_IO;
    if (atomic_load(&card_result) != FS_OK) return atomic_load(&card_result);
    if (mounted) return FS_OK;
    BYTE pdrv;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK) return FS_ERROR_IO;
    char name[6];
    snprintf(name, sizeof(name), "%u:", pdrv);
    drive = pdrv;
    ff_diskio_register(drive, &card_disk);
    esp_vfs_fat_conf_t config = {
        .base_path = SD_CARD_BASE_PATH, .fat_drive = name, .max_files = 8,
    };
    FATFS *fatfs = NULL;
    if (esp_vfs_fat_register_cfg(&config, &fatfs) != ESP_OK) {
        ff_diskio_unregister(drive);
        drive = 0xff;
        return FS_ERROR_IO;
    }
    FRESULT result = f_mount(fatfs, name, 1);
    if (result != FR_OK) {
        (void)f_mount(NULL, name, 0);
        (void)esp_vfs_fat_unregister_path(SD_CARD_BASE_PATH);
        ff_diskio_unregister(drive);
        drive = 0xff;
        return result == FR_NO_FILESYSTEM ? FS_ERROR_UNSUPPORTED :
            result == FR_NOT_READY ? FS_ERROR_NO_MEDIA : FS_ERROR_IO;
    }
    mounted_fs = fatfs;
    mounted = true;
    return FS_OK;
}

#if MCUJS_USB_SD_MSC

static fs_result_t sd_disk_result(DRESULT r) {
    switch(r) {
        case RES_OK: return FS_OK;
        case RES_WRPRT: return FS_ERROR_READ_ONLY;
        case RES_PARERR: return FS_ERROR_INVALID;
        case RES_NOTRDY: return atomic_load(&card_result) != FS_OK ? atomic_load(&card_result) : FS_ERROR_NO_MEDIA;
        default: return FS_ERROR_IO;
    }
}
static uint16_t sd_le16(const BYTE *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t sd_le32(const BYTE *p) {
    return (uint32_t)sd_le16(p) | (uint32_t)sd_le16(p + 2) << 16;
}

/* Match pinned IDF 5.3.2 check_fs recognition, including legacy FAT12/16 without a
 * signature/type string. Recognition and geometry are separate: a header
 * native FatFs recognizes must not be skipped in favour of another volume. */
static bool sd_fat_header(const BYTE *b) {
    if (b[0] != 0xeb && b[0] != 0xe9 && b[0] != 0xe8) return false;
    if (sd_le16(b + 510) == 0xaa55 && !memcmp(b + 82, "FAT32   ", 8)) return true;
    uint32_t cluster = b[13];
    return sd_le16(b + 11) == FS_SECTOR_SIZE && cluster && !(cluster & (cluster - 1)) &&
        sd_le16(b + 14) && (b[16] == 1 || b[16] == 2) && sd_le16(b + 17) &&
        (sd_le16(b + 19) >= 128 || sd_le32(b + 32) >= 0x10000) && sd_le16(b + 22);
}

/* Validate the BPB before publishing any USB capacity. Do not infer the end
 * from cluster count: FAT volumes may legitimately include trailing padding. */
static bool sd_fat_sectors(const BYTE *b, uint32_t limit, uint32_t *sectors) {
    if (!sd_fat_header(b)) return false;
    uint32_t total = sd_le16(b + 19), fat = sd_le16(b + 22);
    uint32_t reserved = sd_le16(b + 14), roots = sd_le16(b + 17);
    uint32_t cluster = b[13], fats = b[16];
    if (total && sd_le32(b + 32)) return false;
    if (!total) total = sd_le32(b + 32);
    if (!fat) fat = sd_le32(b + 36);
    if (sd_le16(b + 11) != FS_SECTOR_SIZE || !cluster ||
        (cluster & (cluster - 1)) || !reserved || (fats != 1 && fats != 2) ||
        !fat || roots % 16 || total < 128 || total > limit) return false;
    uint64_t overhead = reserved + (uint64_t)fats * fat + roots / 16;
    if (overhead >= total) return false;
    uint32_t clusters = (total - overhead) / cluster;
    if (!clusters || clusters > 0x0ffffff5) return false;
    /* Match IDF FatFs's FAT subtype thresholds. */
    uint32_t bits = clusters <= 0xff5 ? 12 : clusters <= 0xfff5 ? 16 : 32;
    if (((uint64_t)clusters + 2) * bits > (uint64_t)fat * FS_SECTOR_SIZE * 8)
        return false;
    if (bits == 32) {
        uint32_t root = sd_le32(b + 44);
        uint16_t info = sd_le16(b + 48), backup = sd_le16(b + 50);
        if (sd_le16(b + 510) != 0xaa55 || roots || sd_le16(b + 22) ||
            sd_le16(b + 42) || root < 2 || root >= clusters + 2 ||
            (info != 0xffff && info >= reserved) ||
            (backup != 0xffff && backup >= reserved)) return false;
    } else if (!roots || !sd_le16(b + 22)) {
        return false;
    }
    *sectors = total;
    return true;
}

/* Only MBR/VBR headers are needed. In particular, never probe CSD count - 1.
 * Check partition extents BEFORE reading a VBR or asking FatFs to mount it. */
static fs_result_t sd_usb_geometry(uint32_t *start, uint32_t *sectors) {
    uint32_t count = card.csd.capacity;
    fs_result_t result;
    if (count < 128) return FS_ERROR_UNSUPPORTED;
#if FF_LBA64
    if (count > UINT32_MAX) return FS_ERROR_UNSUPPORTED;
#endif
    BYTE boot[FS_SECTOR_SIZE];
    result = sd_disk_result(card_read(drive, boot, 0, 1));
    if (result != FS_OK) return result;
    *start = 0;
    if (sd_fat_header(boot))
        return sd_fat_sectors(boot, (uint32_t)count, sectors) ? FS_OK : FS_ERROR_UNSUPPORTED;
    if (sd_le16(boot + 510) != 0xaa55) return FS_ERROR_UNSUPPORTED;
    BYTE table[64];
    memcpy(table, boot + 446, sizeof(table));
#if FF_LBA64
    if (table[4] == 0xee) return FS_ERROR_UNSUPPORTED; /* No unchecked GPT scan. */
#endif
    /* FatFs scans by start LBA, regardless of type. Validate every possible
     * location before selecting a VBR, not only entries before the first FAT. */
    for (unsigned i = 0; i < 4; i++) {
        const BYTE *entry = table + i * 16;
        uint32_t first = sd_le32(entry + 8), length = sd_le32(entry + 12);
        if (!first && !entry[4] && !length) continue;
        if (!first || first >= count || length < 128 || length > count - first)
            return FS_ERROR_UNSUPPORTED;
    }
    for (unsigned i = 0; i < 4; i++) {
        const BYTE *entry = table + i * 16;
        uint32_t first = sd_le32(entry + 8), length = sd_le32(entry + 12);
        if (!first) continue; /* Extent was validated above. */
        result = sd_disk_result(card_read(drive, boot, first, 1));
        if (result != FS_OK) return result;
        if (sd_fat_header(boot)) {
            if (!sd_fat_sectors(boot, length, sectors)) return FS_ERROR_UNSUPPORTED;
            *start = first;
            return FS_OK;
        }
        /* A malformed FAT partition must not turn into a raw-card export or
         * silently select a different FAT volume. Skip only non-FAT entries. */
        if (entry[4] == 0x01 || entry[4] == 0x04 || entry[4] == 0x06 ||
            entry[4] == 0x0b || entry[4] == 0x0c || entry[4] == 0x0e)
            return FS_ERROR_UNSUPPORTED;
    }
    return FS_ERROR_UNSUPPORTED;
}
fs_result_t sd_card_media_status(void) { return atomic_load(&card_result); }

static fs_result_t unmount_card(void) {
    char name[6];
    snprintf(name, sizeof(name), "%u:", drive);
    if (f_mount(NULL, name, 0) != FR_OK) return FS_ERROR_IO;
    /* A failed VFS detach cannot safely grant the host ownership. */
    if (esp_vfs_fat_unregister_path(SD_CARD_BASE_PATH) != ESP_OK) {
        atomic_store(&card_result, FS_ERROR_IO);
        return FS_ERROR_IO;
    }
    mounted = false;
    mounted_fs = NULL;
    ff_diskio_unregister(drive);
    drive = 0xff;
    return FS_OK;
}

fs_result_t sd_card_export(uint32_t *start, uint32_t *sectors) {
    if (!start || !sectors) return FS_ERROR_INVALID;
    *start = *sectors = 0;
    if (!sd_card_prepare()) return FS_ERROR_IO;
    if (atomic_load(&card_result) != FS_OK) return atomic_load(&card_result);
    uint32_t first, count;
    fs_result_t r = sd_usb_geometry(&first, &count);
    if (r != FS_OK) return r;
    r = sd_card_mount();
    if (r != FS_OK) return r;
    if (mounted_fs->volbase != first) return FS_ERROR_UNSUPPORTED;
    r = sd_card_sync();
    if (r != FS_OK) return r;
    r = unmount_card();
    if (r == FS_OK) { *start = first; *sectors = count; }
    return r;
}

fs_result_t sd_card_import(uint32_t start, uint32_t sectors) {
    fs_result_t r = sd_card_sync();
    uint32_t first, count;
    if (r == FS_OK) r = sd_usb_geometry(&first, &count);
    if (r == FS_OK && (first != start || count > sectors)) r = FS_ERROR_UNSUPPORTED;
    if (r == FS_OK) r = sd_card_mount();
    if (r == FS_OK && mounted_fs->volbase != start) r = FS_ERROR_UNSUPPORTED;
    if (r != FS_OK) atomic_store(&card_result, r); /* No stale native reuse. */
    return r;
}

fs_result_t sd_card_transfer(uint32_t start, uint32_t sectors, uint32_t sector,
    uint32_t offset, void *buffer, uint32_t size, bool write) {
    if (!buffer || offset >= 512 || size > 512 - offset || sector >= sectors ||
        start >= card.csd.capacity || sectors > card.csd.capacity - start)
        return FS_ERROR_INVALID;
    if (atomic_load(&card_result) != FS_OK) return atomic_load(&card_result);
    if (write && MCUJS_SD_READONLY) return FS_ERROR_READ_ONLY;
    if (!size) return FS_OK;
    BYTE scratch[512];
    sector += start;
    if (!write || offset || size != 512) {
        fs_result_t r = sd_disk_result(card_read(drive, scratch, sector, 1));
        if (r != FS_OK) return r;
    }
    if (!write) { memcpy(buffer, scratch + offset, size); return FS_OK; }
    if (!offset && size == 512) return sd_disk_result(card_write(drive, buffer, sector, 1));
    memcpy(scratch + offset, buffer, size);
    return sd_disk_result(card_write(drive, scratch, sector, 1));
}

#endif
