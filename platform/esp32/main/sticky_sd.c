/* Sticky's actual shared SD/display bus; no public SPI or SD-specific JS API. */
#include "sticky_sd.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "diskio_impl.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static bool bus_ready, mounted;
static sdmmc_card_t card;
static sdspi_dev_handle_t card_device;
static BYTE drive = 0xff;
static fs_result_t card_result = FS_ERROR_NO_MEDIA;

static DSTATUS ro_status(BYTE d) {
    return STA_PROTECT | ((d == drive && card_result == FS_OK) ? 0 : STA_NOINIT);
}
static DRESULT ro_read(BYTE d, BYTE *buffer, DWORD sector, UINT count) {
    if (d != drive || card_result != FS_OK) return RES_NOTRDY;
    if (!buffer || !count || sector >= card.csd.capacity ||
        count > card.csd.capacity - sector) return RES_PARERR;
    /* Match the qualified bus transfer ceiling, including larger FatFs reads. */
    while (count) {
        UINT chunk = count > 8 ? 8 : count;
        if (sdmmc_read_sectors(&card, buffer, sector, chunk) != ESP_OK) {
            card_result = FS_ERROR_IO; /* Fail closed until reset, including cached handles. */
            return RES_ERROR;
        }
        buffer += chunk * 512;
        sector += chunk;
        count -= chunk;
    }
    return RES_OK;
}
static DRESULT ro_write(BYTE d, const BYTE *buffer, DWORD sector, UINT count) {
    (void)d; (void)buffer; (void)sector; (void)count;
    return RES_WRPRT;
}
static DRESULT ro_ioctl(BYTE d, BYTE command, void *buffer) {
    if (d != drive || card_result != FS_OK) return RES_NOTRDY;
    switch (command) {
        case CTRL_SYNC: return RES_OK; /* There can be no pending writes. */
        case GET_SECTOR_COUNT:
            if (!buffer) return RES_PARERR;
            *(DWORD *)buffer = card.csd.capacity; return RES_OK;
        case GET_SECTOR_SIZE:
            if (!buffer) return RES_PARERR;
            *(WORD *)buffer = card.csd.sector_size; return RES_OK;
        case GET_BLOCK_SIZE:
            if (!buffer) return RES_PARERR;
            *(DWORD *)buffer = 1; return RES_OK;
        default: return RES_WRPRT; /* TRIM, erase and unknown controls denied. */
    }
}
static const ff_diskio_impl_t readonly_disk = {
    .init = ro_status, .status = ro_status, .read = ro_read,
    .write = ro_write, .ioctl = ro_ioctl,
};

bool sticky_sd_prepare(void) {
    if (bus_ready) return true;
    spi_bus_config_t bus = {
        .mosi_io_num = 14, .miso_io_num = 12, .sclk_io_num = 13,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
#ifdef ESP_PLATFORM
        .data4_io_num = -1, .data5_io_num = -1, .data6_io_num = -1, .data7_io_num = -1,
#endif
        .max_transfer_sz = 4096,
    };
    /* Never attach to or free a foreign owner's bus, nor toggle its CS pins. */
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
    gpio_config_t pins = {
        .pin_bit_mask = (1ULL << 8) | (1ULL << 10) | (1ULL << 15),
        .mode = GPIO_MODE_OUTPUT,
    };
    if (gpio_set_level(8, 1) != ESP_OK || gpio_set_level(15, 1) != ESP_OK ||
        gpio_set_level(10, 1) != ESP_OK || gpio_config(&pins) != ESP_OK) {
        (void)spi_bus_free(SPI2_HOST);
        return false;
    }
    /* EN10 stays high, even absent a mount: an unpowered inserted card can
     * block display traffic. No power cycle or reinitialization after display I/O. */
    vTaskDelay(pdMS_TO_TICKS(100));
    bus_ready = true;
    esp_err_t error = sdspi_host_init();
    bool attached = false;
    sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
    device.host_id = SPI2_HOST;
    device.gpio_cs = 8;
    if (error == ESP_OK) {
        error = sdspi_host_init_device(&device, &card_device);
        attached = error == ESP_OK;
    }
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = card_device;
    host.max_freq_khz = 4000;
    host.command_timeout_ms = 1000;
    if (error == ESP_OK) error = sdmmc_card_init(&host, &card);
    if (error == ESP_OK && card.csd.sector_size != 512) error = ESP_ERR_NOT_SUPPORTED;
    if (error != ESP_OK) {
        card_result = error == ESP_ERR_TIMEOUT ? FS_ERROR_NO_MEDIA :
            error == ESP_ERR_NOT_SUPPORTED ? FS_ERROR_UNSUPPORTED : FS_ERROR_IO;
        if (attached) (void)sdspi_host_remove_device(card_device);
        (void)gpio_set_level(8, 1);
    } else card_result = FS_OK;
    return true;
}

fs_result_t sticky_sd_status(void) {
    return card_result != FS_OK ? card_result : mounted ? FS_OK : FS_ERROR_IO;
}
fs_result_t sticky_sd_mount(void) {
    if (!sticky_sd_prepare()) return FS_ERROR_IO;
    if (card_result != FS_OK) return card_result;
    if (mounted) return FS_OK;
    BYTE pdrv;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK) return FS_ERROR_IO;
    char name[6];
    snprintf(name, sizeof(name), "%u:", pdrv);
    drive = pdrv;
    ff_diskio_register(drive, &readonly_disk); /* Never the writable IDF SD driver. */
    esp_vfs_fat_conf_t config = {
        .base_path = STICKY_SD_BASE_PATH, .fat_drive = name, .max_files = 8,
    };
    FATFS *fatfs = NULL;
    if (esp_vfs_fat_register_cfg(&config, &fatfs) != ESP_OK) {
        ff_diskio_unregister(drive);
        drive = 0xff;
        return FS_ERROR_IO;
    }
    FRESULT result = f_mount(fatfs, name, 1); /* No formatting or label migration. */
    if (result != FR_OK) {
        /* FatFs retains its pointer even after mount failure. Detach it before
         * VFS frees the object; this does not write to the card. */
        (void)f_mount(NULL, name, 0);
        (void)esp_vfs_fat_unregister_path(STICKY_SD_BASE_PATH);
        ff_diskio_unregister(drive);
        drive = 0xff;
        return result == FR_NO_FILESYSTEM ? FS_ERROR_UNSUPPORTED :
            result == FR_NOT_READY ? FS_ERROR_NO_MEDIA : FS_ERROR_IO;
    }
    mounted = true;
    return FS_OK;
}
