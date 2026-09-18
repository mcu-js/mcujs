#pragma once
#include "sdmmc_cmd.h"
#define SDMMC_HOST_FLAG_1BIT 1
#define SDMMC_HOST_DEFAULT() ((sdmmc_host_t){.slot=1})
typedef struct {int width, clk, cmd, d0, cd;} sdmmc_slot_config_t;
#define SDMMC_SLOT_CONFIG_DEFAULT() ((sdmmc_slot_config_t){.cd=-1})
esp_err_t sdmmc_host_init(void);
esp_err_t sdmmc_host_init_slot(int, const sdmmc_slot_config_t *);
