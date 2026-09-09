/* MCU.js headless ESP32-S3 bring-up runtime. */

#include "engine.h"
#include "board_config.h"
#include "board_power.h"
#include "boot.h"
#include "repl.h"
#include "usb_cdc.h"
#include "usb_msc.h"
#include "usb_recovery.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "mcujs";

static bool run_smoke(const char *name, const char *source, const char *expected) {
    char result[256] = {0};
    js_result_t status = js_engine_exec(source, strlen(source), result, sizeof(result));
    if (status != JS_OK) {
        char error[512] = {0};
        js_engine_get_error(error, sizeof(error));
        ESP_LOGE(TAG, "MCUJS_SMOKE_%s_FAIL: %s", name, error);
        return false;
    }
    if (expected != NULL && strcmp(result, expected) != 0) {
        ESP_LOGE(TAG, "MCUJS_SMOKE_%s_FAIL: expected %s, got %s", name, expected, result);
        return false;
    }
    ESP_LOGI(TAG, "MCUJS_SMOKE_%s_OK%s%s", name,
             result[0] != '\0' ? ": " : "", result);
    return true;
}

void app_main(void) {
    if (!mcujs_board_power_init()) { ESP_LOGE(TAG, "Board power initialization failed"); return; }
    mcujs_usb_recovery_start();
    mcujs_boot_init();
    usb_cdc_init();
    usb_cdc_task();
    mcujs_usb_recovery_mark_healthy();
    usb_cdc_puts("\r\nMCU.js ESP32-S3 headless runtime\r\n");
    usb_cdc_puts("Build: " MCUJS_BUILD_ID "\r\n");

    if (js_engine_init() != JS_OK) {
        ESP_LOGE(TAG, "MCUJS_SMOKE_ENGINE_FAIL");
        return;
    }
    ESP_LOGI(TAG, "MCUJS_SMOKE_ENGINE_OK");

    bool ok = true;
    ok &= run_smoke("ARITHMETIC", "2 + 2", "4");
    ok &= run_smoke("CONSOLE", "console.log('MCUJS_SMOKE_CONSOLE_OK')", "undefined");
    ok &= run_smoke("IDENTITY", "board.name + ':' + board.chip",
                    "'" MCUJS_BOARD_NAME ":ESP32-S3'");
#ifdef MCUJS_BOARD_SEEED_XIAO_ESP32S3
    ok &= run_smoke("GPIO", "GPIO.init(21, GPIO.OUTPUT); GPIO.set(21, false); GPIO.get(21)",
                    "false");
    ok &= run_smoke("TIMER_SCHEDULE",
                    "setTimeout(function(){ GPIO.set(21, true); console.log('MCUJS_SMOKE_TIMER_OK'); }, 100)",
                    NULL);

#endif
    ESP_LOGI(TAG, "%s", ok ? "MCUJS_SMOKE_SYNC_OK" : "MCUJS_SMOKE_SYNC_FAIL");
    bool runtime_task_watched = esp_task_wdt_status(NULL) == ESP_OK;
    if (!runtime_task_watched) {
        runtime_task_watched = esp_task_wdt_add(NULL) == ESP_OK;
    }
    if (!runtime_task_watched) {
        ESP_LOGE(TAG, "Runtime task watchdog subscription failed; forcing safe mode");
        mcujs_boot_set_safe_mode(true);
    } else {
        esp_task_wdt_reset();
    }
    mcujs_boot_execute_index();
#ifndef MCUJS_BOARD_SEEED_RETERMINAL_STICKY
    if (!mcujs_usb_msc_expose()) {
        ESP_LOGE(TAG, "USB MSC storage ownership transfer failed");
    }
#endif
    usb_cdc_puts("MCU.js ready.\r\n");
    repl_init();

    while (true) {
        usb_cdc_task();
#ifndef MCUJS_BOARD_SEEED_RETERMINAL_STICKY
        mcujs_usb_msc_task();
#endif
        repl_task();
        js_engine_process_timers();
        mcujs_boot_task();
        mcujs_board_power_task();
        if (runtime_task_watched) {
            esp_task_wdt_reset();
        }
        /* One RTOS tick is 10 ms with the IDF default. pdMS_TO_TICKS(1)
         * rounds to zero and starves IDLE0, triggering the task watchdog. */
        vTaskDelay(1);
    }
}
