/* MCU.js external NeoPixel/WS2812 binding for ESP32-S3 RMT. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "led_strip.h"
#include "led_strip_rmt.h"

#include <stdbool.h>
#include <stdint.h>
#include <strings.h>

#define MCUJS_NEOPIXEL_MAX_LENGTH 256
#define MCUJS_NEOPIXEL_RMT_RESOLUTION_HZ 10000000u

static led_strip_handle_t s_strip;
static int s_pin = -1;
static uint32_t s_length;

static jerry_value_t get_property(jerry_value_t object, const char *name) {
    jerry_value_t key = jerry_string_sz(name);
    jerry_value_t value = jerry_object_get(object, key);
    jerry_value_free(key);
    return value;
}

static esp_err_t release_strip(void) {
    if (s_strip == NULL) return ESP_OK;
    esp_err_t err = led_strip_clear(s_strip);
    if (err == ESP_OK) err = led_strip_del(s_strip);
    if (err != ESP_OK) return err;
    mcujs_pin_release(s_pin, MCUJS_PIN_OWNER_NEOPIXEL);
    (void)gpio_reset_pin((gpio_num_t)s_pin);
    s_strip = NULL;
    s_pin = -1;
    s_length = 0;
    return ESP_OK;
}

static jerry_value_t neopixel_init_handler(const jerry_call_info_t *info,
                                           const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (argc < 1 || !jerry_value_is_object(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "neopixel.init requires an options object");
    }

    jerry_value_t pin_value = get_property(args[0], "pin");
    jerry_value_t length_value = get_property(args[0], "length");
    jerry_value_t order_value = get_property(args[0], "order");
    int pin;
    int length;
    mcujs_arg_status_t pin_status = mcujs_value_to_integer(pin_value, &pin);
    mcujs_arg_status_t length_status = mcujs_value_to_integer(length_value, &length);
    jerry_value_free(pin_value);
    jerry_value_free(length_value);
    if (pin_status != MCUJS_ARG_OK) {
        jerry_value_free(order_value);
        return mcujs_throw_arg(pin_status, "neopixel pin must be a finite number",
                              "neopixel pin must be an integer");
    }
    if (length_status != MCUJS_ARG_OK) {
        jerry_value_free(order_value);
        return mcujs_throw_arg(length_status, "neopixel length must be a finite number",
                              "neopixel length must be an integer");
    }
    if (!mcujs_pin_is_peripheral_output(pin)) {
        jerry_value_free(order_value);
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid NeoPixel pin (use GPIO1..GPIO9)");
    }
    if (length < 1 || length > MCUJS_NEOPIXEL_MAX_LENGTH) {
        jerry_value_free(order_value);
        return jerry_throw_sz(JERRY_ERROR_RANGE, "neopixel length must be 1..256");
    }

    bool rgb_order = false;
    if (!jerry_value_is_undefined(order_value)) {
        if (!jerry_value_is_string(order_value)) {
            jerry_value_free(order_value);
            return jerry_throw_sz(JERRY_ERROR_TYPE, "neopixel order must be RGB or GRB");
        }
        jerry_size_t size = jerry_string_size(order_value, JERRY_ENCODING_UTF8);
        if (size != 3) {
            jerry_value_free(order_value);
            return jerry_throw_sz(JERRY_ERROR_RANGE, "neopixel order must be RGB or GRB");
        }
        char order[4] = {0};
        jerry_string_to_buffer(order_value, JERRY_ENCODING_UTF8, (jerry_char_t *)order, size);
        if (strcasecmp(order, "RGB") == 0) {
            rgb_order = true;
        } else if (strcasecmp(order, "GRB") != 0) {
            jerry_value_free(order_value);
            return jerry_throw_sz(JERRY_ERROR_RANGE, "neopixel order must be RGB or GRB");
        }
    }
    jerry_value_free(order_value);

    if (!mcujs_pin_can_claim(pin, MCUJS_PIN_OWNER_NEOPIXEL)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON,
                              "NeoPixel pin is owned by another peripheral");
    }
    if (release_strip() != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel reinitialization failed");
    }
    if (!mcujs_pin_claim(pin, MCUJS_PIN_OWNER_NEOPIXEL)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel pin claim failed");
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = pin,
        .max_leds = (uint32_t)length,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = rgb_order ? LED_STRIP_COLOR_COMPONENT_FMT_RGB
                                            : LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = 0},
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = MCUJS_NEOPIXEL_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 0,
        .flags = {.with_dma = 0},
    };
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (err != ESP_OK) {
        s_strip = NULL;
        (void)gpio_reset_pin((gpio_num_t)pin);
        mcujs_pin_release(pin, MCUJS_PIN_OWNER_NEOPIXEL);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel initialization failed");
    }
    s_pin = pin;
    s_length = (uint32_t)length;
    return jerry_undefined();
}

static jerry_value_t neopixel_set_pixel_handler(const jerry_call_info_t *info,
                                                const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (s_strip == NULL) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel is not initialized");
    }
    int index;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &index);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "neopixel index must be a finite number",
                              "neopixel index must be an integer");
    }
    if (index < 0 || (uint32_t)index >= s_length) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "neopixel index out of range");
    }
    uint8_t colors[3];
    for (jerry_length_t i = 0; i < 3; i++) {
        if (i + 1 >= argc) {
            return jerry_throw_sz(JERRY_ERROR_TYPE,
                                  "neopixel.setPixel requires index and RGB values");
        }
        status = mcujs_value_to_byte(args[i + 1], &colors[i]);
        if (status != MCUJS_ARG_OK) {
            return mcujs_throw_arg(status, "neopixel colors must be finite numbers",
                                  "neopixel colors must be integers 0..255");
        }
    }
    if (led_strip_set_pixel(s_strip, (uint32_t)index, colors[0], colors[1], colors[2]) !=
        ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel pixel update failed");
    }
    return jerry_undefined();
}

static jerry_value_t neopixel_show_handler(const jerry_call_info_t *info,
                                           const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    (void)args;
    (void)argc;
    if (s_strip == NULL) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel is not initialized");
    }
    if (led_strip_refresh(s_strip) != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel refresh failed");
    }
    return jerry_undefined();
}

static jerry_value_t neopixel_clear_handler(const jerry_call_info_t *info,
                                            const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    (void)args;
    (void)argc;
    if (s_strip == NULL) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel is not initialized");
    }
    if (led_strip_clear(s_strip) != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "neopixel clear failed");
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
