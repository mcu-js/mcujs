/*
 * mcujs - GPIO Bindings
 * 
 * Implements: GPIO.init(), GPIO.set(), GPIO.get(), GPIO.toggle()
 * Constants: GPIO.OUTPUT, GPIO.INPUT, GPIO.INPUT_PULLUP, GPIO.INPUT_PULLDOWN
 */

#include "bindings.h"
#include "gpio_internal.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "validation.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_set_number(jerry_value_t object, const char *name, double value);
extern void js_register_global(const char *name, jerry_value_t object);


/* GPIO mode constants */
#define GPIO_MODE_OUTPUT        0
#define GPIO_MODE_INPUT         1
#define GPIO_MODE_INPUT_PULLUP  2
#define GPIO_MODE_INPUT_PULLDOWN 3

static bool s_gpio_initialized[NUM_BANK0_GPIOS];
static bool s_gpio_output[NUM_BANK0_GPIOS];

bool mcujs_rp2_gpio_prepare_soft_output(int pin) {
    if (!mcujs_rp2_gpio_output_pin_allowed(pin)) return false;
    mcujs_rp2_pin_owner_t owner = mcujs_rp2_pin_owner(pin);
    if (owner != MCUJS_RP2_PIN_OWNER_NONE &&
        owner != MCUJS_RP2_PIN_OWNER_GPIO) {
        return false;
    }
    if (!mcujs_rp2_pin_claim(pin, MCUJS_RP2_PIN_OWNER_GPIO)) return false;
    if (owner != MCUJS_RP2_PIN_OWNER_GPIO || !s_gpio_initialized[pin] ||
        !s_gpio_output[pin]) {
        gpio_init((uint)pin);
        gpio_set_dir((uint)pin, GPIO_OUT);
        s_gpio_initialized[pin] = true;
        s_gpio_output[pin] = true;
    }
    return true;
}

static jerry_value_t throw_gpio_busy(int pin, const char *message) {
    const mcujs_error_details_t details = {
        .resource = "gpio",
        .has_pin = true,
        .pin = pin,
    };
    return mcujs_throw_operational_error(MCUJS_ERROR_BUSY, message, &details);
}

static jerry_value_t require_gpio_pin(const jerry_value_t args[],
                                      jerry_length_t argc, int *pin,
                                      bool output) {
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "GPIO pin must be a finite number",
                              "GPIO pin must be an integer");
    }
    if (!(output ? mcujs_rp2_gpio_output_pin_allowed(*pin)
                 : mcujs_rp2_gpio_pin_allowed(*pin))) {
        return jerry_throw_sz(
            JERRY_ERROR_RANGE,
            output ? "GPIO output pin is not available on this board"
                   : "GPIO pin is not available on this board");
    }
    if (mcujs_rp2_pin_owner(*pin) != MCUJS_RP2_PIN_OWNER_GPIO) {
        return throw_gpio_busy(*pin, "GPIO pin is not initialized or is busy");
    }
    if (!s_gpio_initialized[*pin] || (output && !s_gpio_output[*pin])) {
        return throw_gpio_busy(*pin,
                               "GPIO pin is not initialized for this operation");
    }
    return jerry_undefined();
}

/*
 * GPIO.init(pin, mode)
 * Initialize a GPIO pin
 */
static jerry_value_t gpio_init_handler(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args[],
                                        const jerry_length_t argc) {
    (void)call_info_p;

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
    if (mode < GPIO_MODE_OUTPUT || mode > GPIO_MODE_INPUT_PULLDOWN) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO mode");
    }
    bool output = mode == GPIO_MODE_OUTPUT;
    if (!(output ? mcujs_rp2_gpio_output_pin_allowed(pin)
                 : mcujs_rp2_gpio_pin_allowed(pin))) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "GPIO pin is not available in this mode");
    }
    if (!mcujs_rp2_pin_claim(pin, MCUJS_RP2_PIN_OWNER_GPIO)) {
        return throw_gpio_busy(pin, "GPIO pin is owned by another peripheral");
    }

    gpio_init((uint)pin);
    switch (mode) {
        case GPIO_MODE_OUTPUT:
            gpio_set_dir((uint)pin, GPIO_OUT);
            break;
        case GPIO_MODE_INPUT:
            gpio_set_dir((uint)pin, GPIO_IN);
            gpio_disable_pulls((uint)pin);
            break;
        case GPIO_MODE_INPUT_PULLUP:
            gpio_set_dir((uint)pin, GPIO_IN);
            gpio_pull_up((uint)pin);
            break;
        case GPIO_MODE_INPUT_PULLDOWN:
            gpio_set_dir((uint)pin, GPIO_IN);
            gpio_pull_down((uint)pin);
            break;
    }

    s_gpio_initialized[pin] = true;
    s_gpio_output[pin] = output;

    return jerry_undefined();
}

/*
 * GPIO.set(pin, value)
 * Set GPIO output value
 */
static jerry_value_t gpio_set_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;

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

    gpio_put((uint)pin, value);
    return jerry_undefined();
}

/*
 * GPIO.get(pin)
 * Read GPIO input value
 */
static jerry_value_t gpio_get_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;

    int pin;
    jerry_value_t validation = require_gpio_pin(args, argc, &pin, false);
    if (jerry_value_is_exception(validation)) return validation;
    jerry_value_free(validation);

    return jerry_boolean(gpio_get((uint)pin));
}

/*
 * GPIO.toggle(pin)
 * Toggle GPIO output
 */
static jerry_value_t gpio_toggle_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;

    int pin;
    jerry_value_t validation = require_gpio_pin(args, argc, &pin, true);
    if (jerry_value_is_exception(validation)) return validation;
    jerry_value_free(validation);

    bool current = gpio_get((uint)pin);
    gpio_put((uint)pin, !current);
    return jerry_undefined();
}

/*
 * Create GPIO module object
 */
jerry_value_t js_create_gpio_module(void) {
    jerry_value_t gpio = jerry_object();

    /* Methods */
    js_set_function(gpio, "init", gpio_init_handler);
    js_set_function(gpio, "set", gpio_set_handler);
    js_set_function(gpio, "get", gpio_get_handler);
    js_set_function(gpio, "toggle", gpio_toggle_handler);

    /* Constants */
    js_set_number(gpio, "OUTPUT", GPIO_MODE_OUTPUT);
    js_set_number(gpio, "INPUT", GPIO_MODE_INPUT);
    js_set_number(gpio, "INPUT_PULLUP", GPIO_MODE_INPUT_PULLUP);
    js_set_number(gpio, "INPUT_PULLDOWN", GPIO_MODE_INPUT_PULLDOWN);

    return gpio;
}

/*
 * Register GPIO bindings
 */
void js_bind_gpio(void) {
    jerry_value_t gpio = js_create_gpio_module();
    js_register_global("GPIO", gpio);
    jerry_value_free(gpio);
}
