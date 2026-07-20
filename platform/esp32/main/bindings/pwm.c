/* MCU.js PWM binding for ESP32-S3 LEDC. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MCUJS_PWM_CHANNEL_COUNT 8
#define MCUJS_PWM_TIMER_COUNT 4
#define MCUJS_PWM_MAX_INPUT 65535.0
#define MCUJS_PWM_SOURCE_HZ 80000000u
#define MCUJS_PWM_MAX_RESOLUTION 14u

typedef struct {
    bool used;
    uint32_t frequency;
    ledc_timer_bit_t resolution;
    uint8_t references;
} pwm_timer_state_t;

typedef struct {
    bool used;
    int pin;
    ledc_channel_t channel;
    ledc_timer_t timer;
} pwm_channel_state_t;

static pwm_timer_state_t s_timers[MCUJS_PWM_TIMER_COUNT];
static pwm_channel_state_t s_channels[MCUJS_PWM_CHANNEL_COUNT];

static jerry_value_t throw_pwm_error(mcujs_operational_error_t error,
                                     esp_err_t native_error, int pin,
                                     double limit, const char *message) {
    const mcujs_error_details_t details = {
        .resource = "pwm",
        .has_pin = pin >= 0,
        .pin = pin,
        .has_limit = limit > 0,
        .limit = limit,
        .has_native_code = native_error != ESP_OK,
        .native_code = native_error,
    };
    return mcujs_throw_operational_error(error, message, &details);
}

static mcujs_operational_error_t map_pwm_native_error(esp_err_t error) {
    if (error == ESP_ERR_TIMEOUT || error == ESP_ERR_INVALID_STATE) {
        return MCUJS_ERROR_BUSY;
    }
    if (error == ESP_ERR_NO_MEM) return MCUJS_ERROR_RESOURCE_EXHAUSTED;
    if (error == ESP_ERR_NOT_SUPPORTED) return MCUJS_ERROR_NOT_SUPPORTED;
    return MCUJS_ERROR_IO;
}

static pwm_channel_state_t *find_channel(int pin) {
    for (size_t i = 0; i < MCUJS_PWM_CHANNEL_COUNT; i++) {
        if (s_channels[i].used && s_channels[i].pin == pin) {
            return &s_channels[i];
        }
    }
    return NULL;
}

static pwm_channel_state_t *find_free_channel(void) {
    for (size_t i = 0; i < MCUJS_PWM_CHANNEL_COUNT; i++) {
        if (!s_channels[i].used) {
            s_channels[i].channel = (ledc_channel_t)i;
            return &s_channels[i];
        }
    }
    return NULL;
}

static int find_timer(uint32_t frequency,
                      const pwm_channel_state_t *existing) {
    for (int i = 0; i < MCUJS_PWM_TIMER_COUNT; i++) {
        if (s_timers[i].used && s_timers[i].frequency == frequency) {
            return i;
        }
    }
    for (int i = 0; i < MCUJS_PWM_TIMER_COUNT; i++) {
        if (!s_timers[i].used) {
            return i;
        }
    }
    if (existing != NULL &&
        s_timers[existing->timer].references == 1) {
        return (int)existing->timer;
    }
    return -1;
}

static esp_err_t deconfigure_timer(int index) {
    esp_err_t result =
        ledc_timer_pause(LEDC_LOW_SPEED_MODE, (ledc_timer_t)index);
    if (result != ESP_OK) return result;
    ledc_timer_config_t config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = (ledc_timer_t)index,
        .deconfigure = true,
    };
    return ledc_timer_config(&config);
}

static esp_err_t release_channel(pwm_channel_state_t *channel) {
    esp_err_t result = ledc_stop(LEDC_LOW_SPEED_MODE, channel->channel, 0);
    if (result != ESP_OK) return result;
    result = gpio_reset_pin((gpio_num_t)channel->pin);
    if (result != ESP_OK) return result;

    int timer_index = (int)channel->timer;
    bool last_reference = s_timers[timer_index].references == 1;
    if (last_reference) {
        result = deconfigure_timer(timer_index);
        if (result != ESP_OK) return result;
    }

    mcujs_pin_release(channel->pin, MCUJS_PIN_OWNER_PWM);
    if (s_timers[timer_index].references > 0) {
        s_timers[timer_index].references--;
    }
    if (last_reference) s_timers[timer_index] = (pwm_timer_state_t){0};
    *channel = (pwm_channel_state_t){0};
    return ESP_OK;
}

static jerry_value_t pwm_init_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int pin;
    int frequency_value;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM pin must be a finite number",
                              "PWM pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &frequency_value);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM frequency must be a finite number",
                              "PWM frequency must be an integer");
    }
    if (!mcujs_pin_is_peripheral_output(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid PWM output pin (use GPIO1..GPIO9)");
    }
    if (frequency_value < 1 || frequency_value > 1000000) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "PWM frequency must be 1..1000000 Hz");
    }
    if (!mcujs_pin_can_claim(pin, MCUJS_PIN_OWNER_PWM)) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, ESP_OK, pin, 0,
                               "PWM pin is owned by another peripheral");
    }

    pwm_channel_state_t *existing = find_channel(pin);
    pwm_channel_state_t *channel =
        existing != NULL ? existing : find_free_channel();
    if (channel == NULL) {
        return throw_pwm_error(MCUJS_ERROR_RESOURCE_EXHAUSTED, ESP_OK, pin,
                               MCUJS_PWM_CHANNEL_COUNT,
                               "No PWM channels available");
    }
    uint32_t frequency = (uint32_t)frequency_value;
    int timer_index = find_timer(frequency, existing);
    if (timer_index < 0) {
        return throw_pwm_error(MCUJS_ERROR_RESOURCE_EXHAUSTED, ESP_OK, pin,
                               MCUJS_PWM_TIMER_COUNT,
                               "No PWM timers available for this frequency");
    }

    bool new_timer = !s_timers[timer_index].used ||
                     (existing != NULL && existing->timer == timer_index &&
                      s_timers[timer_index].references == 1);
    uint32_t resolution = 0;
    if (new_timer) {
        resolution = ledc_find_suitable_duty_resolution(MCUJS_PWM_SOURCE_HZ,
                                                        frequency);
        if (resolution == 0) {
            return throw_pwm_error(MCUJS_ERROR_NOT_SUPPORTED, ESP_OK, pin, 0,
                                   "PWM frequency is not achievable");
        }
        if (resolution > MCUJS_PWM_MAX_RESOLUTION) {
            resolution = MCUJS_PWM_MAX_RESOLUTION;
        }
    }

    if (existing != NULL) {
        esp_err_t release_error = release_channel(existing);
        if (release_error != ESP_OK) {
            return throw_pwm_error(map_pwm_native_error(release_error),
                                   release_error, pin, 0,
                                   "PWM reinitialization failed");
        }
    }
    channel->channel = (ledc_channel_t)(channel - s_channels);
    if (!mcujs_pin_claim(pin, MCUJS_PIN_OWNER_PWM)) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, ESP_OK, pin, 0,
                               "PWM pin claim failed");
    }
    if (new_timer) {
        ledc_timer_config_t timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = (ledc_timer_bit_t)resolution,
            .timer_num = (ledc_timer_t)timer_index,
            .freq_hz = frequency,
            .clk_cfg = LEDC_AUTO_CLK,
            .deconfigure = false,
        };
        esp_err_t timer_error = ledc_timer_config(&timer);
        if (timer_error != ESP_OK) {
            esp_err_t reset_error = gpio_reset_pin((gpio_num_t)pin);
            if (reset_error != ESP_OK) {
                return throw_pwm_error(map_pwm_native_error(reset_error),
                                       reset_error, pin, 0,
                                       "PWM timer rollback failed");
            }
            mcujs_pin_release(pin, MCUJS_PIN_OWNER_PWM);
            return throw_pwm_error(map_pwm_native_error(timer_error),
                                   timer_error, pin, 0,
                                   "PWM timer initialization failed");
        }
        uint32_t actual_frequency =
            ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)timer_index);
        if (actual_frequency != frequency) {
            esp_err_t cleanup_error = deconfigure_timer(timer_index);
            if (cleanup_error == ESP_OK) {
                cleanup_error = gpio_reset_pin((gpio_num_t)pin);
            }
            if (cleanup_error != ESP_OK) {
                return throw_pwm_error(map_pwm_native_error(cleanup_error),
                                       cleanup_error, pin, 0,
                                       "PWM frequency rollback failed");
            }
            mcujs_pin_release(pin, MCUJS_PIN_OWNER_PWM);
            return throw_pwm_error(
                MCUJS_ERROR_NOT_SUPPORTED, ESP_OK, pin, 0,
                "PWM frequency cannot be represented exactly");
        }
        s_timers[timer_index].used = true;
        s_timers[timer_index].frequency = frequency;
        s_timers[timer_index].resolution = (ledc_timer_bit_t)resolution;
    }

    ledc_channel_config_t config = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer_index,
        .duty = 0,
        .hpoint = 0,
        .flags = {.output_invert = 0},
    };
    esp_err_t channel_error = ledc_channel_config(&config);
    if (channel_error != ESP_OK) {
        if (new_timer) {
            esp_err_t cleanup_error = deconfigure_timer(timer_index);
            if (cleanup_error != ESP_OK) {
                return throw_pwm_error(map_pwm_native_error(cleanup_error),
                                       cleanup_error, pin, 0,
                                       "PWM channel rollback failed");
            }
            s_timers[timer_index] = (pwm_timer_state_t){0};
        }
        esp_err_t reset_error = gpio_reset_pin((gpio_num_t)pin);
        if (reset_error != ESP_OK) {
            return throw_pwm_error(map_pwm_native_error(reset_error),
                                   reset_error, pin, 0,
                                   "PWM channel pin rollback failed");
        }
        mcujs_pin_release(pin, MCUJS_PIN_OWNER_PWM);
        return throw_pwm_error(map_pwm_native_error(channel_error),
                               channel_error, pin, 0,
                               "PWM channel initialization failed");
    }

    channel->used = true;
    channel->pin = pin;
    channel->timer = (ledc_timer_t)timer_index;
    s_timers[timer_index].references++;
    return jerry_undefined();
}

static jerry_value_t pwm_set_duty_handler(const jerry_call_info_t *info,
                                          const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int pin;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM pin must be a finite number",
                              "PWM pin must be an integer");
    }
    double input;
    status = mcujs_get_finite_number(args, argc, 1, &input);
    if (status != MCUJS_ARG_OK) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "PWM duty must be a finite number");
    }
    if (input < 0 || input > 1.0) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "PWM duty must be 0..1");
    }
    if (!mcujs_pin_is_peripheral_output(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid PWM output pin (use GPIO1..GPIO9)");
    }
    pwm_channel_state_t *channel = find_channel(pin);
    if (channel == NULL) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, ESP_OK, pin, 0,
                               "PWM pin is not initialized");
    }
    double public_duty = input * MCUJS_PWM_MAX_INPUT;
    uint32_t resolution = (uint32_t)s_timers[channel->timer].resolution;
    uint32_t maximum = (1u << resolution) - 1u;
    uint32_t duty = (uint32_t)((public_duty * maximum) / MCUJS_PWM_MAX_INPUT);
    esp_err_t duty_error =
        ledc_set_duty(LEDC_LOW_SPEED_MODE, channel->channel, duty);
    if (duty_error == ESP_OK) {
        duty_error = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel->channel);
    }
    if (duty_error != ESP_OK) {
        return throw_pwm_error(map_pwm_native_error(duty_error), duty_error,
                               pin, 0,
                               "PWM duty update failed");
    }
    return jerry_undefined();
}

static jerry_value_t pwm_stop_handler(const jerry_call_info_t *info,
                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    int pin;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "PWM pin must be a finite number",
                              "PWM pin must be an integer");
    }
    if (!mcujs_pin_is_peripheral_output(pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Invalid PWM output pin (use GPIO1..GPIO9)");
    }
    pwm_channel_state_t *channel = find_channel(pin);
    if (channel == NULL) {
        return throw_pwm_error(MCUJS_ERROR_BUSY, ESP_OK, pin, 0,
                               "PWM pin is not initialized");
    }
    esp_err_t release_error = release_channel(channel);
    if (release_error != ESP_OK) {
        return throw_pwm_error(map_pwm_native_error(release_error),
                               release_error, pin, 0,
                               "PWM stop failed");
    }
    return jerry_undefined();
}

jerry_value_t js_create_pwm_module(void) {
    jerry_value_t pwm = jerry_object();
    js_set_function(pwm, "init", pwm_init_handler);
    js_set_function(pwm, "setDuty", pwm_set_duty_handler);
    js_set_function(pwm, "stop", pwm_stop_handler);
    return pwm;
}

void js_bind_pwm(void) {
    jerry_value_t pwm = js_create_pwm_module();
    js_register_global("PWM", pwm);
    jerry_value_free(pwm);
}
