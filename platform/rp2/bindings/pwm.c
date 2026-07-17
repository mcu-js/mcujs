/*
 * mcujs - PWM Bindings
 * 
 * Implements: PWM.init(), PWM.setDuty(), PWM.stop()
 */

#include "bindings.h"
#include "jerryscript.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);
extern double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, double default_value);

/* Default PWM wrap value for 16-bit resolution */
#define PWM_WRAP_DEFAULT 65535

/*
 * PWM.init(pin, frequency)
 * Initialize PWM on a pin with specified frequency
 */
static jerry_value_t pwm_init_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "PWM.init requires pin and frequency");
    }
    
    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    uint32_t freq = (uint32_t)js_get_number_arg(args, argc, 1, 1000);
    
    /* Validate pin */
    if (pin >= NUM_BANK0_GPIOS) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    
    /* Set up PWM */
    gpio_set_function(pin, GPIO_FUNC_PWM);
    
    uint slice = pwm_gpio_to_slice_num(pin);
    
    /* Calculate divider for desired frequency */
    uint32_t clock_freq = clock_get_hz(clk_sys);
    float divider = (float)clock_freq / (freq * (PWM_WRAP_DEFAULT + 1));
    
    if (divider < 1.0f) {
        divider = 1.0f;
    } else if (divider > 255.0f) {
        divider = 255.0f;
    }
    
    pwm_set_clkdiv(slice, divider);
    pwm_set_wrap(slice, PWM_WRAP_DEFAULT);
    pwm_set_gpio_level(pin, 0);
    pwm_set_enabled(slice, true);
    
    return jerry_undefined();
}

/*
 * PWM.setDuty(pin, duty)
 * Set PWM duty cycle (0-65535 or 0.0-1.0)
 */
static jerry_value_t pwm_set_duty_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 2) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "PWM.setDuty requires pin and duty");
    }
    
    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    double duty = js_get_number_arg(args, argc, 1, 0);
    
    if (pin >= NUM_BANK0_GPIOS) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    
    /* Handle fractional duty (0.0-1.0) */
    uint16_t level;
    if (duty >= 0.0 && duty <= 1.0) {
        level = (uint16_t)(duty * PWM_WRAP_DEFAULT);
    } else {
        level = (uint16_t)duty;
        if (level > PWM_WRAP_DEFAULT) {
            level = PWM_WRAP_DEFAULT;
        }
    }
    
    pwm_set_gpio_level(pin, level);
    
    return jerry_undefined();
}

/*
 * PWM.stop(pin)
 * Stop PWM on a pin
 */
static jerry_value_t pwm_stop_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;
    
    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "PWM.stop requires pin");
    }
    
    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    
    if (pin >= NUM_BANK0_GPIOS) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid GPIO pin number");
    }
    
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_enabled(slice, false);
    
    /* Reset to GPIO function */
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_init(pin);
    
    return jerry_undefined();
}

/*
 * Create PWM module object
 */
jerry_value_t js_create_pwm_module(void) {
    jerry_value_t pwm = jerry_object();

    js_set_function(pwm, "init", pwm_init_handler);
    js_set_function(pwm, "setDuty", pwm_set_duty_handler);
    js_set_function(pwm, "stop", pwm_stop_handler);

    return pwm;
}

/*
 * Register PWM bindings
 */
void js_bind_pwm(void) {
    jerry_value_t pwm = js_create_pwm_module();
    js_register_global("PWM", pwm);
    jerry_value_free(pwm);
}
