/* Persistent safe boot and /app/index.js startup for MCU.js on ESP32-S3. */

#include "boot.h"

#include "engine.h"
#include "fs.h"
#include "filesystem.h"
#include "usb_cdc.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <stdint.h>
#include <stdio.h>

#define MCUJS_BOOT_NAMESPACE "mcujs_boot"
#define MCUJS_BOOT_INDEX "/app/index.js"
#define MCUJS_BOOT_HEALTHY_DELAY_US (5LL * 1000LL * 1000LL)

static const char *TAG = "mcujs_boot";
static nvs_handle_t s_nvs;
static bool s_nvs_ready;
static bool s_storage_ready;
static bool s_safe_mode = true;
static bool s_boot_attempt_active;
static bool s_health_pending;
static int64_t s_health_deadline;

static bool nvs_get_u8_default(const char *key, uint8_t *value) {
    esp_err_t error = nvs_get_u8(s_nvs, key, value);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        *value = 0;
        return true;
    }
    return error == ESP_OK;
}

static bool nvs_write_state(uint8_t safe_mode, uint8_t pending, uint8_t failures) {
    if (!s_nvs_ready || nvs_set_u8(s_nvs, "safe", safe_mode) != ESP_OK ||
        nvs_set_u8(s_nvs, "pending", pending) != ESP_OK ||
        nvs_set_u8(s_nvs, "failures", failures) != ESP_OK ||
        nvs_commit(s_nvs) != ESP_OK) {
        return false;
    }
    return true;
}

static void initialize_boot_state(void) {
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed without erase: %s", esp_err_to_name(error));
        return;
    }

    error = nvs_open(MCUJS_BOOT_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "NVS namespace open failed: %s", esp_err_to_name(error));
        return;
    }
    s_nvs_ready = true;

    uint8_t explicit_safe = 0;
    uint8_t pending = 0;
    uint8_t failures = 0;
    if (!nvs_get_u8_default("safe", &explicit_safe) ||
        !nvs_get_u8_default("pending", &pending) ||
        !nvs_get_u8_default("failures", &failures)) {
        ESP_LOGE(TAG, "Boot state read failed; forcing safe mode");
        return;
    }

    bool state_changed = false;
    if (pending != 0) {
        if (failures != UINT8_MAX) {
            failures++;
        }
        pending = 0;
        state_changed = true;
    }

    esp_reset_reason_t reset_reason = esp_reset_reason();
    if ((reset_reason == ESP_RST_TASK_WDT || reset_reason == ESP_RST_INT_WDT ||
         reset_reason == ESP_RST_WDT) && failures == 0) {
        failures = 1;
        state_changed = true;
    }

    if (state_changed && !nvs_write_state(explicit_safe, pending, failures)) {
        ESP_LOGE(TAG, "Failed to record interrupted boot; forcing safe mode");
        return;
    }

    s_safe_mode = explicit_safe != 0 || failures != 0;
}

bool mcujs_boot_init(void) {
    initialize_boot_state();

    uint8_t filesystem_initialized = 0;
    bool have_filesystem_marker = s_nvs_ready &&
        nvs_get_u8_default("fs_ready", &filesystem_initialized);
    bool may_format_filesystem = have_filesystem_marker && filesystem_initialized == 0 &&
        mcujs_filesystem_partition_is_erased();
    fs_result_t result = fs_init();
    if (result != FS_OK && may_format_filesystem) {
        ESP_LOGI(TAG, "Formatting verified-erased filesystem");
        result = fs_format();
    }
    if (result != FS_OK) {
        ESP_LOGE(TAG, "Filesystem unavailable (%d)", result);
        s_storage_ready = false;
        return false;
    }

    if (have_filesystem_marker && filesystem_initialized == 0 &&
        (nvs_set_u8(s_nvs, "fs_ready", 1) != ESP_OK || nvs_commit(s_nvs) != ESP_OK)) {
        ESP_LOGE(TAG, "Failed to persist filesystem initialization marker");
        s_safe_mode = true;
    }

    s_storage_ready = true;
    ESP_LOGI(TAG, "Filesystem mounted: %lu sectors, %lu bytes free",
             (unsigned long)fs_get_total_sectors(),
             (unsigned long)fs_get_free_space());
    return true;
}

void mcujs_boot_execute_index(void) {
    if (!s_storage_ready) {
        usb_cdc_puts("Storage unavailable; boot script skipped.\r\n");
        return;
    }
    if (s_safe_mode) {
        usb_cdc_puts("Safe mode active; /app/index.js skipped.\r\n");
        return;
    }
    if (fs_exists(MCUJS_BOOT_INDEX) != FS_OK) {
        return;
    }
    if (!nvs_write_state(0, 1, 0)) {
        s_safe_mode = true;
        usb_cdc_puts("Boot state unavailable; /app/index.js skipped.\r\n");
        return;
    }

    s_boot_attempt_active = true;
    usb_cdc_puts("Running /app/index.js\r\n");
    js_result_t result = js_engine_exec_file(MCUJS_BOOT_INDEX);
    if (result != JS_OK) {
        s_boot_attempt_active = false;
        char error[512] = {0};
        js_engine_get_error(error, sizeof(error));
        ESP_LOGE(TAG, "/app/index.js failed: %s", error);
        usb_cdc_puts("/app/index.js failed; reset will enter safe mode.\r\n");
        return;
    }

    if (s_safe_mode) {
        s_boot_attempt_active = false;
        return;
    }

    s_health_pending = true;
    s_health_deadline = esp_timer_get_time() + MCUJS_BOOT_HEALTHY_DELAY_US;
}

void mcujs_boot_task(void) {
    if (!s_health_pending || esp_timer_get_time() < s_health_deadline) {
        return;
    }
    if (!nvs_write_state(0, 0, 0)) {
        ESP_LOGE(TAG, "Failed to mark boot healthy; next reset will be safe");
        s_safe_mode = true;
    } else {
        ESP_LOGI(TAG, "/app/index.js passed the healthy-loop window");
    }
    s_boot_attempt_active = false;
    s_health_pending = false;
}

bool mcujs_boot_storage_ready(void) {
    return s_storage_ready;
}

bool mcujs_boot_safe_mode(void) {
    return s_safe_mode;
}

bool mcujs_boot_set_safe_mode(bool enabled) {
    if (!enabled && s_boot_attempt_active) {
        return false;
    }
    if (!nvs_write_state(enabled ? 1 : 0, 0, 0)) {
        return false;
    }
    s_safe_mode = enabled;
    if (enabled) {
        s_boot_attempt_active = false;
    }
    s_health_pending = false;
    return true;
}
