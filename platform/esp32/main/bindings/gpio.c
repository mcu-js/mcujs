/* MCU.js GPIO binding for ESP32-S3. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"

#include "driver/gpio.h"
#include "esp_err.h"

#define MCUJS_GPIO_OUTPUT 0
#define MCUJS_GPIO_INPUT 1
#define MCUJS_GPIO_INPUT_PULLUP 2
#define MCUJS_GPIO_INPUT_PULLDOWN 3

static bool s_gpio_initialized[GPIO_NUM_MAX];
static bool s_gpio_output[GPIO_NUM_MAX];

static jerry_value_t throw_gpio_busy(int pin, const char *message) {
    const mcujs_error_details_t details = {
        .resource = "gpio",
        .has_pin = true,
        .pin = pin,
    };
    return mcujs_throw_operational_error(MCUJS_ERROR_BUSY, message, &details);
}

static jerry_value_t throw_gpio_io(esp_err_t error, int pin,
                                   const char *message) {
    const mcujs_error_details_t details = {
        .resource = "gpio",
        .has_pin = true,
        .pin = pin,
        .has_native_code = true,
        .native_code = error,
    };
    return mcujs_throw_operational_error(MCUJS_ERROR_IO, message, &details);
}

static jerry_value_t gpio_init_handler(const jerry_call_info_t *info,
                                       const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int pin;
    int mode;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "GPIO pin must be a finite number",
                              "GPIO pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &mode);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "GPIO mode must be a finite number",
                              "GPIO mode must be an integer");
    }
    if (!mcujs_pin_is_exposed(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    if (mode < MCUJS_GPIO_OUTPUT || mode > MCUJS_GPIO_INPUT_PULLDOWN) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO mode");
    }
    if (mode == MCUJS_GPIO_OUTPUT && !mcujs_pin_is_exposed_output(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "GPIO is not output-capable");
    }
    if (!mcujs_pin_can_claim(pin, MCUJS_PIN_OWNER_GPIO)) {
        return throw_gpio_busy(pin, "GPIO pin is owned by another peripheral");
    }

    if (!mcujs_pin_claim(pin, MCUJS_PIN_OWNER_GPIO)) {
        return throw_gpio_busy(pin, "GPIO pin claim failed");
    }
    esp_err_t err = gpio_reset_pin((gpio_num_t)pin);
    if (err == ESP_OK) {
        switch (mode) {
            case MCUJS_GPIO_OUTPUT:
                err = gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT_OUTPUT);
                break;
            case MCUJS_GPIO_INPUT:
                err = gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
                if (err == ESP_OK) err = gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
                break;
            case MCUJS_GPIO_INPUT_PULLUP:
                err = gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
                if (err == ESP_OK) err = gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
                break;
            case MCUJS_GPIO_INPUT_PULLDOWN:
                err = gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
                if (err == ESP_OK) err = gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLDOWN_ONLY);
                break;
        }
    }
    if (err != ESP_OK) {
        s_gpio_initialized[pin] = false;
        s_gpio_output[pin] = false;
        (void)gpio_reset_pin((gpio_num_t)pin);
        mcujs_pin_release(pin, MCUJS_PIN_OWNER_GPIO);
        return throw_gpio_io(err, pin, "GPIO initialization failed");
    }
    s_gpio_initialized[pin] = true;
    s_gpio_output[pin] = mode == MCUJS_GPIO_OUTPUT;
    return jerry_undefined();
}

static jerry_value_t require_gpio_pin(const jerry_value_t args[], jerry_length_t argc,
                                      int *pin, bool output) {
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "GPIO pin must be a finite number",
                              "GPIO pin must be an integer");
    }
    if (!(output ? mcujs_pin_is_exposed_output(*pin) : mcujs_pin_is_exposed(*pin))) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              output ? "Invalid GPIO output pin" : "Invalid GPIO pin number");
    }
    if (mcujs_pin_owner(*pin) != MCUJS_PIN_OWNER_GPIO) {
        return throw_gpio_busy(*pin, "GPIO pin is not initialized or is busy");
    }
    if (!s_gpio_initialized[*pin] || (output && !s_gpio_output[*pin])) {
        return throw_gpio_busy(*pin,
                               "GPIO pin is not initialized for this operation");
    }
    return jerry_undefined();
}

static jerry_value_t gpio_set_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int pin;
    bool value;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "GPIO pin must be a finite number",
                              "GPIO pin must be an integer");
    }
    status = mcujs_get_boolean(args, argc, 1, &value);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "GPIO value must be boolean",
                              "GPIO value must be boolean");
    }
    jerry_value_t validation = require_gpio_pin(args, argc, &pin, true);
    if (jerry_value_is_exception(validation)) return validation;
    jerry_value_free(validation);
    esp_err_t error = gpio_set_level((gpio_num_t)pin, value);
    if (error != ESP_OK) {
        return throw_gpio_io(error, pin, "GPIO write failed");
    }
    return jerry_undefined();
}

static jerry_value_t gpio_get_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int pin;
    jerry_value_t validation = require_gpio_pin(args, argc, &pin, false);
    if (jerry_value_is_exception(validation)) return validation;
    jerry_value_free(validation);
    return jerry_boolean(gpio_get_level((gpio_num_t)pin) != 0);
}

static jerry_value_t gpio_toggle_handler(const jerry_call_info_t *info,
                                         const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int pin;
    jerry_value_t validation = require_gpio_pin(args, argc, &pin, true);
    if (jerry_value_is_exception(validation)) return validation;
    jerry_value_free(validation);
    esp_err_t error =
        gpio_set_level((gpio_num_t)pin, !gpio_get_level((gpio_num_t)pin));
    if (error != ESP_OK) {
        return throw_gpio_io(error, pin, "GPIO toggle failed");
    }
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
