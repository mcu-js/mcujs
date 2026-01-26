/*
 * mcujs - Board Bindings
 * 
 * Implements: board object with system information
 */

#include "bindings.h"
#include "jerryscript.h"
#include "board_config.h"
#include "board.h"
#include "usb/usb_cdc.h"
#include "neopixel.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

#if MCUJS_HAS_CYW43
#include "pico/cyw43_arch.h"
#endif

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
    
    usb_cdc_reset_usb(250);
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
 * board.enterUf2()
 * Enter UF2 bootloader mode
 */
static jerry_value_t board_enter_uf2_handler(const jerry_call_info_t *call_info_p,
                                             const jerry_value_t args[],
                                             const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;

    board_enter_uf2();

    return jerry_undefined();
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

#if MCUJS_HAS_CYW43
    /* Pico W / Pico 2 W - LED is on CYW43 chip */
    static bool cyw43_led_state = false;
    
    if (argc < 1) {
        /* No argument - return current state */
        return jerry_boolean(cyw43_led_state);
    }

    cyw43_led_state = jerry_value_to_boolean(args[0]);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, cyw43_led_state ? 1 : 0);

    return jerry_undefined();
#elif MCUJS_LED_PIN == 255
    (void)args;
    (void)argc;
    return jerry_undefined();
#else
    if (argc < 1) {
        /* No argument - return current state */
        return jerry_boolean(gpio_get(MCUJS_LED_PIN));
    }

    bool on = jerry_value_to_boolean(args[0]);
    gpio_put(MCUJS_LED_PIN, on ? 1 : 0);

    return jerry_undefined();
#endif
}

#if MCUJS_HAS_NEOPIXEL
static uint8_t board_neopixel_get_array_value(jerry_value_t array, uint32_t index) {
    char index_str[8];
    snprintf(index_str, sizeof(index_str), "%u", index);
    jerry_value_t key = jerry_string_sz(index_str);
    jerry_value_t val = jerry_object_get(array, key);
    jerry_value_free(key);

    uint8_t out = 0;
    if (jerry_value_is_number(val)) {
        out = (uint8_t)jerry_value_as_number(val);
    }
    jerry_value_free(val);
    return out;
}

static uint8_t board_neopixel_get_object_value(jerry_value_t obj, const char *key_name) {
    jerry_value_t key = jerry_string_sz(key_name);
    jerry_value_t val = jerry_object_get(obj, key);
    jerry_value_free(key);

    uint8_t out = 0;
    if (jerry_value_is_number(val)) {
        out = (uint8_t)jerry_value_as_number(val);
    }
    jerry_value_free(val);
    return out;
}

/*
 * Helper: Set a pixel using the board's compile-time wire order.
 * This ensures board.neopixel() always works correctly for the onboard LED,
 * regardless of any runtime order changes from neopixel.init().
 */
static void board_neopixel_set_pixel_rgb(uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
#if MCUJS_NEOPIXEL_ORDER_GRB
    neopixel_set_pixel_ordered(index, r, g, b, true);
#else
    neopixel_set_pixel_ordered(index, r, g, b, false);
#endif
}

/*
 * Helper: Set a single pixel from RGB object {r, g, b}.
 * Object keys always represent logical RGB.
 */
static void board_neopixel_set_from_object(uint32_t index, jerry_value_t obj) {
    uint8_t r = board_neopixel_get_object_value(obj, "r");
    uint8_t g = board_neopixel_get_object_value(obj, "g");
    uint8_t b = board_neopixel_get_object_value(obj, "b");
    board_neopixel_set_pixel_rgb(index, r, g, b);
}

/*
 * Helper: Set a single pixel from array [v0, v1, v2].
 * Array positions match the runtime-configured order from neopixel.init():
 * - If order is 'GRB': [G, R, B] - index 0 is green, index 1 is red, index 2 is blue
 * - If order is 'RGB': [R, G, B] - index 0 is red, index 1 is green, index 2 is blue
 * After extracting logical RGB, uses board's compile-time order for wire conversion.
 */
static void board_neopixel_set_from_array(uint32_t index, jerry_value_t arr) {
    jerry_length_t length = jerry_array_length(arr);
    uint8_t v0 = length > 0 ? board_neopixel_get_array_value(arr, 0) : 0;
    uint8_t v1 = length > 1 ? board_neopixel_get_array_value(arr, 1) : 0;
    uint8_t v2 = length > 2 ? board_neopixel_get_array_value(arr, 2) : 0;
    
    uint8_t r, g, b;
    
    /* Extract logical RGB based on runtime-configured order */
    if (neopixel_is_grb()) {
        /* User specified GRB: [G, R, B] */
        g = v0; r = v1; b = v2;
    } else {
        /* User specified RGB: [R, G, B] */
        r = v0; g = v1; b = v2;
    }
    
    /* Use board's compile-time order for wire conversion */
    board_neopixel_set_pixel_rgb(index, r, g, b);
}

/*
 * board.neopixel([v0, v1, v2] | {r, g, b} | [[...], ...] | [{...}, ...])
 * Shortcut for onboard NeoPixel.
 *
 * Input semantics:
 * - Array [v0, v1, v2]: Wire-order values (matches configured order, e.g. GRB)
 * - Object {r, g, b}: Logical RGB, driver handles wire conversion
 * 
 * Objects always use board's compile-time order, independent of neopixel.init().
 */
static jerry_value_t board_neopixel_handler(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args[],
                                            const jerry_length_t argc) {
    (void)call_info_p;

    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "board.neopixel requires a value");
    }

    if (!neopixel_is_ready()) {
        neopixel_init(MCUJS_NEOPIXEL_PIN, MCUJS_NEOPIXEL_LENGTH);
#if MCUJS_NEOPIXEL_ORDER_GRB
        neopixel_set_order(true);
#else
        neopixel_set_order(false);
#endif
    }

    if (!neopixel_is_ready()) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel init failed");
    }

    jerry_value_t input = args[0];

    /* Handle array input */
    if (jerry_value_is_array(input)) {
        jerry_length_t length = jerry_array_length(input);
        if (length == 0) {
            return jerry_undefined();
        }

        /* Peek at first element to determine input type */
        jerry_value_t first_key = jerry_string_sz("0");
        jerry_value_t first = jerry_object_get(input, first_key);
        jerry_value_free(first_key);
        bool first_is_number = jerry_value_is_number(first);
        bool first_is_array = jerry_value_is_array(first);
        bool first_is_object = !first_is_array && jerry_value_is_object(first);
        jerry_value_free(first);

        /* Single pixel: [v0, v1, v2] - raw wire-order */
        if (first_is_number) {
            board_neopixel_set_from_array(0, input);
            neopixel_show();
            return jerry_undefined();
        }

        /* Multi-pixel array: [[...], ...] or [{...}, ...] */
        if (first_is_array || first_is_object) {
            uint32_t max_pixels = MCUJS_NEOPIXEL_LENGTH;
            uint32_t count = length < max_pixels ? length : max_pixels;

            for (uint32_t i = 0; i < count; i++) {
                char index_str[8];
                snprintf(index_str, sizeof(index_str), "%u", i);
                jerry_value_t key = jerry_string_sz(index_str);
                jerry_value_t item = jerry_object_get(input, key);
                jerry_value_free(key);

                if (jerry_value_is_array(item)) {
                    board_neopixel_set_from_array(i, item);
                } else if (jerry_value_is_object(item)) {
                    board_neopixel_set_from_object(i, item);
                }

                jerry_value_free(item);
            }

            neopixel_show();
            return jerry_undefined();
        }
    }

    /* Handle object input: {r, g, b} - logical RGB */
    if (jerry_value_is_object(input)) {
        board_neopixel_set_from_object(0, input);
        neopixel_show();
        return jerry_undefined();
    }

    return jerry_throw_sz(JERRY_ERROR_TYPE, "board.neopixel expects array or object input");
}
#endif

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
    js_set_number(board, "cpuFreq", (double)clock_get_hz(clk_sys));
    js_set_number(board, "ledPin", MCUJS_LED_PIN == 255 ? -1 : (double)MCUJS_LED_PIN);

#if MCUJS_HAS_NEOPIXEL
    js_set_number(board, "neopixelPin", (double)MCUJS_NEOPIXEL_PIN);
    js_set_number(board, "neopixelLength", (double)MCUJS_NEOPIXEL_LENGTH);
#endif

    /* Methods */
    js_set_function(board, "freeMemory", board_free_memory_handler);
    js_set_function(board, "uniqueId", board_unique_id_handler);
    js_set_function(board, "reset", board_reset_handler);
    js_set_function(board, "millis", board_millis_handler);
    js_set_function(board, "enterUf2", board_enter_uf2_handler);
    js_set_function(board, "delay", board_delay_handler);
    js_set_function(board, "led", board_led_handler);

#if MCUJS_HAS_NEOPIXEL
    js_set_function(board, "neopixel", board_neopixel_handler);
#endif

    /* Version from build */
    #ifdef MCUJS_VERSION
    js_set_string(board, "version", MCUJS_VERSION);
    #else
    js_set_string(board, "version", "0.0.0");
    #endif

    js_register_global("board", board);
    jerry_value_free(board);
}
