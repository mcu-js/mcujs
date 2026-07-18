/* Persistent TinyUSB startup crash-loop recovery without requiring buttons. */

#include "usb_recovery.h"

#include "esp_log.h"
#include "esp_private/system_internal.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <stdbool.h>
#include <stdint.h>

#define MCUJS_USB_RECOVERY_NAMESPACE "mcujs_usb"
#define MCUJS_USB_RECOVERY_PENDING_KEY "pending"
#define APP_REQUEST_UF2_RESET_HINT 0x11F2

static const char *TAG = "mcujs_usb_recovery";
static nvs_handle_t s_nvs;
static bool s_armed;

static void enter_tinyuf2(void) __attribute__((noreturn));

static void enter_tinyuf2(void) {
    /* TinyUF2 checks this RTC reset hint before selecting the factory app. */
    (void)esp_reset_reason();
    esp_reset_reason_set_hint((esp_reset_reason_t)APP_REQUEST_UF2_RESET_HINT);
    vTaskDelay(1);
    esp_restart();
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void recovery_failure(const char *message, esp_err_t error) {
    ESP_LOGE(TAG, "%s: %s", message, esp_err_to_name(error));
    enter_tinyuf2();
}

void mcujs_usb_recovery_start(void) {
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) {
        recovery_failure("Cannot initialize USB recovery state", error);
    }

    error = nvs_open(MCUJS_USB_RECOVERY_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (error != ESP_OK) {
        recovery_failure("Cannot open USB recovery state", error);
    }

    uint8_t pending = 0;
    error = nvs_get_u8(s_nvs, MCUJS_USB_RECOVERY_PENDING_KEY, &pending);
    if (error != ESP_OK && error != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(s_nvs);
        recovery_failure("Cannot read USB recovery state", error);
    }

    if (pending != 0) {
        error = nvs_set_u8(s_nvs, MCUJS_USB_RECOVERY_PENDING_KEY, 0);
        if (error == ESP_OK) {
            error = nvs_commit(s_nvs);
        }
        nvs_close(s_nvs);
        if (error != ESP_OK) {
            recovery_failure("Cannot clear interrupted USB boot", error);
        }
        ESP_LOGW(TAG, "Interrupted TinyUSB startup detected; entering TinyUF2");
        enter_tinyuf2();
    }

    error = nvs_set_u8(s_nvs, MCUJS_USB_RECOVERY_PENDING_KEY, 1);
    if (error == ESP_OK) {
        error = nvs_commit(s_nvs);
    }
    if (error != ESP_OK) {
        nvs_close(s_nvs);
        recovery_failure("Cannot arm USB recovery state", error);
    }
    s_armed = true;

    /* A blocked TinyUSB initialization must reset into the pending check. */
    if (esp_task_wdt_status(NULL) != ESP_OK && esp_task_wdt_add(NULL) != ESP_OK) {
        nvs_close(s_nvs);
        s_armed = false;
        ESP_LOGE(TAG, "Cannot arm USB startup task watchdog");
        enter_tinyuf2();
    }
    (void)esp_task_wdt_reset();
}

void mcujs_usb_recovery_mark_healthy(void) {
    if (!s_armed) {
        return;
    }

    esp_err_t error = nvs_set_u8(s_nvs, MCUJS_USB_RECOVERY_PENDING_KEY, 0);
    if (error == ESP_OK) {
        error = nvs_commit(s_nvs);
    }
    nvs_close(s_nvs);
    s_armed = false;
    if (error != ESP_OK) {
        recovery_failure("Cannot mark TinyUSB startup healthy", error);
    }
    ESP_LOGI(TAG, "TinyUSB startup marked healthy");
}
