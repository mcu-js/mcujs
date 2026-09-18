/* Exercise production Sticky SD through its bus/VFS and diskio boundaries. */
#include "sd_card.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "diskio_impl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef MCUJS_SD_READONLY
#define MCUJS_SD_READONLY 1
#endif
static const char *scenario;
static int pins[49], bus, bus_inits, frees, card_inits, devices, reads, mounts, unmounts;
static int registrations, vfs_live;
static FATFS volume;
static const ff_diskio_impl_t *disk;
static int is(const char *name) { return !strcmp(scenario, name); }
void vTaskDelay(unsigned ms) { (void)ms; }
int gpio_set_level(int pin, int value) {
    assert(pin == 8 || pin == 10 || pin == 15);
    assert(value == 1); /* Never power off an inserted card. */
    pins[pin] = value; return ESP_OK;
}
int gpio_config(const gpio_config_t *cfg) {
    assert(cfg->mode == GPIO_MODE_OUTPUT);
    assert(cfg->pin_bit_mask == ((1ULL<<8)|(1ULL<<10)|(1ULL<<15)));
    assert(pins[8] && pins[10] && pins[15]);
    return is("gpio-failure") ? -1 : ESP_OK;
}
int spi_bus_initialize(int host, const spi_bus_config_t *cfg, int dma) {
    assert(host == SPI2_HOST && dma == SPI_DMA_CH_AUTO);
    assert(cfg->mosi_io_num == 14 && cfg->miso_io_num == 12 && cfg->sclk_io_num == 13);
    assert(cfg->quadwp_io_num == -1 && cfg->quadhd_io_num == -1 && cfg->max_transfer_sz == 4096);
    bus_inits++;
    if (is("foreign-bus")) return ESP_ERR_INVALID_STATE;
    assert(!bus); bus = 1; return ESP_OK;
}
int spi_bus_free(int host) { assert(host == SPI2_HOST && bus && !devices); bus = 0; frees++; return ESP_OK; }
esp_err_t sdspi_host_init(void) { assert(bus && pins[8] && pins[15] && pins[10]); return is("host-failure") ? -1 : ESP_OK; }
esp_err_t sdspi_host_init_device(const sdspi_device_config_t *cfg, sdspi_dev_handle_t *out) {
    assert(cfg->host_id == SPI2_HOST && cfg->gpio_cs == 8);
    if (is("device-failure")) return -1;
    devices++; *out = 42; return ESP_OK;
}
esp_err_t sdspi_host_remove_device(sdspi_dev_handle_t d) { assert(d == 42 && devices); devices--; return ESP_OK; }
esp_err_t sdmmc_card_init(const sdmmc_host_t *host, sdmmc_card_t *card) {
    assert(bus && devices && pins[8] && pins[15] && pins[10]);
    assert(host->slot == 42 && host->max_freq_khz == 4000 && host->command_timeout_ms == 1000);
    card_inits++;
    card->csd.capacity = 8192; card->csd.sector_size = is("sector-size") ? 4096 : 512;
    return is("absent") ? ESP_ERR_TIMEOUT : ESP_OK;
}
esp_err_t sdmmc_read_sectors(sdmmc_card_t *card, void *bytes, size_t sector, size_t count) {
    assert(bus && devices && card->csd.sector_size == 512);
    assert(count && sector < 8192 && count <= 8192-sector);
    reads++; memset(bytes, 0x5a, count*512); return is("read-failure") ? -1 : ESP_OK;
}
esp_err_t sdmmc_get_status(sdmmc_card_t *c) { assert(c->csd.capacity == 8192); return ESP_OK; }
esp_err_t sdmmc_write_sectors(sdmmc_card_t *c, const void *b, size_t s, size_t n) {
    assert(!MCUJS_SD_READONLY && c->csd.capacity == 8192 && b && s < 8192 && n <= 8192-s); return ESP_OK;
}
esp_err_t ff_diskio_get_drive(BYTE *d) { *d = 2; return is("drive-failure") ? -1 : ESP_OK; }
void ff_diskio_register(BYTE d, const ff_diskio_impl_t *impl) {
    assert(d == 2);
    if (impl) { assert(!disk); registrations++; } else { assert(!vfs_live); }
    disk = impl;
}
esp_err_t esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t *cfg, FATFS **out) {
    assert(disk && !strcmp(cfg->base_path, "/mcujs-sd") && !strcmp(cfg->fat_drive, "2:"));
    assert(cfg->max_files > 0);
    if (is("vfs-failure")) return -1;
    assert(!vfs_live); vfs_live = 1; *out = &volume; return ESP_OK;
}
esp_err_t esp_vfs_fat_unregister_path(const char *path) {
    assert(vfs_live && !volume.mounted && !strcmp(path, "/mcujs-sd"));
    vfs_live = 0; return ESP_OK;
}
FRESULT f_mount(FATFS *fs, const char *drive, BYTE now) {
    assert(!strcmp(drive, "2:"));
    if (!fs) { assert(!now); unmounts++; volume.mounted = 0; return FR_OK; }
    assert(fs == &volume && disk && now == 1); mounts++; volume.mounted = 1;
    assert(disk->init(2) == (MCUJS_SD_READONLY ? STA_PROTECT : 0));
    assert(disk->status(2) == (MCUJS_SD_READONLY ? STA_PROTECT : 0));
    if (is("unsupported")) return FR_NO_FILESYSTEM;
    if (is("mount-failure")) return FR_DISK_ERR;
    return FR_OK;
}
int main(int argc, char **argv) {
    assert(argc == 2); scenario = argv[1];
    if (is("foreign-bus") || is("gpio-failure")) {
        assert(!sd_card_prepare());
        assert(!devices && !card_inits && !disk);
        assert(frees == (is("gpio-failure") ? 1 : 0));
        if (is("foreign-bus")) assert(!pins[8] && !pins[15] && !pins[10]);
    } else {
        assert(sd_card_prepare()); /* Unavailable card must not disable display. */
        assert(bus && pins[8] && pins[15] && pins[10] && !mounts);
        assert(sd_card_prepare() && bus_inits == 1);
        int init_count = card_inits;
        fs_result_t result = sd_card_mount();
        if (is("absent")) assert(result == FS_ERROR_NO_MEDIA && !devices);
        else if (is("unsupported") || is("sector-size")) assert(result == FS_ERROR_UNSUPPORTED);
        else if (is("host-failure") || is("device-failure") || is("drive-failure") || is("vfs-failure") || is("mount-failure")) assert(result == FS_ERROR_IO);
        else {
            assert(result == FS_OK && sd_card_status() == FS_OK);
            assert(sd_card_mount() == FS_OK && mounts == 1 && registrations == 1);
            BYTE bytes[1024] = {0}; DWORD value = 0; WORD size = 0;
            assert(disk->status(1) & STA_NOINIT);
            assert(disk->read(1, bytes, 0, 1) == RES_NOTRDY);
            assert(disk->read(2, bytes, 0, 0) == RES_PARERR);
            assert(disk->read(2, NULL, 0, 1) == RES_PARERR);
            assert(disk->read(2, bytes, 8192, 1) == RES_PARERR);
            assert(disk->read(2, bytes, 8191, 2) == RES_PARERR);
            assert(disk->read(2, bytes, 0xffffffffu, 2) == RES_PARERR && !reads);
            assert(disk->write(2, bytes, 0, 1) == (MCUJS_SD_READONLY ? RES_WRPRT : RES_OK));
            assert(disk->write(1, NULL, 0xffffffffu, 0) == (MCUJS_SD_READONLY ? RES_WRPRT : RES_NOTRDY));
            assert(disk->ioctl(2, CTRL_TRIM, bytes) == RES_WRPRT);
            assert(disk->ioctl(2, 255, bytes) == RES_WRPRT);
            assert(disk->ioctl(2, CTRL_SYNC, NULL) == RES_OK);
            assert(disk->ioctl(2, GET_SECTOR_COUNT, &value) == RES_OK && value == 8192);
            assert(disk->ioctl(2, GET_SECTOR_SIZE, &size) == RES_OK && size == 512);
            assert(disk->ioctl(2, GET_BLOCK_SIZE, &value) == RES_OK && value == 1);
            assert(disk->ioctl(2, GET_SECTOR_SIZE, NULL) == RES_PARERR);
            DRESULT read_result = disk->read(2, bytes, 8190, 2);
            if (is("read-failure")) {
                assert(read_result == RES_ERROR && sd_card_status() == FS_ERROR_IO);
                assert(disk->status(2) == (STA_NOINIT | (MCUJS_SD_READONLY ? STA_PROTECT : 0)));
                assert(disk->read(2, bytes, 0, 1) == RES_NOTRDY && reads == 1);
                assert(sd_card_mount() == FS_ERROR_IO); /* No stale handle retarget. */
            } else assert(read_result == RES_OK && reads == 1 && bytes[0] == 0x5a && bytes[1023] == 0x5a);
        }
        if (result != FS_OK) {
            assert(!disk && !vfs_live);
            assert(sd_card_mount() == result);
            if (is("unsupported") || is("mount-failure")) assert(unmounts == mounts);
        }
        assert(card_inits == init_count && bus && !frees);
    }
    printf("PASS Sticky SD: %s\n", scenario);
}
