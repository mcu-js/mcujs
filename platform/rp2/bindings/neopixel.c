#include "neopixel.h"
#include "neopixel.h"
#include "bindings.h"
#include "jerryscript.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "neopixel.pio.h"
#include "pico/stdlib.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef MCUJS_NEOPIXEL_DEFAULT_FREQ
#define MCUJS_NEOPIXEL_DEFAULT_FREQ 800000
#endif

static PIO s_pio = pio0;
static uint s_sm = 0;
static uint s_offset = 0;
static bool s_program_loaded = false;
static bool s_rgb_order_grb = true;
static bool s_ready = false;
static uint32_t s_length = 0;
static uint32_t s_pin = 0;
static uint32_t s_buffer_len = 0;
static uint32_t *s_pixels = NULL;

static void neopixel_cleanup(void) {
    if (s_pixels != NULL) {
        free(s_pixels);
        s_pixels = NULL;
        s_buffer_len = 0;
    }
    s_length = 0;
    s_ready = false;
}

void neopixel_init(uint32_t pin, uint32_t length) {
    neopixel_cleanup();

    if (length == 0) {
        return;
    }

    s_pin = pin;
    s_length = length;
    s_buffer_len = length;

    s_pixels = (uint32_t *)calloc(s_buffer_len, sizeof(uint32_t));
    if (s_pixels == NULL) {
        s_length = 0;
        return;
    }

    if (!s_program_loaded) {
        s_offset = pio_add_program(s_pio, &mcujs_ws2812_program);
        s_program_loaded = true;
    }

    mcujs_ws2812_program_init(s_pio, s_sm, s_offset, s_pin, MCUJS_NEOPIXEL_DEFAULT_FREQ, false);
    s_ready = true;
}

void neopixel_set_order(bool grb) {
    s_rgb_order_grb = grb;
}

bool neopixel_is_grb(void) {
    return s_rgb_order_grb;
}

bool neopixel_is_ready(void) {
    return s_ready;
}

uint32_t neopixel_length(void) {
    return s_length;
}

void neopixel_set_pixel_ordered(uint32_t index, uint8_t r, uint8_t g, uint8_t b, bool grb) {
    if (!s_ready || index >= s_length || s_pixels == NULL) {
        return;
    }

    /*
     * Pack RGB into 24-bit value for wire transmission.
     * PIO sends MSB first, so high byte goes out first on the wire.
     * 
     * For GRB LEDs: wire order is G-R-B, so pack as (G << 16) | (R << 8) | B
     * For RGB LEDs: wire order is R-G-B, so pack as (R << 16) | (G << 8) | B
     *
     * The 'grb' flag indicates the LED's wire format, not a data transformation.
     * Input r/g/b are always logical colors; we reorder for the wire.
     */
    uint32_t color = grb
        ? ((uint32_t)g << 16) | ((uint32_t)r << 8) | b
        : ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    s_pixels[index] = color;
}
void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
    neopixel_set_pixel_ordered(index, r, g, b, s_rgb_order_grb);
}

void neopixel_clear(void) {
    if (!s_ready || s_pixels == NULL) {
        return;
    }

    memset(s_pixels, 0, s_buffer_len * sizeof(uint32_t));
}

void neopixel_show(void) {
    if (!s_ready || s_pixels == NULL) {
        return;
    }

    for (uint32_t i = 0; i < s_length; i++) {
        pio_sm_put_blocking(s_pio, s_sm, s_pixels[i] << 8u);
    }

    sleep_us(80);
}

static jerry_value_t neopixel_init_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;

    if (argc < 1 || !jerry_value_is_object(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "neopixel.init requires an options object");
    }

    jerry_value_t options = args[0];
    jerry_value_t pin_key = jerry_string_sz("pin");
    jerry_value_t len_key = jerry_string_sz("length");
    jerry_value_t order_key = jerry_string_sz("order");

    jerry_value_t pin_val = jerry_object_get(options, pin_key);
    jerry_value_t len_val = jerry_object_get(options, len_key);
    jerry_value_t order_val = jerry_object_get(options, order_key);

    jerry_value_free(pin_key);
    jerry_value_free(len_key);
    jerry_value_free(order_key);

    if (!jerry_value_is_number(pin_val) || !jerry_value_is_number(len_val)) {
        jerry_value_free(pin_val);
        jerry_value_free(len_val);
        jerry_value_free(order_val);
        return jerry_throw_sz(JERRY_ERROR_TYPE, "neopixel.init requires pin and length numbers");
    }

    uint32_t pin = (uint32_t)jerry_value_as_number(pin_val);
    uint32_t length = (uint32_t)jerry_value_as_number(len_val);

#ifdef MCUJS_NEOPIXEL_ORDER_GRB
    s_rgb_order_grb = MCUJS_NEOPIXEL_ORDER_GRB ? true : false;
#else
    s_rgb_order_grb = true;
#endif
    if (jerry_value_is_string(order_val)) {
        jerry_size_t order_size = jerry_string_size(order_val, JERRY_ENCODING_UTF8);
        if (order_size > 0 && order_size < 8) {
            char order_buf[8] = {0};
            jerry_string_to_buffer(order_val, JERRY_ENCODING_UTF8, (jerry_char_t *)order_buf, order_size);
            s_rgb_order_grb = (strcasecmp(order_buf, "RGB") != 0);
        }
    }

    jerry_value_free(pin_val);
    jerry_value_free(len_val);
    jerry_value_free(order_val);

    neopixel_init(pin, length);

    if (!neopixel_is_ready()) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel.init failed");
    }

    return jerry_undefined();
}

static jerry_value_t neopixel_set_pixel_handler(const jerry_call_info_t *call_info_p,
                                                const jerry_value_t args[],
                                                const jerry_length_t argc) {
    (void)call_info_p;

    if (argc < 4) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "neopixel.setPixel requires index and RGB values");
    }

    if (!neopixel_is_ready()) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel is not initialized");
    }

    uint32_t index = (uint32_t)jerry_value_as_number(args[0]);
    uint8_t r = (uint8_t)jerry_value_as_number(args[1]);
    uint8_t g = (uint8_t)jerry_value_as_number(args[2]);
    uint8_t b = (uint8_t)jerry_value_as_number(args[3]);

    if (index >= neopixel_length()) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "neopixel index out of range");
    }

    neopixel_set_pixel(index, r, g, b);
    return jerry_undefined();
}

static jerry_value_t neopixel_show_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;

    if (!neopixel_is_ready()) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel is not initialized");
    }

    neopixel_show();
    return jerry_undefined();
}

static jerry_value_t neopixel_clear_handler(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args[],
                                            const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;

    if (!neopixel_is_ready()) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel is not initialized");
    }

    neopixel_clear();
    neopixel_show();
    return jerry_undefined();
}

jerry_value_t js_create_neopixel_module(void) {
    jerry_value_t module = jerry_object();

    js_set_function(module, "init", neopixel_init_handler);
    js_set_function(module, "setPixel", neopixel_set_pixel_handler);
    js_set_function(module, "show", neopixel_show_handler);
    js_set_function(module, "clear", neopixel_clear_handler);

    return module;
}

void js_bind_neopixel(void) {
    jerry_value_t module = js_create_neopixel_module();
    js_register_global("neopixel", module);
    jerry_value_free(module);
}
