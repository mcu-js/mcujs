/* MCU.js external NeoPixel/WS2812 binding for ESP32-S3 RMT. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "neopixel_options.h"
#include "pin_policy.h"
#include "runtime_features.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

#include <stdbool.h>
#include <stdint.h>

#define MCUJS_NEOPIXEL_RMT_RESOLUTION_HZ 10000000u

static led_strip_handle_t s_strip;
static int s_pin = -1;
static uint32_t s_length;
static bool s_pin_claimed;

static mcujs_operational_error_t map_neopixel_error(esp_err_t error) {
    if (error == ESP_ERR_INVALID_STATE || error == ESP_ERR_TIMEOUT) {
        return MCUJS_ERROR_BUSY;
    }
    if (error == ESP_ERR_NO_MEM) return MCUJS_ERROR_RESOURCE_EXHAUSTED;
    if (error == ESP_ERR_NOT_SUPPORTED) return MCUJS_ERROR_NOT_SUPPORTED;
    return MCUJS_ERROR_IO;
}

static jerry_value_t throw_neopixel_error(mcujs_operational_error_t error,
                                          esp_err_t native_error, int pin,
                                          const char *message) {
    const mcujs_error_details_t details = {
        .resource = "neopixel",
        .has_pin = pin >= 0,
        .pin = pin,
        .has_limit = error == MCUJS_ERROR_RESOURCE_EXHAUSTED,
        .limit = MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH,
        .has_native_code = native_error != ESP_OK,
        .native_code = native_error,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static jerry_value_t throw_neopixel_native(esp_err_t error, int pin,
                                           const char *message) {
    return throw_neopixel_error(map_neopixel_error(error), error, pin,
                                message);
}

static esp_err_t release_strip(void) {
    if (s_strip != NULL) {
        esp_err_t err = led_strip_clear(s_strip);
        if (err != ESP_OK) return err;
        err = led_strip_del(s_strip);
        if (err != ESP_OK) return err;
        s_strip = NULL;
        s_length = 0;
    }
    if (!s_pin_claimed) return ESP_OK;

    esp_err_t err = gpio_reset_pin((gpio_num_t)s_pin);
    if (err != ESP_OK) return err;
    mcujs_pin_release(s_pin, MCUJS_PIN_OWNER_NEOPIXEL);
    s_pin_claimed = false;
    s_pin = -1;
    return ESP_OK;
}

static jerry_value_t neopixel_init_handler(const jerry_call_info_t *info,
                                           const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    mcujs_neopixel_init_options_t options;
    jerry_value_t parsed = mcujs_parse_neopixel_init_args(args, argc, &options);
    if (jerry_value_is_exception(parsed)) return parsed;
    jerry_value_free(parsed);

    if (!mcujs_pin_is_peripheral_output(options.pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "NeoPixel pin is not an external output pin");
    }
    if (!mcujs_pin_can_claim(options.pin, MCUJS_PIN_OWNER_NEOPIXEL)) {
        return throw_neopixel_error(
            MCUJS_ERROR_BUSY, ESP_OK, options.pin,
            "NeoPixel pin is owned by another peripheral");
    }
    esp_err_t err = release_strip();
    if (err != ESP_OK) {
        return throw_neopixel_native(err, s_pin,
                                     "NeoPixel reinitialization failed");
    }
    if (!mcujs_pin_claim(options.pin, MCUJS_PIN_OWNER_NEOPIXEL)) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, ESP_OK, options.pin,
                                    "NeoPixel pin claim failed");
    }
    s_pin = options.pin;
    s_pin_claimed = true;

    led_strip_config_t strip_config = {
        .strip_gpio_num = options.pin,
        .max_leds = (uint32_t)options.length,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = options.grb
                                      ? LED_STRIP_COLOR_COMPONENT_FMT_GRB
                                      : LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .flags = {.invert_out = 0},
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = MCUJS_NEOPIXEL_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 0,
        .flags = {.with_dma = 0},
    };
    led_strip_handle_t new_strip = NULL;
    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &new_strip);
    if (err != ESP_OK) {
        esp_err_t reset_error = gpio_reset_pin((gpio_num_t)options.pin);
        if (reset_error != ESP_OK) {
            return throw_neopixel_native(
                reset_error, options.pin,
                "NeoPixel initialization rollback failed");
        }
        mcujs_pin_release(options.pin, MCUJS_PIN_OWNER_NEOPIXEL);
        s_pin_claimed = false;
        s_pin = -1;
        return throw_neopixel_native(err, options.pin,
                                     "NeoPixel initialization failed");
    }
    s_strip = new_strip;
    s_length = (uint32_t)options.length;
    return jerry_undefined();
}

static jerry_value_t neopixel_set_pixel_handler(const jerry_call_info_t *info,
                                                const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (argc < 4) {
        return jerry_throw_sz(
            JERRY_ERROR_TYPE,
            "neopixel.setPixel requires index and RGB values");
    }
    int index;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "neopixel index must be a finite number",
                              "neopixel index must be an integer");
    }
    if (index < 0) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "neopixel index out of range");
    }
    uint8_t colors[3];
    for (jerry_length_t i = 0; i < 3; i++) {
        status = mcujs_value_to_byte(args[i + 1], &colors[i]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status, "neopixel colors must be finite numbers",
                                  "neopixel colors must be integers 0..255");
        }
    }
    if (s_strip == NULL) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, ESP_OK, -1,
                                    "NeoPixel is not initialized");
    }
    if ((uint32_t)index >= s_length) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "neopixel index out of range");
    }
    esp_err_t err = led_strip_set_pixel(
        s_strip, (uint32_t)index, colors[0], colors[1], colors[2]);
    if (err != ESP_OK) {
        return throw_neopixel_error(MCUJS_ERROR_IO, err, s_pin,
                                    "NeoPixel pixel update failed");
    }
    return jerry_undefined();
}

static jerry_value_t neopixel_show_handler(const jerry_call_info_t *info,
                                           const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    (void)args;
    (void)argc;
    if (s_strip == NULL) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, ESP_OK, -1,
                                    "NeoPixel is not initialized");
    }
    esp_err_t err = led_strip_refresh(s_strip);
    if (err != ESP_OK) {
        return throw_neopixel_native(err, s_pin, "NeoPixel refresh failed");
    }
    return jerry_undefined();
}

static jerry_value_t neopixel_clear_handler(const jerry_call_info_t *info,
                                            const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    (void)args;
    (void)argc;
    if (s_strip == NULL) {
        return throw_neopixel_error(MCUJS_ERROR_BUSY, ESP_OK, -1,
                                    "NeoPixel is not initialized");
    }
    esp_err_t err = led_strip_clear(s_strip);
    if (err != ESP_OK) {
        mcujs_operational_error_t mapped =
            (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_TIMEOUT)
                ? MCUJS_ERROR_BUSY
                : MCUJS_ERROR_IO;
        return throw_neopixel_error(mapped, err, s_pin,
                                    "NeoPixel clear failed");
    }
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
