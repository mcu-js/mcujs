/*
 * mcujs - GPIO Bindings
 * 
 * Implements: GPIO.init(), GPIO.set(), GPIO.get(), GPIO.toggle()
 * Constants: GPIO.OUTPUT, GPIO.INPUT, GPIO.INPUT_PULLUP, GPIO.INPUT_PULLDOWN
 */

#include "bindings.h"
#include "jerryscript.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_set_number(jerry_value_t object, const char *name, double value);
extern void js_register_global(const char *name, jerry_value_t object);
extern double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, double default_value);
extern bool js_get_boolean_arg(const jerry_value_t args[], jerry_length_t argc,
                               jerry_length_t index, bool default_value);

/* GPIO mode constants */
#define GPIO_MODE_OUTPUT        0
#define GPIO_MODE_INPUT         1
#define GPIO_MODE_INPUT_PULLUP  2
#define GPIO_MODE_INPUT_PULLDOWN 3

/*
 * GPIO.init(pin, mode)
 * Initialize a GPIO pin
 */
static jerry_value_t gpio_init_handler(const jerry_call_info_t *call_info_p,
                                        const jerry_value_t args[],
                                        const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.init requires pin and mode");
    }
    
    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    int mode = (int)js_get_number_arg(args, argc, 1, GPIO_MODE_OUTPUT);
    
    /* Validate pin number */
    if (pin >= NUM_BANK0_GPIOS) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    
    gpio_init(pin);
    
    switch (mode) {
        case GPIO_MODE_OUTPUT:
            gpio_set_dir(pin, GPIO_OUT);
            break;
        case GPIO_MODE_INPUT:
            gpio_set_dir(pin, GPIO_IN);
            gpio_disable_pulls(pin);
            break;
        case GPIO_MODE_INPUT_PULLUP:
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);
            break;
        case GPIO_MODE_INPUT_PULLDOWN:
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_down(pin);
            break;
        default:
            return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO mode");
    }
    
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
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.set requires pin and value");
    }
    
    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    bool value = js_get_boolean_arg(args, argc, 1, false);
    
    if (pin >= NUM_BANK0_GPIOS) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    
    gpio_put(pin, value);
    
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
    
    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.get requires pin");
    }
    
    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    
    if (pin >= NUM_BANK0_GPIOS) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    
    bool value = gpio_get(pin);
    
    return jerry_boolean(value);
}

/*
 * GPIO.toggle(pin)
 * Toggle GPIO output
 */
static jerry_value_t gpio_toggle_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "GPIO.toggle requires pin");
    }
    
    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    
    if (pin >= NUM_BANK0_GPIOS) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    
    bool current = gpio_get(pin);
    gpio_put(pin, !current);
    
    return jerry_undefined();
}

/*
 * Register GPIO bindings
 */
void js_bind_gpio(void) {
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
    
    js_register_global("GPIO", gpio);
    jerry_value_free(gpio);
}
