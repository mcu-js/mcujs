/*
 * mcujs - Board Bindings
 * 
 * Implements: board object with system information
 */

#include "bindings.h"
#include "runtime_features.h"
#include "validation.h"
#include "jerryscript.h"
#include "board_config.h"
#include "board.h"
#include "fs.h"
#include "usb/usb_cdc.h"
#include "neopixel.h"
#include "onboard_led.h"

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

static jerry_value_t board_storage_ready_handler(const jerry_call_info_t *call_info_p,
                                                  const jerry_value_t args[],
                                                  const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    return jerry_boolean(fs_storage_ready());
}

#if MCUJS_REGISTRY_ONBOARD_LED
#if !MCUJS_HAS_CYW43 && MCUJS_LED_PIN == 255
#error "Onboard LED registry entry requires CYW43 or a GPIO LED pin"
#endif

/* board.led(on) controls only a physically declared onboard LED. */
#if MCUJS_HAS_CYW43
static jerry_value_t board_led_handler(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args[],
                                        const jerry_length_t argc) {
    (void)call_info_p;

    /* Pico W / Pico 2 W - LED is on CYW43 chip */
    static bool cyw43_led_state = false;
    
    if (argc < 1) {
        /* No argument - return current state */
        return jerry_boolean(cyw43_led_state);
    }

    bool on;
    mcujs_arg_status_t status = mcujs_get_boolean(args, argc, 0, &on);
    if (status != MCUJS_ARG_OK) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "LED state must be boolean");
    }
    cyw43_led_state = on;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, cyw43_led_state ? 1 : 0);

    return jerry_undefined();
}
#else
#define board_led_handler mcujs_rp2_board_led_handler
#endif
#endif

#if MCUJS_HAS_NEOPIXEL
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} board_neopixel_color_t;

static jerry_value_t board_neopixel_get_array_value(jerry_value_t array,
                                                     uint32_t index,
                                                     uint8_t *out) {
    jerry_value_t value = jerry_object_get_index(array, index);
    if (jerry_value_is_exception(value)) return value;
    if (jerry_value_is_undefined(value)) {
        *out = 0;
        jerry_value_free(value);
        return jerry_undefined();
    }
    mcujs_arg_status_t status = mcujs_value_to_byte(value, out);
    jerry_value_free(value);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status,
                              "NeoPixel colors must be finite numbers",
                              "NeoPixel colors must be integers 0..255");
    }
    return jerry_undefined();
}

static jerry_value_t board_neopixel_get_object_value(jerry_value_t obj,
                                                      const char *key_name,
                                                      uint8_t *out) {
    jerry_value_t key = jerry_string_sz(key_name);
    jerry_value_t value = jerry_object_get(obj, key);
    jerry_value_free(key);
    if (jerry_value_is_exception(value)) return value;
    if (jerry_value_is_undefined(value)) {
        *out = 0;
        jerry_value_free(value);
        return jerry_undefined();
    }
    mcujs_arg_status_t status = mcujs_value_to_byte(value, out);
    jerry_value_free(value);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status,
                              "NeoPixel colors must be finite numbers",
                              "NeoPixel colors must be integers 0..255");
    }
    return jerry_undefined();
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
static jerry_value_t board_neopixel_parse_object(
    jerry_value_t object, board_neopixel_color_t *color) {
    jerry_value_t keys = jerry_object_keys(object);
    if (jerry_value_is_exception(keys)) return keys;
    uint32_t key_count = jerry_array_length(keys);
    for (uint32_t i = 0; i < key_count; i++) {
        jerry_value_t key = jerry_object_get_index(keys, i);
        if (jerry_value_is_exception(key)) {
            jerry_value_free(keys);
            return key;
        }
        jerry_size_t size = jerry_string_size(key, JERRY_ENCODING_UTF8);
        char name[2] = {0};
        if (size == 1) {
            jerry_string_to_buffer(key, JERRY_ENCODING_UTF8,
                                   (jerry_char_t *)name, size);
        }
        jerry_value_free(key);
        if (size != 1 || (name[0] != 'r' && name[0] != 'g' && name[0] != 'b')) {
            jerry_value_free(keys);
            return jerry_throw_sz(JERRY_ERROR_RANGE,
                                  "NeoPixel color objects allow only r, g, and b");
        }
    }
    jerry_value_free(keys);

    jerry_value_t result =
        board_neopixel_get_object_value(object, "r", &color->r);
    if (jerry_value_is_exception(result)) return result;
    jerry_value_free(result);
    result = board_neopixel_get_object_value(object, "g", &color->g);
    if (jerry_value_is_exception(result)) return result;
    jerry_value_free(result);
    return board_neopixel_get_object_value(object, "b", &color->b);
}

/*
 * Helper: Set a single pixel from array [v0, v1, v2].
 * Array positions match the onboard device's declared order:
 * - If order is 'GRB': [G, R, B] - index 0 is green, index 1 is red, index 2 is blue
 * - If order is 'RGB': [R, G, B] - index 0 is red, index 1 is green, index 2 is blue
 * After extracting logical RGB, uses board's compile-time order for wire conversion.
 */
static jerry_value_t board_neopixel_parse_array(
    jerry_value_t array, board_neopixel_color_t *color, bool grb) {
    jerry_length_t length = jerry_array_length(array);
    if (length > 3) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "NeoPixel color arrays contain at most three bytes");
    }
    uint8_t values[3] = {0, 0, 0};
    for (jerry_length_t i = 0; i < length; i++) {
        jerry_value_t result =
            board_neopixel_get_array_value(array, i, &values[i]);
        if (jerry_value_is_exception(result)) return result;
        jerry_value_free(result);
    }
    /* Extract logical RGB based on the onboard device's declared order. */
    if (grb) {
        /* User specified GRB: [G, R, B] */
        color->g = values[0]; color->r = values[1]; color->b = values[2];
    } else {
        /* User specified RGB: [R, G, B] */
        color->r = values[0]; color->g = values[1]; color->b = values[2];
    }
    return jerry_undefined();
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

    jerry_value_t input = args[0];
    board_neopixel_color_t colors[MCUJS_NEOPIXEL_LENGTH] = {0};
    uint32_t color_count = 0;
    jerry_value_t parsed;
#if MCUJS_NEOPIXEL_ORDER_GRB
    const bool onboard_grb = true;
#else
    const bool onboard_grb = false;
#endif

    if (jerry_value_is_array(input)) {
        jerry_length_t length = jerry_array_length(input);
        if (length == 0) {
            parsed = board_neopixel_parse_array(input, &colors[0],
                                                 onboard_grb);
            color_count = 1;
        } else {
            jerry_value_t first = jerry_object_get_index(input, 0);
            if (jerry_value_is_exception(first)) return first;
            bool first_is_array = jerry_value_is_array(first);
            bool first_is_object = !first_is_array && jerry_value_is_object(first);
            bool single_color = jerry_value_is_number(first) ||
                                jerry_value_is_undefined(first);
            jerry_value_free(first);

            if (single_color) {
                parsed = board_neopixel_parse_array(input, &colors[0],
                                                     onboard_grb);
                color_count = 1;
            } else if (first_is_array || first_is_object) {
                if (length > MCUJS_NEOPIXEL_LENGTH) {
                    return jerry_throw_sz(
                        JERRY_ERROR_RANGE,
                        "NeoPixel pixel list exceeds the onboard length");
                }
                for (uint32_t i = 0; i < length; i++) {
                    jerry_value_t item = jerry_object_get_index(input, i);
                    if (jerry_value_is_exception(item)) return item;
                    bool item_matches = first_is_array
                        ? jerry_value_is_array(item)
                        : (!jerry_value_is_array(item) &&
                           jerry_value_is_object(item));
                    if (!item_matches) {
                        jerry_value_free(item);
                        return jerry_throw_sz(
                            JERRY_ERROR_TYPE,
                            "NeoPixel pixel lists must use one color shape");
                    }
                    parsed = first_is_array
                        ? board_neopixel_parse_array(item, &colors[i],
                                                     onboard_grb)
                        : board_neopixel_parse_object(item, &colors[i]);
                    jerry_value_free(item);
                    if (jerry_value_is_exception(parsed)) return parsed;
                    jerry_value_free(parsed);
                }
                color_count = length;
                parsed = jerry_undefined();
            } else {
                return jerry_throw_sz(
                    JERRY_ERROR_TYPE,
                    "board.neopixel expects byte colors or color objects");
            }
        }
    } else if (jerry_value_is_object(input)) {
        parsed = board_neopixel_parse_object(input, &colors[0]);
        color_count = 1;
    } else {
        return jerry_throw_sz(JERRY_ERROR_TYPE,
                              "board.neopixel expects array or object input");
    }

    if (jerry_value_is_exception(parsed)) return parsed;
    jerry_value_free(parsed);

    if (!neopixel_is_ready() || neopixel_pin() != MCUJS_NEOPIXEL_PIN ||
        neopixel_length() != MCUJS_NEOPIXEL_LENGTH) {
        if (!neopixel_init(MCUJS_NEOPIXEL_PIN, MCUJS_NEOPIXEL_LENGTH)) {
            return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel init failed");
        }
    }
#if MCUJS_NEOPIXEL_ORDER_GRB
    neopixel_set_order(true);
#else
    neopixel_set_order(false);
#endif
    for (uint32_t i = 0; i < color_count; i++) {
        board_neopixel_set_pixel_rgb(i, colors[i].r, colors[i].g, colors[i].b);
    }
    neopixel_show();
    return jerry_undefined();
}
#endif

/* Create the RP board object before registering its global compatibility alias. */
jerry_value_t mcujs_rp2_create_board_module(void) {
    jerry_value_t board = jerry_object();

    /* Static properties */
    js_set_number(board, "flashSize", (double)MCUJS_FLASH_SIZE);
    js_set_number(board, "ramSize", (double)MCUJS_RAM_SIZE);
    js_set_number(board, "cpuFreq", (double)clock_get_hz(clk_sys));
#if MCUJS_REGISTRY_LED_PIN
    js_set_number(board, "ledPin", (double)MCUJS_LED_PIN);
#endif

#if MCUJS_REGISTRY_ONBOARD_NEOPIXEL
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
#if MCUJS_REGISTRY_ONBOARD_LED
    js_set_function(board, "led", board_led_handler);
#endif

#if MCUJS_REGISTRY_ONBOARD_NEOPIXEL
    js_set_function(board, "neopixel", board_neopixel_handler);
#endif

    if (!js_board_apply_registry(board, NULL, board_storage_ready_handler)) {
        jerry_value_free(board);
        return jerry_throw_sz(JERRY_ERROR_COMMON,
                              "Unable to apply the board registry");
    }
    return board;
}

/*
 * Register board bindings
 */
void js_bind_board(void) {
    jerry_value_t board = mcujs_rp2_create_board_module();
    if (jerry_value_is_exception(board)) {
        jerry_value_free(board);
        return;
    }
    js_register_global("board", board);
    jerry_value_free(board);
}
