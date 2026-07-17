/* MCU.js board binding for Seeed XIAO ESP32-S3. */

#include "bindings.h"
#include "board_config.h"
#include "boot.h"
#include "jerryscript.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_private/system_internal.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

static bool s_led_initialized;

static void board_led_init(void) {
    if (!s_led_initialized) {
        gpio_reset_pin(MCUJS_LED_PIN);
        gpio_set_direction(MCUJS_LED_PIN, GPIO_MODE_INPUT_OUTPUT);
        gpio_set_level(MCUJS_LED_PIN, 1);
        s_led_initialized = true;
    }
}

static jerry_value_t board_free_memory(const jerry_call_info_t *info,
                                       const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    return jerry_number((double)heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

static jerry_value_t board_unique_id(const jerry_call_info_t *info,
                                     const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    uint8_t mac[6];
    char id[13];
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Unable to read board identity");
    }
    snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return jerry_string_sz(id);
}

static jerry_value_t board_reset(const jerry_call_info_t *info,
                                 const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_restart();
    return jerry_undefined();
}

static jerry_value_t board_millis(const jerry_call_info_t *info,
                                  const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    return jerry_number((double)(esp_timer_get_time() / 1000));
}

static jerry_value_t board_delay(const jerry_call_info_t *info,
                                 const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    uint32_t delay_ms = (uint32_t)js_get_number_arg(args, argc, 0, 0);
    TickType_t ticks = pdMS_TO_TICKS(delay_ms);
    if (delay_ms > 0 && ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
    return jerry_undefined();
}

static jerry_value_t board_enter_uf2(const jerry_call_info_t *info,
                                     const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    enum { APP_REQUEST_UF2_RESET_HINT = 0x11F2 };
    /* TinyUF2 requires this reference so IDF links the hint implementation. */
    (void)esp_reset_reason();
    esp_reset_reason_set_hint((esp_reset_reason_t)APP_REQUEST_UF2_RESET_HINT);
    vTaskDelay(pdMS_TO_TICKS(20));
    esp_restart();
    return jerry_undefined();
}

static jerry_value_t board_safe_mode(const jerry_call_info_t *info,
                                     const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (argc == 0) {
        return jerry_boolean(mcujs_boot_safe_mode());
    }
    if (!jerry_value_is_boolean(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "safeMode value must be boolean");
    }
    if (!mcujs_boot_set_safe_mode(jerry_value_is_true(args[0]))) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Unable to persist safe mode");
    }
    return jerry_undefined();
}

static jerry_value_t board_storage_ready(const jerry_call_info_t *info,
                                         const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    return jerry_boolean(mcujs_boot_storage_ready());
}

static jerry_value_t board_led(const jerry_call_info_t *info,
                               const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    board_led_init();
    if (argc > 0) {
        bool on = js_get_boolean_arg(args, argc, 0, false);
        gpio_set_level(MCUJS_LED_PIN, on ? 0 : 1);
        return jerry_undefined();
    }
    return jerry_boolean(gpio_get_level(MCUJS_LED_PIN) == 0);
}

void js_bind_board(void) {
    jerry_value_t board = jerry_object();
    js_set_string(board, "name", MCUJS_BOARD_NAME);
    js_set_string(board, "chip", MCUJS_BOARD_CHIP);
    js_set_number(board, "flashSize", (double)MCUJS_FLASH_SIZE);
    js_set_number(board, "ramSize", (double)MCUJS_RAM_SIZE);
    js_set_number(board, "cpuFreq", (double)MCUJS_CPU_FREQ_HZ);
    js_set_number(board, "ledPin", MCUJS_LED_PIN);
    js_set_string(board, "version", MCUJS_VERSION);
    js_set_function(board, "freeMemory", board_free_memory);
    js_set_function(board, "uniqueId", board_unique_id);
    js_set_function(board, "reset", board_reset);
    js_set_function(board, "millis", board_millis);
    js_set_function(board, "delay", board_delay);
    js_set_function(board, "enterUf2", board_enter_uf2);
    js_set_function(board, "safeMode", board_safe_mode);
    js_set_function(board, "storageReady", board_storage_ready);
    js_set_function(board, "led", board_led);
    js_register_global("board", board);
    jerry_value_free(board);
}
