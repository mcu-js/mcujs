#ifndef MCUJS_PWM_POLICY_H
#define MCUJS_PWM_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool used;
    bool pending_cleanup;
    uint32_t frequency;
    uint16_t references;
} mcujs_pwm_timer_resource_t;

typedef struct {
    uint16_t divider_scaled;
    uint16_t wrap;
} mcujs_pwm_rp_frequency_config_t;

typedef struct {
    uint32_t resolution;
    uint32_t period;
    uint32_t divider;
    uint32_t actual_frequency;
} mcujs_pwm_esp_frequency_config_t;

bool mcujs_pwm_rp_find_exact_frequency(
    uint32_t clock_frequency, uint32_t frequency,
    mcujs_pwm_rp_frequency_config_t *config);

bool mcujs_pwm_esp_find_exact_frequency(
    uint32_t source_frequency, uint32_t requested_frequency,
    uint32_t candidate_resolution, uint32_t maximum_safe_resolution,
    uint8_t divider_fractional_bits, uint32_t minimum_divider,
    uint32_t maximum_divider, mcujs_pwm_esp_frequency_config_t *config);

bool mcujs_pwm_ratio_to_level(double ratio, uint32_t period,
                              uint32_t maximum_level, uint32_t *level);

bool mcujs_pwm_fixed_timer_accepts(uint16_t references, bool current_owner,
                                   uint32_t configured_frequency,
                                   uint32_t requested_frequency);

int mcujs_pwm_select_timer(const mcujs_pwm_timer_resource_t timers[],
                           size_t timer_count, uint32_t requested_frequency,
                           int current_timer);

#endif
