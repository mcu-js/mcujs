#pragma once
#include "fake_idf.h"
typedef int esp_err_t;
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_INVALID_STATE 0x103
typedef int sdspi_dev_handle_t;
typedef struct {int slot, max_freq_khz, command_timeout_ms;} sdmmc_host_t;
typedef struct {struct {uint32_t capacity, sector_size;} csd;} sdmmc_card_t;
typedef struct {int host_id, gpio_cs;} sdspi_device_config_t;
#define SDSPI_HOST_DEFAULT() ((sdmmc_host_t){0})
#define SDSPI_DEVICE_CONFIG_DEFAULT() ((sdspi_device_config_t){0})
esp_err_t sdspi_host_init(void);
esp_err_t sdspi_host_init_device(const sdspi_device_config_t *, sdspi_dev_handle_t *);
esp_err_t sdspi_host_remove_device(sdspi_dev_handle_t);
esp_err_t sdmmc_card_init(const sdmmc_host_t *, sdmmc_card_t *);
esp_err_t sdmmc_read_sectors(sdmmc_card_t *, void *, size_t, size_t);
