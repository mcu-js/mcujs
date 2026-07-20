/*
 * mcujs - PWM Bindings
 * 
 * Implements: PWM.init(), PWM.setDuty(), PWM.stop()
 */

#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "validation.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);

#define PWM_DIVIDER_SCALE 16u
#define PWM_DIVIDER_SCALED_MIN 16u
#define PWM_DIVIDER_SCALED_MAX 4095u
#define PWM_PERIOD_MAX UINT16_MAX
#define PWM_SLICE_STORAGE ((NUM_BANK0_GPIOS + 1u) / 2u)

static bool s_pwm_initialized[NUM_BANK0_GPIOS];
static uint16_t s_pwm_slice_references[PWM_SLICE_STORAGE];
static uint32_t s_pwm_slice_frequency[PWM_SLICE_STORAGE];
static uint16_t s_pwm_slice_wrap[PWM_SLICE_STORAGE];
/* Zero means unclaimed; otherwise the stored value is GPIO + 1. */
static uint8_t s_pwm_output_owners[PWM_SLICE_STORAGE][2];

typedef struct {
    uint16_t divider_scaled;
    uint16_t wrap;
} pwm_frequency_config_t;

static bool find_exact_frequency(uint32_t clock_frequency, uint32_t frequency,
                                 pwm_frequency_config_t *config) {
    uint64_t scaled_clock = (uint64_t)clock_frequency * PWM_DIVIDER_SCALE;
    if (scaled_clock % frequency != 0) return false;

    uint64_t divider_period_product = scaled_clock / frequency;
    uint32_t first_divider = (uint32_t)(
        (divider_period_product + PWM_PERIOD_MAX - 1u) / PWM_PERIOD_MAX);
    if (first_divider < PWM_DIVIDER_SCALED_MIN) {
        first_divider = PWM_DIVIDER_SCALED_MIN;
    }
    for (uint32_t divider = first_divider;
         divider <= PWM_DIVIDER_SCALED_MAX; divider++) {
        if (divider_period_product % divider != 0) continue;
        uint64_t period = divider_period_product / divider;
        if (period < 1 || period > PWM_PERIOD_MAX) continue;
        config->divider_scaled = (uint16_t)divider;
        config->wrap = (uint16_t)(period - 1u);
        return true;
    }
    return false;
}

static bool find_exact_duty_level(double duty, uint32_t period,
                                  uint16_t *level) {
    uint32_t candidate = (uint32_t)(duty * (double)period + 0.5);
    if (candidate > period || candidate > UINT16_MAX ||
        (double)candidate / (double)period != duty) {
        return false;
    }
    *level = (uint16_t)candidate;
    return true;
}

static jerry_value_t throw_pwm_error(mcujs_operational_error_t error, int pin,
                                     const char *message) {
    const mcujs_error_details_t details = {
        .resource = "pwm",
        .has_pin = true,
        .pin = pin,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static void release_pwm_pin(uint pin) {
    if (!s_pwm_initialized[pin]) return;
    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_init(pin);
    s_pwm_initialized[pin] = false;
    if (slice < PWM_SLICE_STORAGE && s_pwm_slice_references[slice] > 0) {
        s_pwm_slice_references[slice]--;
        if (s_pwm_slice_references[slice] == 0) {
            pwm_set_enabled(slice, false);
            s_pwm_slice_frequency[slice] = 0;
            s_pwm_slice_wrap[slice] = 0;
        }
    }
    if (slice < PWM_SLICE_STORAGE && channel < 2u &&
        s_pwm_output_owners[slice][channel] == (uint8_t)(pin + 1u)) {
        s_pwm_output_owners[slice][channel] = 0;
    }
    mcujs_rp2_pin_release((int)pin, MCUJS_RP2_PIN_OWNER_PWM);
}

/*
 * PWM.init(pin, frequency)
 * Initialize PWM on a pin with specified frequency
 */
static jerry_value_t pwm_init_handler(const jerry_call_info_t *call_info_p,
                                       const jerry_value_t args[],
                                       const jerry_length_t argc) {
    (void)call_info_p;

    int pin;
    int frequency;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM pin must be a finite number",
                              "PWM pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &frequency);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM frequency must be a finite number",
                              "PWM frequency must be an integer");
    }
    if (!mcujs_rp2_pwm_pin_allowed(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "PWM pin is not available on this board");
    }

    uint32_t clock_freq = clock_get_hz(clk_sys);
    if (frequency < MCUJS_RUNTIME_PWM_MIN_HZ ||
        frequency > MCUJS_RUNTIME_PWM_MAX_HZ) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "PWM frequency is outside the board capability");
    }

    pwm_frequency_config_t frequency_config;
    if (!find_exact_frequency(clock_freq, (uint32_t)frequency,
                              &frequency_config)) {
        return throw_pwm_error(MCUJS_ERROR_NOT_SUPPORTED, pin,
                               "PWM frequency cannot be represented exactly");
    }

    uint slice = pwm_gpio_to_slice_num((uint)pin);
    uint channel = pwm_gpio_to_channel((uint)pin);
    if (slice >= PWM_SLICE_STORAGE || channel >= 2u) {
        return throw_pwm_error(MCUJS_ERROR_NOT_SUPPORTED, pin,
                               "PWM pin cannot be mapped to a timer");
    }
    uint8_t output_owner = s_pwm_output_owners[slice][channel];
    if (output_owner != 0 && output_owner != (uint8_t)(pin + 1)) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, pin,
                               "PWM output is owned by an aliased pin");
    }
    uint16_t own_reference = s_pwm_initialized[pin] ? 1u : 0u;
    if (s_pwm_slice_references[slice] > own_reference &&
        s_pwm_slice_frequency[slice] != (uint32_t)frequency) {
        return throw_pwm_error(
            MCUJS_ERROR_BUSY, pin,
            "PWM timer is already configured at a different frequency");
    }
    if (!mcujs_rp2_pin_can_claim(pin, MCUJS_RP2_PIN_OWNER_PWM)) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, pin,
                               "PWM pin is owned by another peripheral");
    }
    if (s_pwm_initialized[pin]) release_pwm_pin((uint)pin);
    if (!mcujs_rp2_pin_claim(pin, MCUJS_RP2_PIN_OWNER_PWM)) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, pin, "PWM pin claim failed");
    }

    gpio_set_function((uint)pin, GPIO_FUNC_PWM);
    pwm_set_clkdiv_int_frac(slice,
                            (uint8_t)(frequency_config.divider_scaled >> 4u),
                            (uint8_t)(frequency_config.divider_scaled & 0x0fu));
    pwm_set_wrap(slice, frequency_config.wrap);
    pwm_set_gpio_level((uint)pin, 0);
    pwm_set_enabled(slice, true);

    s_pwm_initialized[pin] = true;
    s_pwm_slice_references[slice]++;
    s_pwm_slice_frequency[slice] = (uint32_t)frequency;
    s_pwm_slice_wrap[slice] = frequency_config.wrap;
    s_pwm_output_owners[slice][channel] = (uint8_t)(pin + 1);
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

    int pin;
    double duty;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM pin must be a finite number",
                              "PWM pin must be an integer");
    }
    status = mcujs_get_number_range(args, argc, 1, 0.0, 1.0, &duty);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM duty must be a finite number",
                              "PWM duty must be 0..1");
    }
    if (!mcujs_rp2_pwm_pin_allowed(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "PWM pin is not available on this board");
    }
    if (!s_pwm_initialized[pin]) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, pin,
                               "PWM pin is not initialized");
    }

    uint slice = pwm_gpio_to_slice_num((uint)pin);
    uint32_t period = (uint32_t)s_pwm_slice_wrap[slice] + 1u;
    uint16_t level;
    if (!find_exact_duty_level(duty, period, &level)) {
        return throw_pwm_error(MCUJS_ERROR_NOT_SUPPORTED, pin,
                               "PWM duty cannot be represented exactly");
    }
    pwm_set_gpio_level((uint)pin, level);
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

    int pin;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM pin must be a finite number",
                              "PWM pin must be an integer");
    }
    if (!mcujs_rp2_pwm_pin_allowed(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "PWM pin is not available on this board");
    }
    if (!s_pwm_initialized[pin]) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, pin,
                               "PWM pin is not initialized");
    }

    release_pwm_pin((uint)pin);
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
