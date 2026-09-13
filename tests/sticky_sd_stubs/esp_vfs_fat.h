#pragma once
#include "ff.h"
#include "sdmmc_cmd.h"
typedef struct {const char *base_path, *fat_drive; size_t max_files;} esp_vfs_fat_conf_t;
esp_err_t esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t *, FATFS **);
esp_err_t esp_vfs_fat_unregister_path(const char *);
