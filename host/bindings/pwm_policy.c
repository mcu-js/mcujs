#include "pwm_policy.h"

#include <math.h>

#define MCUJS_RP_PWM_DIVIDER_SCALE 16u
#define MCUJS_RP_PWM_DIVIDER_SCALED_MIN 16u
#define MCUJS_RP_PWM_DIVIDER_SCALED_MAX 4095u
#define MCUJS_RP_PWM_PERIOD_MAX UINT16_MAX

bool mcujs_pwm_rp_find_exact_frequency(
    uint32_t clock_frequency, uint32_t frequency,
    mcujs_pwm_rp_frequency_config_t *config) {
    if (config == NULL || clock_frequency == 0u || frequency == 0u) {
        return false;
    }

    uint64_t scaled_clock =
        (uint64_t)clock_frequency * MCUJS_RP_PWM_DIVIDER_SCALE;
    if (scaled_clock % frequency != 0u) return false;

    uint64_t divider_period_product = scaled_clock / frequency;
    uint64_t first_divider =
        (divider_period_product + MCUJS_RP_PWM_PERIOD_MAX - 1u) /
        MCUJS_RP_PWM_PERIOD_MAX;
    if (first_divider < MCUJS_RP_PWM_DIVIDER_SCALED_MIN) {
        first_divider = MCUJS_RP_PWM_DIVIDER_SCALED_MIN;
    }
    for (uint64_t divider = first_divider;
         divider <= MCUJS_RP_PWM_DIVIDER_SCALED_MAX; divider++) {
        if (divider_period_product % divider != 0u) continue;
        uint64_t period = divider_period_product / divider;
        if (period < 1u || period > MCUJS_RP_PWM_PERIOD_MAX) continue;

        mcujs_pwm_rp_frequency_config_t candidate = {
            .divider_scaled = (uint16_t)divider,
            .wrap = (uint16_t)(period - 1u),
        };
        *config = candidate;
        return true;
    }
    return false;
}

bool mcujs_pwm_esp_find_exact_frequency(
    uint32_t source_frequency, uint32_t requested_frequency,
    uint32_t candidate_resolution, uint32_t maximum_safe_resolution,
    uint8_t divider_fractional_bits, uint32_t minimum_divider,
    uint32_t maximum_divider, mcujs_pwm_esp_frequency_config_t *config) {
    if (config == NULL || source_frequency == 0u || requested_frequency == 0u ||
        candidate_resolution == 0u || maximum_safe_resolution == 0u ||
        candidate_resolution >= 32u || maximum_safe_resolution >= 32u ||
        divider_fractional_bits >= 32u || minimum_divider == 0u ||
        minimum_divider > maximum_divider) {
        return false;
    }

    uint64_t scaled_source =
        (uint64_t)source_frequency << divider_fractional_bits;
    uint32_t highest_resolution = candidate_resolution;
    if (highest_resolution > maximum_safe_resolution) {
        highest_resolution = maximum_safe_resolution;
    }
    for (uint32_t resolution = highest_resolution; resolution > 0u;
         resolution--) {
        uint64_t period = (uint64_t)1u << resolution;
        uint64_t requested_denominator =
            (uint64_t)requested_frequency * period;
        uint64_t divider =
            (scaled_source + requested_denominator / 2u) /
            requested_denominator;
        if (divider < minimum_divider || divider > maximum_divider) continue;

        uint64_t actual_denominator = period * divider;
        if (actual_denominator == 0u ||
            scaled_source % actual_denominator != 0u ||
            scaled_source / actual_denominator != requested_frequency) {
            continue;
        }

        mcujs_pwm_esp_frequency_config_t candidate = {
            .resolution = resolution,
            .period = (uint32_t)period,
            .divider = (uint32_t)divider,
            .actual_frequency = requested_frequency,
        };
        *config = candidate;
        return true;
    }
    return false;
}

bool mcujs_pwm_ratio_to_level(double ratio, uint32_t period,
                              uint32_t maximum_level, uint32_t *level) {
    if (level == NULL || period == 0u || !isfinite(ratio) || ratio < 0.0 ||
        ratio > 1.0) {
        return false;
    }

    double scaled = ratio * (double)period;
    uint64_t candidate = (uint64_t)(scaled + 0.5);
    if (candidate > period || candidate > maximum_level ||
        (double)candidate / (double)period != ratio) {
        return false;
    }

    *level = (uint32_t)candidate;
    return true;
}

bool mcujs_pwm_fixed_timer_accepts(uint16_t references, bool current_owner,
                                   uint32_t configured_frequency,
                                   uint32_t requested_frequency) {
    uint16_t own_reference = current_owner ? 1u : 0u;
    return references <= own_reference ||
           configured_frequency == requested_frequency;
}

int mcujs_pwm_select_timer(const mcujs_pwm_timer_resource_t timers[],
                           size_t timer_count, uint32_t requested_frequency,
                           int current_timer) {
    if (timers == NULL) return -1;

    for (size_t i = 0; i < timer_count; i++) {
        if (timers[i].used && !timers[i].pending_cleanup &&
            timers[i].frequency == requested_frequency) {
            return (int)i;
        }
    }
    for (size_t i = 0; i < timer_count; i++) {
        if (!timers[i].used) return (int)i;
    }
    if (current_timer >= 0 && (size_t)current_timer < timer_count &&
        timers[current_timer].used && timers[current_timer].references == 1u) {
        return current_timer;
    }
    return -1;
}
