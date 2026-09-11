/*
 * mcujs - PWM Bindings
 * 
 * Implements: PWM.init(), PWM.setDuty(), PWM.stop()
 */

#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "pwm_policy.h"
#include "validation.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);

#define PWM_SLICE_STORAGE ((NUM_BANK0_GPIOS + 1u) / 2u)

#if MCUJS_HAS_CONFIGURED_BUZZER
#include "board_config.h"
static bool s_buzzer_open;
#endif
static bool s_pwm_initialized[NUM_BANK0_GPIOS];
static uint16_t s_pwm_slice_references[PWM_SLICE_STORAGE];
static uint32_t s_pwm_slice_frequency[PWM_SLICE_STORAGE];
static uint16_t s_pwm_slice_wrap[PWM_SLICE_STORAGE];
/* Zero means unclaimed; otherwise the stored value is GPIO + 1. */
static uint8_t s_pwm_output_owners[PWM_SLICE_STORAGE][2];

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
    gpio_put(pin, false);
    gpio_set_dir(pin, GPIO_OUT);
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

    mcujs_pwm_rp_frequency_config_t frequency_config;
    if (!mcujs_pwm_rp_find_exact_frequency(
            clock_freq, (uint32_t)frequency, &frequency_config)) {
        return throw_pwm_error(MCUJS_ERROR_NOT_SUPPORTED, pin,
                               "PWM frequency cannot be represented exactly");
    }

    uint slice = pwm_gpio_to_slice_num((uint)pin);
    uint channel = pwm_gpio_to_channel((uint)pin);
    if (slice >= PWM_SLICE_STORAGE || channel >= 2u) {
        return throw_pwm_error(MCUJS_ERROR_NOT_SUPPORTED, pin,
                               "PWM pin cannot be mapped to a timer");
    }
#if MCUJS_HAS_CONFIGURED_BUZZER
    if (s_buzzer_open && slice == pwm_gpio_to_slice_num(MCUJS_BUZZER_PIN))
        return throw_pwm_error(MCUJS_ERROR_BUSY, pin, "PWM slice belongs to configured buzzer");
#endif
    uint8_t output_owner = s_pwm_output_owners[slice][channel];
    if (output_owner != 0 && output_owner != (uint8_t)(pin + 1)) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, pin,
                               "PWM output is owned by an aliased pin");
    }
    if (!mcujs_pwm_fixed_timer_accepts(
            s_pwm_slice_references[slice], s_pwm_initialized[pin],
            s_pwm_slice_frequency[slice], (uint32_t)frequency)) {
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
 * Set PWM duty cycle as a ratio from 0.0 to 1.0.
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
    uint32_t level;
    if (!mcujs_pwm_ratio_to_level(duty, period, UINT16_MAX, &level)) {
        return throw_pwm_error(MCUJS_ERROR_NOT_SUPPORTED, pin,
                               "PWM duty cannot be represented exactly");
    }
    pwm_set_gpio_level((uint)pin, (uint16_t)level);
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

#if MCUJS_HAS_CONFIGURED_BUZZER
#include "board_config.h"
#include "pico/time.h"
#include "hardware/sync.h"
#if !defined(MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69) || MCUJS_BUZZER_PIN != 2
#error Unqualified configured buzzer adapter
#endif
static volatile bool s_buzzer_playing;
static volatile alarm_id_t s_buzzer_alarm;
static int s_buzzer_generation;
static void buzzer_silence(void) {
    pwm_set_enabled(pwm_gpio_to_slice_num(MCUJS_BUZZER_PIN), false);
    gpio_set_function(MCUJS_BUZZER_PIN, GPIO_FUNC_SIO);
    gpio_init(MCUJS_BUZZER_PIN);
    gpio_put(MCUJS_BUZZER_PIN, false);
    gpio_set_dir(MCUJS_BUZZER_PIN, GPIO_OUT);
    s_buzzer_playing = false;
}
static int64_t buzzer_deadline(alarm_id_t id, void *data) {
    (void)data;
    if (id == s_buzzer_alarm) { buzzer_silence(); s_buzzer_alarm = 0; }
    return 0;
}
static void buzzer_stop(void) {
    uint32_t irq = save_and_disable_interrupts();
    alarm_id_t alarm = s_buzzer_alarm;
    s_buzzer_alarm = 0;
    if (alarm > 0) cancel_alarm(alarm);
    if (s_buzzer_open) buzzer_silence();
    restore_interrupts(irq);
}
void js_buzzer_cleanup(void) {
    buzzer_stop();
    s_buzzer_open = false;
}
static jerry_value_t buzzer_error(mcujs_operational_error_t code, const char *message) {
    const mcujs_error_details_t details = {.resource = "buzzer"};
    return mcujs_throw_operational_error(code, message, &details);
}
static bool buzzer_valid(const jerry_value_t args[], jerry_length_t argc) {
    int token;
    return s_buzzer_open && mcujs_get_integer(args, argc, 0, &token) == MCUJS_ARG_OK && token == s_buzzer_generation;
}
static jerry_value_t buzzer_open_handler(const jerry_call_info_t *info, const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args;
    if (argc) return jerry_throw_sz(JERRY_ERROR_TYPE, "Buzzer open takes no arguments");
    if (s_buzzer_open || s_pwm_slice_references[pwm_gpio_to_slice_num(MCUJS_BUZZER_PIN)])
        return buzzer_error(MCUJS_ERROR_BUSY, "Buzzer PWM slice is owned");
    if (s_buzzer_generation == INT32_MAX) return buzzer_error(MCUJS_ERROR_RESOURCE_EXHAUSTED, "Buzzer handle tokens exhausted");
    s_buzzer_open = true;
    buzzer_silence();
    return jerry_number(++s_buzzer_generation);
}
static jerry_value_t buzzer_start_handler(const jerry_call_info_t *info, const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (!buzzer_valid(args, argc)) return buzzer_error(MCUJS_ERROR_NO_DEVICE, "Stale buzzer handle");
    int frequency, duration;
    if (argc != 3 || mcujs_get_integer(args, argc, 1, &frequency) != MCUJS_ARG_OK ||
        mcujs_get_integer(args, argc, 2, &duration) != MCUJS_ARG_OK)
        return jerry_throw_sz(JERRY_ERROR_TYPE, "Buzzer expects integer frequency and duration");
    if (frequency < 500 || frequency > 4000 || duration < 1 || duration > 1000)
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Buzzer tone outside capability");
    if (s_buzzer_playing) return buzzer_error(MCUJS_ERROR_BUSY, "Buzzer is playing");
    mcujs_pwm_rp_frequency_config_t config;
    if (!mcujs_pwm_rp_find_exact_frequency(clock_get_hz(clk_sys), frequency, &config) || ((config.wrap + 1u) & 1u))
        return buzzer_error(MCUJS_ERROR_NOT_SUPPORTED, "Tone frequency or 50% duty cannot be represented exactly");
    uint32_t irq = save_and_disable_interrupts();
    s_buzzer_alarm = add_alarm_in_ms(duration, buzzer_deadline, NULL, false);
    if (s_buzzer_alarm <= 0) {
        s_buzzer_alarm = 0;
        restore_interrupts(irq);
        return buzzer_error(MCUJS_ERROR_RESOURCE_EXHAUSTED, "No buzzer safety alarm available");
    }
    uint slice = pwm_gpio_to_slice_num(MCUJS_BUZZER_PIN);
    pwm_set_clkdiv_int_frac(slice, config.divider_scaled >> 4u, config.divider_scaled & 15u);
    pwm_set_wrap(slice, config.wrap);
    pwm_set_gpio_level(MCUJS_BUZZER_PIN, (config.wrap + 1u) / 2u);
    gpio_set_function(MCUJS_BUZZER_PIN, GPIO_FUNC_PWM);
    s_buzzer_playing = true;
    pwm_set_enabled(slice, true);
    restore_interrupts(irq);
    return jerry_number(frequency);
}
static jerry_value_t buzzer_stop_handler(const jerry_call_info_t *info, const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (!buzzer_valid(args, argc)) return buzzer_error(MCUJS_ERROR_NO_DEVICE, "Stale buzzer handle");
    buzzer_stop(); return jerry_undefined();
}
static jerry_value_t buzzer_close_handler(const jerry_call_info_t *info, const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (!buzzer_valid(args, argc)) return buzzer_error(MCUJS_ERROR_NO_DEVICE, "Stale buzzer handle");
    js_buzzer_cleanup(); return jerry_undefined();
}
static jerry_value_t buzzer_playing_handler(const jerry_call_info_t *info, const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    if (!buzzer_valid(args, argc)) return buzzer_error(MCUJS_ERROR_NO_DEVICE, "Stale buzzer handle");
    return jerry_boolean(s_buzzer_playing);
}
static jerry_value_t buzzer_state_handler(const jerry_call_info_t *info, const jerry_value_t args[], jerry_length_t argc) {
    (void)info; (void)args; (void)argc;
    return jerry_string_sz(s_buzzer_open || s_pwm_slice_references[pwm_gpio_to_slice_num(MCUJS_BUZZER_PIN)] ? "busy" : "idle");
}
jerry_value_t js_create_buzzer_native_module(void) {
    jerry_value_t module = jerry_object();
    js_set_function(module, "open", buzzer_open_handler);
    js_set_function(module, "start", buzzer_start_handler);
    js_set_function(module, "stop", buzzer_stop_handler);
    js_set_function(module, "close", buzzer_close_handler);
    js_set_function(module, "playing", buzzer_playing_handler);
    js_set_function(module, "state", buzzer_state_handler);
    return module;
}
#endif
