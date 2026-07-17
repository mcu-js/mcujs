/* MCU.js GPIO binding for ESP32-S3. */

#include "bindings.h"
#include "jerryscript.h"

#include "driver/gpio.h"

#define MCUJS_GPIO_OUTPUT 0
#define MCUJS_GPIO_INPUT 1
#define MCUJS_GPIO_INPUT_PULLUP 2
#define MCUJS_GPIO_INPUT_PULLDOWN 3

static bool valid_pin(int pin) {
    if (!GPIO_IS_VALID_GPIO((gpio_num_t)pin)) {
        return false;
    }

    /* XIAO D0-D10 plus the onboard LED. Do not expose flash/PSRAM pins. */
    switch (pin) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 21:
        case 43:
        case 44:
            return true;
        default:
            return false;
    }
}

static jerry_value_t gpio_init_handler(const jerry_call_info_t *info,
                                       const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.init requires pin and mode");
    }
    int pin = (int)js_get_number_arg(args, argc, 0, -1);
    int mode = (int)js_get_number_arg(args, argc, 1, MCUJS_GPIO_OUTPUT);
    if (!valid_pin(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    if (mode == MCUJS_GPIO_OUTPUT && !GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "GPIO is not output-capable");
    }

    gpio_reset_pin((gpio_num_t)pin);
    switch (mode) {
        case MCUJS_GPIO_OUTPUT:
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT);
            break;
        case MCUJS_GPIO_INPUT:
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
            break;
        case MCUJS_GPIO_INPUT_PULLUP:
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
            break;
        case MCUJS_GPIO_INPUT_PULLDOWN:
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY);
            break;
        default:
            return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO mode");
    }
    return jerry_undefined();
}

static jerry_value_t gpio_set_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.set requires pin and value");
    }
    int pin = (int)js_get_number_arg(args, argc, 0, -1);
    if (!valid_pin(pin) || !GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO output pin");
    }
    gpio_set_level((gpio_num_t)pin, js_get_boolean_arg(args, argc, 1, false));
    return jerry_undefined();
}

static jerry_value_t gpio_get_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.get requires pin");
    }
    int pin = (int)js_get_number_arg(args, argc, 0, -1);
    if (!valid_pin(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    return jerry_boolean(gpio_get_level((gpio_num_t)pin) != 0);
}

static jerry_value_t gpio_toggle_handler(const jerry_call_info_t *info,
                                         const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.toggle requires pin");
    }
    int pin = (int)js_get_number_arg(args, argc, 0, -1);
    if (!valid_pin(pin) || !GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO output pin");
    }
    gpio_set_level((gpio_num_t)pin, !gpio_get_level((gpio_num_t)pin));
    return jerry_undefined();
}

jerry_value_t js_create_gpio_module(void) {
    jerry_value_t gpio = jerry_object();
    js_set_function(gpio, "init", gpio_init_handler);
    js_set_function(gpio, "set", gpio_set_handler);
    js_set_function(gpio, "get", gpio_get_handler);
    js_set_function(gpio, "toggle", gpio_toggle_handler);
    js_set_number(gpio, "OUTPUT", MCUJS_GPIO_OUTPUT);
    js_set_number(gpio, "INPUT", MCUJS_GPIO_INPUT);
    js_set_number(gpio, "INPUT_PULLUP", MCUJS_GPIO_INPUT_PULLUP);
    js_set_number(gpio, "INPUT_PULLDOWN", MCUJS_GPIO_INPUT_PULLDOWN);
    return gpio;
}

void js_bind_gpio(void) {
    jerry_value_t gpio = js_create_gpio_module();
    js_register_global("GPIO", gpio);
    jerry_value_free(gpio);
}
