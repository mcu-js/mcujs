#include "pwm_policy.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static void test_exact_ratio_scaling(void) {
    uint32_t level = UINT32_MAX;

    assert(mcujs_pwm_ratio_to_level(0.0, 62500u, UINT16_MAX, &level));
    assert(level == 0u);
    assert(mcujs_pwm_ratio_to_level(0.5, 62500u, UINT16_MAX, &level));
    assert(level == 31250u);
    assert(mcujs_pwm_ratio_to_level(1.0, 62500u, UINT16_MAX, &level));
    assert(level == 62500u);

    level = 123u;
    assert(!mcujs_pwm_ratio_to_level(1.0 / 3.0, 62500u, UINT16_MAX,
                                     &level));
    assert(level == 123u);
    assert(!mcujs_pwm_ratio_to_level(NAN, 62500u, UINT16_MAX, &level));
    assert(!mcujs_pwm_ratio_to_level(INFINITY, 62500u, UINT16_MAX, &level));
    assert(!mcujs_pwm_ratio_to_level(-0.01, 62500u, UINT16_MAX, &level));
    assert(!mcujs_pwm_ratio_to_level(1.01, 62500u, UINT16_MAX, &level));
    assert(!mcujs_pwm_ratio_to_level(0.5, 0u, UINT16_MAX, &level));
    assert(!mcujs_pwm_ratio_to_level(1.0, 65536u, UINT16_MAX, &level));
}

static void test_rp_exact_frequency_and_scaling(void) {
    mcujs_pwm_rp_frequency_config_t config = {0};
    assert(mcujs_pwm_rp_find_exact_frequency(125000000u, 1250u, &config));
    assert(config.divider_scaled == 25u);
    assert(config.wrap == 63999u);

    uint32_t level = UINT32_MAX;
    assert(mcujs_pwm_ratio_to_level(0.0, 64000u, UINT16_MAX, &level));
    assert(level == 0u);
    assert(mcujs_pwm_ratio_to_level(1.0 / 64.0, 64000u, UINT16_MAX,
                                    &level));
    assert(level == 1000u);
    assert(mcujs_pwm_ratio_to_level(0.5, 64000u, UINT16_MAX, &level));
    assert(level == 32000u);
    assert(mcujs_pwm_ratio_to_level(1.0, 64000u, UINT16_MAX, &level));
    assert(level == 64000u);

    assert(mcujs_pwm_rp_find_exact_frequency(150000000u, 1250u, &config));
    assert(config.divider_scaled == 30u);
    assert(config.wrap == 63999u);

    config = (mcujs_pwm_rp_frequency_config_t){
        .divider_scaled = 123u,
        .wrap = 456u,
    };
    assert(!mcujs_pwm_rp_find_exact_frequency(125000000u, 1100u, &config));
    assert(config.divider_scaled == 123u);
    assert(config.wrap == 456u);
}

static void test_esp_exact_frequency_and_scaling(void) {
    mcujs_pwm_esp_frequency_config_t config = {0};
    assert(mcujs_pwm_esp_find_exact_frequency(
        80000000u, 1250u, 14u, 13u, 8u, 1u << 8u, 0x3ffffu,
        &config));
    assert(config.resolution == 13u);
    assert(config.period == 8192u);
    assert(config.divider == 2000u);
    assert(config.actual_frequency == 1250u);

    uint32_t level = UINT32_MAX;
    assert(mcujs_pwm_ratio_to_level(0.0, config.period, UINT32_MAX, &level));
    assert(level == 0u);
    assert(mcujs_pwm_ratio_to_level(1.0 / 64.0, config.period,
                                    UINT32_MAX, &level));
    assert(level == 128u);
    assert(mcujs_pwm_ratio_to_level(0.5, config.period, UINT32_MAX, &level));
    assert(level == 4096u);
    assert(mcujs_pwm_ratio_to_level(1.0, config.period, UINT32_MAX, &level));
    assert(level == 8192u);

    assert(mcujs_pwm_esp_find_exact_frequency(
        80000000u, 1600u, 14u, 13u, 8u, 1u << 8u, 0x3ffffu,
        &config));
    assert(config.resolution == 12u);
    assert(config.period == 4096u);
    assert(config.divider == 3125u);
    assert(config.actual_frequency == 1600u);

    /* IDF's highest suitable resolution is 13, where the rounded divider 313
     * is inexact. Resolution 12 is the highest exact option (divider 625). */
    assert(mcujs_pwm_esp_find_exact_frequency(
        80000000u, 8000u, 13u, 13u, 8u, 1u << 8u, 0x3ffffu,
        &config));
    assert(config.resolution == 12u);
    assert(config.period == 4096u);
    assert(config.divider == 625u);
    assert(config.actual_frequency == 8000u);

    config = (mcujs_pwm_esp_frequency_config_t){
        .resolution = 3u,
        .period = 4u,
        .divider = 5u,
        .actual_frequency = 6u,
    };

    /* The LEDC integer getter rounds this ~10.9999868 Hz configuration to
     * 11 Hz, but the rational hardware frequency is not exact. */
    assert(!mcujs_pwm_esp_find_exact_frequency(
        80000000u, 11u, 14u, 13u, 8u, 1u << 8u, 0x3ffffu,
        &config));
    assert(config.resolution == 3u);
    assert(config.period == 4u);
    assert(config.divider == 5u);
    assert(config.actual_frequency == 6u);

    assert(!mcujs_pwm_esp_find_exact_frequency(
        80000000u, 9u, 14u, 13u, 8u, 1u << 8u, 0x3ffffu,
        &config));
}

static void test_fixed_timer_sharing(void) {
    assert(mcujs_pwm_fixed_timer_accepts(0u, false, 0u, 1000u));
    assert(mcujs_pwm_fixed_timer_accepts(1u, false, 1000u, 1000u));
    assert(!mcujs_pwm_fixed_timer_accepts(1u, false, 1000u, 1250u));
    assert(mcujs_pwm_fixed_timer_accepts(1u, true, 1000u, 1250u));
    assert(mcujs_pwm_fixed_timer_accepts(2u, true, 1000u, 1000u));
    assert(!mcujs_pwm_fixed_timer_accepts(2u, true, 1000u, 1250u));
}

static void test_allocated_timer_selection(void) {
    mcujs_pwm_timer_resource_t timers[4] = {
        {.used = true, .frequency = 1000u, .references = 2u},
        {.used = true, .frequency = 2000u, .references = 1u},
        {0},
        {.used = true, .frequency = 3000u, .references = 1u},
    };

    assert(mcujs_pwm_select_timer(timers, 4u, 1000u, -1) == 0);
    assert(mcujs_pwm_select_timer(timers, 4u, 4000u, -1) == 2);

    timers[2] = (mcujs_pwm_timer_resource_t){
        .used = true,
        .frequency = 4000u,
        .references = 1u,
    };
    assert(mcujs_pwm_select_timer(timers, 4u, 5000u, 1) == 1);
    assert(mcujs_pwm_select_timer(timers, 4u, 5000u, 0) == -1);
    assert(mcujs_pwm_select_timer(timers, 4u, 3000u, 0) == 3);
    assert(mcujs_pwm_select_timer(timers, 4u, 5000u, -1) == -1);

    timers[0].pending_cleanup = true;
    assert(mcujs_pwm_select_timer(timers, 4u, 1000u, -1) == -1);
    timers[0].references = 1u;
    assert(mcujs_pwm_select_timer(timers, 4u, 5000u, 0) == 0);
}

int main(void) {
    test_exact_ratio_scaling();
    test_rp_exact_frequency_and_scaling();
    test_esp_exact_frequency_and_scaling();
    test_fixed_timer_sharing();
    test_allocated_timer_selection();
    puts("portable PWM scaling and resource policy passed");
    return 0;
}
