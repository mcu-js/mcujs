/*
 * mcujs - Board Bindings
 * 
 * Implements: board object with system information
 */

#include "bindings.h"
#include "jerryscript.h"
#include "board_config.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_set_string(jerry_value_t object, const char *name, const char *value);
extern void js_set_number(jerry_value_t object, const char *name, double value);
extern void js_register_global(const char *name, jerry_value_t object);

/*
 * board.freeMemory()
 * Returns approximate free heap memory
 */
static jerry_value_t board_free_memory_handler(const jerry_call_info_t *call_info_p,
                                                const jerry_value_t args[],
                                                const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    /* Get JerryScript heap stats */
    jerry_heap_stats_t stats;
    if (jerry_heap_stats(&stats)) {
        size_t free_bytes = stats.size - stats.allocated_bytes;
        return jerry_number((double)free_bytes);
    }
    
    return jerry_number(0);
}

/*
 * board.uniqueId()
 * Returns board unique ID as hex string
 */
static jerry_value_t board_unique_id_handler(const jerry_call_info_t *call_info_p,
                                              const jerry_value_t args[],
                                              const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    
    /* Convert to hex string */
    char hex_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        sprintf(&hex_str[i * 2], "%02X", id.id[i]);
    }
    hex_str[sizeof(hex_str) - 1] = '\0';
    
    return jerry_string_sz(hex_str);
}

/*
 * board.reset()
 * Reset the microcontroller
 */
static jerry_value_t board_reset_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    /* Trigger a watchdog reset */
    watchdog_enable(1, false);
    while (1) {
        tight_loop_contents();
    }
    
    /* Never reached */
    return jerry_undefined();
}

/*
 * board.millis()
 * Returns milliseconds since boot
 */
static jerry_value_t board_millis_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    
    return jerry_number((double)to_ms_since_boot(get_absolute_time()));
}

/*
 * board.delay(ms)
 * Blocking delay in milliseconds
 */
static jerry_value_t board_delay_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return jerry_undefined();
    }
    
    uint32_t ms = (uint32_t)jerry_value_as_number(args[0]);
    sleep_ms(ms);
    
    return jerry_undefined();
}

/*
 * board.led(on)
 * Control the onboard LED
 */
static jerry_value_t board_led_handler(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args[],
                                        const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        /* No argument - return current state */
        return jerry_boolean(gpio_get(MCUJS_LED_PIN));
    }
    
    bool on = jerry_value_to_boolean(args[0]);
    gpio_put(MCUJS_LED_PIN, on ? 1 : 0);
    
    return jerry_undefined();
}

/*
 * Register board bindings
 */
void js_bind_board(void) {
    jerry_value_t board = jerry_object();
    
    /* Static properties */
    js_set_string(board, "name", MCUJS_BOARD_NAME);
    js_set_string(board, "chip", MCUJS_BOARD_CHIP);
    js_set_number(board, "flashSize", (double)MCUJS_FLASH_SIZE);
    js_set_number(board, "ramSize", (double)MCUJS_RAM_SIZE);
    js_set_number(board, "cpuFreq", (double)MCUJS_CPU_FREQ_HZ);
    js_set_number(board, "ledPin", (double)MCUJS_LED_PIN);
    
    /* Methods */
    js_set_function(board, "freeMemory", board_free_memory_handler);
    js_set_function(board, "uniqueId", board_unique_id_handler);
    js_set_function(board, "reset", board_reset_handler);
    js_set_function(board, "millis", board_millis_handler);
    js_set_function(board, "delay", board_delay_handler);
    js_set_function(board, "led", board_led_handler);
    
    /* Version from build */
    #ifdef MCUJS_VERSION
    js_set_string(board, "version", MCUJS_VERSION);
    #else
    js_set_string(board, "version", "0.0.0");
    #endif
    
    js_register_global("board", board);
    jerry_value_free(board);
}
