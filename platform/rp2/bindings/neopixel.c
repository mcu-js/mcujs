#include "neopixel.h"
#include "bindings.h"
#include "jerryscript.h"
#include "neopixel_options.h"
#include "pin_policy.h"
#include "validation.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "neopixel.pio.h"
#include "pico/stdlib.h"

#include <stdlib.h>
#include <string.h>

#ifndef MCUJS_NEOPIXEL_DEFAULT_FREQ
#define MCUJS_NEOPIXEL_DEFAULT_FREQ 800000
#endif

static PIO s_pio = pio0;
static int s_sm = -1;
static bool s_sm_claimed = false;
static uint s_offset = 0;
static bool s_program_loaded = false;
static bool s_rgb_order_grb = true;
static bool s_ready = false;
static uint32_t s_length = 0;
static int s_pin = -1;
static uint32_t s_buffer_len = 0;
static uint32_t *s_pixels = NULL;
static neopixel_init_failure_t s_last_init_failure =
    NEOPIXEL_INIT_FAILURE_NONE;

static jerry_value_t throw_neopixel_error(mcujs_operational_error_t error,
                                          int pin, int native_code, int limit,
                                          const char *message) {
    const mcujs_error_details_t details = {
        .resource = "neopixel",
        .has_pin = pin >= 0,
        .pin = pin,
        .has_limit = limit > 0,
        .limit = limit,
        .has_native_code = native_code >= 0,
        .native_code = native_code,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static void neopixel_cleanup(bool release_state_machine) {
    if (s_ready) pio_sm_set_enabled(s_pio, (uint)s_sm, false);
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
    if (release_state_machine && s_sm_claimed) {
        pio_sm_unclaim(s_pio, (uint)s_sm);
        s_sm = -1;
        s_sm_claimed = false;
    }
}

bool neopixel_init(uint32_t pin, uint32_t length) {
    s_last_init_failure = NEOPIXEL_INIT_FAILURE_NONE;
    if (pin > INT32_MAX || !mcujs_rp2_neopixel_pin_allowed((int)pin) ||
        length < 1 || length > MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH) {
        s_last_init_failure = NEOPIXEL_INIT_FAILURE_IO;
        return false;
    }
    if (!mcujs_rp2_pin_can_claim((int)pin,
                                 MCUJS_RP2_PIN_OWNER_NEOPIXEL)) {
        s_last_init_failure = NEOPIXEL_INIT_FAILURE_BUSY;
        return false;
    }

    neopixel_cleanup(false);
    if (!s_sm_claimed) {
        int state_machine = pio_claim_unused_sm(s_pio, false);
        if (state_machine < 0) {
            s_last_init_failure = NEOPIXEL_INIT_FAILURE_RESOURCE_EXHAUSTED;
            return false;
        }
        s_sm = state_machine;
        s_sm_claimed = true;
    }
    if (!mcujs_rp2_pin_claim((int)pin, MCUJS_RP2_PIN_OWNER_NEOPIXEL)) {
        s_last_init_failure = NEOPIXEL_INIT_FAILURE_BUSY;
        neopixel_cleanup(true);
        return false;
    }
    s_pin = (int)pin;
    s_length = length;
    s_buffer_len = length;
    s_pixels = (uint32_t *)calloc(s_buffer_len, sizeof(uint32_t));
    if (s_pixels == NULL) {
        s_last_init_failure = NEOPIXEL_INIT_FAILURE_RESOURCE_EXHAUSTED;
        neopixel_cleanup(true);
        return false;
    }

    if (!s_program_loaded) {
        if (!pio_can_add_program(s_pio, &mcujs_ws2812_program)) {
            s_last_init_failure = NEOPIXEL_INIT_FAILURE_RESOURCE_EXHAUSTED;
            neopixel_cleanup(true);
            return false;
        }
        s_offset = pio_add_program(s_pio, &mcujs_ws2812_program);
        s_program_loaded = true;
    }

    mcujs_ws2812_program_init(s_pio, (uint)s_sm, s_offset, pin,
                              MCUJS_NEOPIXEL_DEFAULT_FREQ, false);
    s_ready = true;
    return true;
}

neopixel_init_failure_t neopixel_last_init_failure(void) {
    return s_last_init_failure;
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
        pio_sm_put_blocking(s_pio, (uint)s_sm, s_pixels[i] << 8u);
    }
    sleep_us(80);
}

static jerry_value_t neopixel_init_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    mcujs_neopixel_init_options_t options;
    jerry_value_t parsed = mcujs_parse_neopixel_init_args(args, argc, &options);
    if (jerry_value_is_exception(parsed)) return parsed;
    jerry_value_free(parsed);

    if (!neopixel_init((uint32_t)options.pin, (uint32_t)options.length)) {
        neopixel_init_failure_t failure = neopixel_last_init_failure();
        if (failure == NEOPIXEL_INIT_FAILURE_BUSY) {
            return throw_neopixel_error(
                MCUJS_ERROR_BUSY, options.pin, -1, 0,
                "NeoPixel pin is owned by another peripheral");
        }
        if (failure == NEOPIXEL_INIT_FAILURE_RESOURCE_EXHAUSTED) {
            return throw_neopixel_error(
                MCUJS_ERROR_RESOURCE_EXHAUSTED, options.pin, -1,
                MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH,
                "NeoPixel waveform or pixel buffer is unavailable");
        }
        return throw_neopixel_error(MCUJS_ERROR_IO, options.pin, 0, 0,
                                    "NeoPixel initialization failed");
    }
    neopixel_set_order(options.grb);
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
    int index;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status,
                              "neopixel index must be a finite number",
                              "neopixel index must be an integer");
    }
    if (index < 0) {
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
    if (!neopixel_is_ready()) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, -1, -1, 0,
                                    "neopixel is not initialized");
    }
    if ((uint32_t)index >= neopixel_length()) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "neopixel index out of range");
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
        return throw_neopixel_error(MCUJS_ERROR_BUSY, -1, -1, 0,
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
        return throw_neopixel_error(MCUJS_ERROR_BUSY, -1, -1, 0,
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
