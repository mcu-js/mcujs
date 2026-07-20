#include "neopixel.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "validation.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "neopixel.pio.h"
#include "pico/stdlib.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef MCUJS_NEOPIXEL_DEFAULT_FREQ
#define MCUJS_NEOPIXEL_DEFAULT_FREQ 800000
#endif

#define MCUJS_NEOPIXEL_MAX_LENGTH 256

static PIO s_pio = pio0;
static uint s_sm = 0;
static uint s_offset = 0;
static bool s_program_loaded = false;
static bool s_rgb_order_grb = true;
static bool s_ready = false;
static uint32_t s_length = 0;
static int s_pin = -1;
static uint32_t s_buffer_len = 0;
static uint32_t *s_pixels = NULL;

static jerry_value_t get_property(jerry_value_t object, const char *name) {
    jerry_value_t key = jerry_string_sz(name);
    jerry_value_t value = jerry_object_get(object, key);
    jerry_value_free(key);
    return value;
}

static jerry_value_t throw_neopixel_error(mcujs_operational_error_t error,
                                          int pin, int native_code,
                                          const char *message) {
    const mcujs_error_details_t details = {
        .resource = "neopixel",
        .has_pin = pin >= 0,
        .pin = pin,
        .has_native_code = native_code >= 0,
        .native_code = native_code,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static void neopixel_cleanup(void) {
    if (s_ready) pio_sm_set_enabled(s_pio, s_sm, false);
    if (s_pin >= 0) {
        gpio_set_function((uint)s_pin, GPIO_FUNC_SIO);
        gpio_init((uint)s_pin);
        mcujs_rp2_pin_release(s_pin, MCUJS_RP2_PIN_OWNER_NEOPIXEL);
        s_pin = -1;
    }
    if (s_pixels != NULL) {
        free(s_pixels);
        s_pixels = NULL;
    }
    s_buffer_len = 0;
    s_length = 0;
    s_ready = false;
}

bool neopixel_init(uint32_t pin, uint32_t length) {
    if (pin > INT32_MAX || !mcujs_rp2_neopixel_pin_allowed((int)pin) ||
        length < 1 || length > MCUJS_NEOPIXEL_MAX_LENGTH ||
        !mcujs_rp2_pin_can_claim((int)pin,
                                 MCUJS_RP2_PIN_OWNER_NEOPIXEL)) {
        return false;
    }

    neopixel_cleanup();
    if (!mcujs_rp2_pin_claim((int)pin, MCUJS_RP2_PIN_OWNER_NEOPIXEL)) {
        return false;
    }
    s_pin = (int)pin;
    s_length = length;
    s_buffer_len = length;
    s_pixels = (uint32_t *)calloc(s_buffer_len, sizeof(uint32_t));
    if (s_pixels == NULL) {
        neopixel_cleanup();
        return false;
    }

    if (!s_program_loaded) {
        if (!pio_can_add_program(s_pio, &mcujs_ws2812_program)) {
            neopixel_cleanup();
            return false;
        }
        s_offset = pio_add_program(s_pio, &mcujs_ws2812_program);
        s_program_loaded = true;
    }

    mcujs_ws2812_program_init(s_pio, s_sm, s_offset, pin,
                              MCUJS_NEOPIXEL_DEFAULT_FREQ, false);
    s_ready = true;
    return true;
}

void neopixel_set_order(bool grb) { s_rgb_order_grb = grb; }
bool neopixel_is_grb(void) { return s_rgb_order_grb; }
bool neopixel_is_ready(void) { return s_ready; }
uint32_t neopixel_length(void) { return s_length; }
int neopixel_pin(void) { return s_pin; }

void neopixel_set_pixel_ordered(uint32_t index, uint8_t r, uint8_t g, uint8_t b,
                                 bool grb) {
    if (!s_ready || index >= s_length || s_pixels == NULL) return;
    uint32_t color = grb
        ? ((uint32_t)g << 16) | ((uint32_t)r << 8) | b
        : ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    s_pixels[index] = color;
}

void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
    neopixel_set_pixel_ordered(index, r, g, b, s_rgb_order_grb);
}

void neopixel_clear(void) {
    if (!s_ready || s_pixels == NULL) return;
    memset(s_pixels, 0, s_buffer_len * sizeof(uint32_t));
}

void neopixel_show(void) {
    if (!s_ready || s_pixels == NULL) return;
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
        return jerry_throw_sz(JERRY_ERROR_TYPE,
                              "neopixel.init requires an options object");
    }

    jerry_value_t pin_value = get_property(args[0], "pin");
    if (jerry_value_is_exception(pin_value)) return pin_value;
    int pin;
    mcujs_arg_status_t pin_status = mcujs_value_to_integer(pin_value, &pin);
    jerry_value_free(pin_value);
    if (pin_status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(pin_status,
                              "neopixel pin must be a finite number",
                              "neopixel pin must be an integer");
    }
    if (!mcujs_rp2_neopixel_pin_allowed(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "NeoPixel pin is not available on this board");
    }

    jerry_value_t length_value = get_property(args[0], "length");
    if (jerry_value_is_exception(length_value)) return length_value;
    int length;
    mcujs_arg_status_t length_status =
        mcujs_value_to_integer(length_value, &length);
    jerry_value_free(length_value);
    if (length_status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(length_status,
                              "neopixel length must be a finite number",
                              "neopixel length must be an integer");
    }
    if (length < 1 || length > MCUJS_NEOPIXEL_MAX_LENGTH) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "neopixel length must be 1..256");
    }

    jerry_value_t order_value = get_property(args[0], "order");
    if (jerry_value_is_exception(order_value)) return order_value;

#ifdef MCUJS_NEOPIXEL_ORDER_GRB
    bool grb = MCUJS_NEOPIXEL_ORDER_GRB ? true : false;
#else
    bool grb = true;
#endif
    if (!jerry_value_is_undefined(order_value)) {
        if (!jerry_value_is_string(order_value)) {
            jerry_value_free(order_value);
            return jerry_throw_sz(JERRY_ERROR_TYPE,
                                  "neopixel order must be RGB or GRB");
        }
        jerry_size_t size =
            jerry_string_size(order_value, JERRY_ENCODING_UTF8);
        if (size != 3) {
            jerry_value_free(order_value);
            return jerry_throw_sz(JERRY_ERROR_RANGE,
                                  "neopixel order must be RGB or GRB");
        }
        char order[4] = {0};
        jerry_string_to_buffer(order_value, JERRY_ENCODING_UTF8,
                               (jerry_char_t *)order, size);
        if (strcasecmp(order, "RGB") == 0) {
            grb = false;
        } else if (strcasecmp(order, "GRB") == 0) {
            grb = true;
        } else {
            jerry_value_free(order_value);
            return jerry_throw_sz(JERRY_ERROR_RANGE,
                                  "neopixel order must be RGB or GRB");
        }
    }
    jerry_value_free(order_value);

    if (!mcujs_rp2_pin_can_claim(pin, MCUJS_RP2_PIN_OWNER_NEOPIXEL)) {
        return throw_neopixel_error(
            MCUJS_ERROR_BUSY, pin, -1,
            "NeoPixel pin is owned by another peripheral");
    }
    if (!neopixel_init((uint32_t)pin, (uint32_t)length)) {
        return throw_neopixel_error(MCUJS_ERROR_IO, pin, 0,
                                    "neopixel initialization failed");
    }
    neopixel_set_order(grb);
    return jerry_undefined();
}

static jerry_value_t neopixel_set_pixel_handler(
    const jerry_call_info_t *call_info_p, const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    if (argc < 4) {
        return jerry_throw_sz(
            JERRY_ERROR_TYPE,
            "neopixel.setPixel requires index and RGB values");
    }
    if (!neopixel_is_ready()) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, -1, -1,
                                    "neopixel is not initialized");
    }

    int index;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status,
                              "neopixel index must be a finite number",
                              "neopixel index must be an integer");
    }
    if (index < 0 || (uint32_t)index >= neopixel_length()) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "neopixel index out of range");
    }

    uint8_t colors[3];
    for (jerry_length_t i = 0; i < 3; i++) {
        status = mcujs_value_to_byte(args[i + 1], &colors[i]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status,
                                  "neopixel colors must be finite numbers",
                                  "neopixel colors must be integers 0..255");
        }
    }
    neopixel_set_pixel((uint32_t)index, colors[0], colors[1], colors[2]);
    return jerry_undefined();
}

static jerry_value_t neopixel_show_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    if (!neopixel_is_ready()) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, -1, -1,
                                    "neopixel is not initialized");
    }
    neopixel_show();
    return jerry_undefined();
}

static jerry_value_t neopixel_clear_handler(
    const jerry_call_info_t *call_info_p, const jerry_value_t args[],
    const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;
    if (!neopixel_is_ready()) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, -1, -1,
                                    "neopixel is not initialized");
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
