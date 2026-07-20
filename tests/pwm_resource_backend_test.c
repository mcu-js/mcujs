#include "bindings.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool eval_source(const char *source) {
    jerry_value_t result = jerry_eval((const jerry_char_t *)source,
                                      strlen(source), JERRY_PARSE_NO_OPTS);
    if (jerry_value_is_exception(result)) {
        jerry_value_t error = jerry_exception_value(result, true);
        jerry_value_t text = jerry_value_to_string(error);
        char buffer[256];
        jerry_size_t size = jerry_string_size(text, JERRY_ENCODING_UTF8);
        if (size >= sizeof(buffer)) size = sizeof(buffer) - 1;
        jerry_string_to_buffer(text, JERRY_ENCODING_UTF8,
                               (jerry_char_t *)buffer, size);
        buffer[size] = '\0';
        fprintf(stderr, "PWM resource test exception: %s\n", buffer);
        jerry_value_free(text);
        jerry_value_free(error);
        return false;
    }
    jerry_value_free(result);
    return true;
}

static void install_module(const char *name, jerry_value_t module) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t result = jerry_object_set_sz(global, name, module);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(global);
    jerry_value_free(module);
}

static bool assert_operational_error(const char *operation, const char *name,
                                     const char *code, int limit) {
    char source[1024];
    const char *resource = strncmp(operation, "GPIO.", 5) == 0 ? "gpio" : "pwm";
    int written = snprintf(
        source, sizeof(source),
        "(function () { var error; try { %s; } catch (caught) { error = caught; } "
        "if (!(error instanceof Error)) throw new Error('missing %s'); "
        "if (error.name !== '%s') throw new Error('wrong name: ' + error.name); "
        "if (error.code !== '%s') throw new Error('wrong code: ' + error.code); "
        "if (error.resource !== '%s') throw new Error('wrong resource'); "
        "if (%d >= 0 && error.limit !== %d) throw new Error('wrong limit: ' + error.limit); "
        "}());",
        operation, code, name, code, resource, limit, limit);
    return written > 0 && (size_t)written < sizeof(source) && eval_source(source);
}

#if defined(MCUJS_PLATFORM_ESP32)
static void test_esp_sharing_and_exhaustion(void) {
    assert(eval_source("PWM.init(1, 1250); PWM.init(2, 1250);"));
    assert(mcujs_test_ledc_timer_config_calls == 1u);
    assert(mcujs_test_ledc_channel_config_calls == 2u);

    assert(eval_source("PWM.setDuty(1, 1 / 64); PWM.setDuty(2, 0.5);"));
    assert(mcujs_test_ledc_duty == 4096u);
    assert(eval_source("PWM.stop(1);"));
    assert(mcujs_test_ledc_timer_pause_calls == 0u);
    assert(mcujs_test_ledc_timer_deconfigure_calls == 0u);
    assert(eval_source("PWM.setDuty(2, 1 / 64);"));
    assert(mcujs_test_ledc_duty == 128u);
    assert(eval_source("PWM.stop(2);"));
    assert(mcujs_test_ledc_timer_pause_calls == 1u);
    assert(mcujs_test_ledc_timer_deconfigure_calls == 1u);
    assert(mcujs_test_gpio_is_output(1));
    assert(mcujs_test_gpio_level_at(1) == 0);
    assert(mcujs_test_gpio_pull_at(1) == 0);
    assert(mcujs_test_gpio_is_output(2));
    assert(mcujs_test_gpio_level_at(2) == 0);
    assert(mcujs_test_gpio_pull_at(2) == 0);
    assert(eval_source("GPIO.init(1, GPIO.OUTPUT); GPIO.init(2, GPIO.OUTPUT);"));

    assert(eval_source(
        "PWM.init(1, 1250); PWM.init(2, 1250); PWM.init(3, 1250); "
        "PWM.init(4, 1250); PWM.init(5, 1250); PWM.init(6, 1250); "
        "PWM.init(7, 1250); PWM.init(8, 1250);"));
    unsigned timer_calls = mcujs_test_ledc_timer_config_calls;
    unsigned channel_calls = mcujs_test_ledc_channel_config_calls;
    unsigned stop_calls = mcujs_test_ledc_stop_calls;
    unsigned reset_calls = mcujs_test_gpio_reset_calls;
    assert(assert_operational_error("PWM.init(9, 1250)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED", 8));
    assert(mcujs_test_ledc_timer_config_calls == timer_calls);
    assert(mcujs_test_ledc_channel_config_calls == channel_calls);
    assert(mcujs_test_ledc_stop_calls == stop_calls);
    assert(mcujs_test_gpio_reset_calls == reset_calls);
    assert(eval_source("PWM.stop(1); PWM.init(9, 1250);"));
    assert(eval_source(
        "PWM.stop(2); PWM.stop(3); PWM.stop(4); PWM.stop(5); "
        "PWM.stop(6); PWM.stop(7); PWM.stop(8); PWM.stop(9);"));

    assert(eval_source(
        "PWM.init(1, 1000); PWM.init(2, 1250); "
        "PWM.init(3, 2000); PWM.init(4, 4000);"));
    timer_calls = mcujs_test_ledc_timer_config_calls;
    channel_calls = mcujs_test_ledc_channel_config_calls;
    stop_calls = mcujs_test_ledc_stop_calls;
    reset_calls = mcujs_test_gpio_reset_calls;
    assert(assert_operational_error("PWM.init(5, 5000)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED", 4));
    assert(mcujs_test_ledc_timer_config_calls == timer_calls);
    assert(mcujs_test_ledc_channel_config_calls == channel_calls);
    assert(mcujs_test_ledc_stop_calls == stop_calls);
    assert(mcujs_test_gpio_reset_calls == reset_calls);
    assert(eval_source("PWM.stop(1); PWM.stop(2); PWM.stop(3); PWM.stop(4);"));
}

static void test_esp_reinit_and_retryable_failures(void) {
    assert(eval_source("PWM.init(1, 1250); PWM.setDuty(1, 1 / 64);"));
    assert(mcujs_test_ledc_duty == 128u);
    assert(eval_source("PWM.init(1, 1000);"));
    assert(mcujs_test_ledc_duty == 0u);
    assert(eval_source("PWM.setDuty(1, 1 / 64);"));
    assert(mcujs_test_ledc_duty == 128u);

    unsigned timer_calls = mcujs_test_ledc_timer_config_calls;
    unsigned channel_calls = mcujs_test_ledc_channel_config_calls;
    unsigned stop_calls = mcujs_test_ledc_stop_calls;
    unsigned reset_calls = mcujs_test_gpio_reset_calls;
    unsigned duty_calls = mcujs_test_ledc_duty_calls;
    assert(assert_operational_error("PWM.init(1, 11)",
                                    "NotSupportedError",
                                    "ERR_NOT_SUPPORTED", -1));
    assert(mcujs_test_ledc_timer_config_calls == timer_calls);
    assert(mcujs_test_ledc_channel_config_calls == channel_calls);
    assert(mcujs_test_ledc_stop_calls == stop_calls);
    assert(mcujs_test_gpio_reset_calls == reset_calls);
    assert(mcujs_test_ledc_duty_calls == duty_calls);
    assert(eval_source("PWM.setDuty(1, 0.5);"));
    assert(mcujs_test_ledc_duty == 4096u);
    duty_calls = mcujs_test_ledc_duty_calls;

    assert(assert_operational_error("PWM.init(1, 1601)",
                                    "NotSupportedError",
                                    "ERR_NOT_SUPPORTED", -1));
    assert(mcujs_test_ledc_timer_config_calls == timer_calls);
    assert(mcujs_test_ledc_channel_config_calls == channel_calls);
    assert(mcujs_test_ledc_stop_calls == stop_calls);
    assert(mcujs_test_gpio_reset_calls == reset_calls);
    assert(mcujs_test_ledc_duty_calls == duty_calls);
    assert(eval_source("PWM.setDuty(1, 0.5);"));
    assert(mcujs_test_ledc_duty == 4096u);
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(eval_source("PWM.stop(1);"));

    assert(eval_source("PWM.init(1, 8000);"));
    assert(mcujs_test_ledc_configured_resolution == 12u);
    assert(eval_source("PWM.setDuty(1, 1 / 64);"));
    assert(mcujs_test_ledc_duty == 64u);
    assert(eval_source("PWM.stop(1);"));

    assert(eval_source("PWM.init(1, 1250);"));
    mcujs_test_ledc_stop_result = 0x103;
    assert(assert_operational_error("PWM.stop(1)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));
    mcujs_test_ledc_stop_result = 0;
    assert(eval_source("PWM.stop(1);"));

    assert(eval_source("PWM.init(1, 1250);"));
    mcujs_test_gpio_reset_result = 0x103;
    assert(assert_operational_error("PWM.stop(1)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));
    mcujs_test_gpio_reset_result = 0;
    assert(eval_source("PWM.stop(1);"));

    assert(eval_source("PWM.init(1, 1250);"));
    mcujs_test_gpio_result = -1;
    assert(assert_operational_error("PWM.stop(1)",
                                    "Error", "EIO", -1));
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));
    mcujs_test_gpio_result = 0;
    assert(eval_source("PWM.stop(1);"));
    assert(mcujs_test_gpio_is_output(1));
    assert(mcujs_test_gpio_level_at(1) == 0);
    assert(mcujs_test_gpio_pull_at(1) == 0);

    assert(eval_source("PWM.init(1, 1250);"));
    mcujs_test_ledc_pause_result = 0x103;
    assert(assert_operational_error("PWM.stop(1)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));
    mcujs_test_ledc_pause_result = 0;
    assert(eval_source("PWM.stop(1);"));

    mcujs_test_ledc_timer_result = 0x102;
    mcujs_test_gpio_reset_result = 0x103;
    assert(assert_operational_error("PWM.init(1, 1250)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));
    mcujs_test_ledc_timer_result = 0;
    mcujs_test_gpio_reset_result = 0;
    assert(eval_source("PWM.init(1, 1250); PWM.stop(1);"));
}

static void test_esp_final_stop_deconfigure_retry(void) {
    assert(eval_source(
        "PWM.init(1, 1250); PWM.init(2, 1000); "
        "PWM.init(3, 2000); PWM.init(4, 4000);"));

    mcujs_test_ledc_timer_deconfigure_result = 0x103;
    unsigned timer_calls = mcujs_test_ledc_timer_config_calls;
    unsigned channel_calls = mcujs_test_ledc_channel_config_calls;
    unsigned stop_calls = mcujs_test_ledc_stop_calls;
    unsigned reset_calls = mcujs_test_gpio_reset_calls;
    unsigned pause_calls = mcujs_test_ledc_timer_pause_calls;
    unsigned deconfigure_calls = mcujs_test_ledc_timer_deconfigure_calls;
    assert(assert_operational_error("PWM.stop(1)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(mcujs_test_ledc_stop_calls == stop_calls + 1u);
    assert(mcujs_test_gpio_reset_calls == reset_calls + 1u);
    assert(mcujs_test_ledc_timer_pause_calls == pause_calls + 1u);
    assert(mcujs_test_ledc_timer_deconfigure_calls ==
           deconfigure_calls + 1u);
    assert(mcujs_test_ledc_timer_config_calls == timer_calls + 1u);

    timer_calls = mcujs_test_ledc_timer_config_calls;
    channel_calls = mcujs_test_ledc_channel_config_calls;
    stop_calls = mcujs_test_ledc_stop_calls;
    reset_calls = mcujs_test_gpio_reset_calls;
    assert(assert_operational_error("PWM.init(5, 1250)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED", 4));
    assert(mcujs_test_ledc_timer_config_calls == timer_calls);
    assert(mcujs_test_ledc_channel_config_calls == channel_calls);
    assert(mcujs_test_ledc_stop_calls == stop_calls);
    assert(mcujs_test_gpio_reset_calls == reset_calls);
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));

    mcujs_test_ledc_timer_deconfigure_result = 0;
    assert(eval_source("PWM.stop(1); PWM.init(5, 1250);"));
    assert(mcujs_test_ledc_timer_config_calls == timer_calls + 2u);
    assert(mcujs_test_ledc_channel_config_calls == channel_calls + 1u);
    assert(assert_operational_error("PWM.init(6, 5000)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED", 4));
    assert(eval_source(
        "PWM.stop(2); PWM.stop(3); PWM.stop(4); PWM.stop(5); "
        "GPIO.init(1, GPIO.OUTPUT);"));
}

static void test_esp_channel_rollback_retry(void) {
    assert(eval_source("PWM.init(1, 1000); PWM.setDuty(1, 0.5);"));
    assert(mcujs_test_ledc_duty == 4096u);

    mcujs_test_ledc_channel_result = -1;
    mcujs_test_ledc_timer_deconfigure_result = 0x103;
    assert(assert_operational_error("PWM.init(2, 1250)",
                                    "ResourceBusyError", "EBUSY", -1));
    unsigned failed_timer_calls = mcujs_test_ledc_timer_config_calls;
    unsigned failed_channel_calls = mcujs_test_ledc_channel_config_calls;
    assert(mcujs_test_ledc_timer_deconfigure_calls > 0u);
    assert(assert_operational_error("GPIO.init(2, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(eval_source("PWM.setDuty(1, 1 / 64);"));
    assert(mcujs_test_ledc_duty == 128u);

    mcujs_test_ledc_channel_result = 0;
    mcujs_test_ledc_timer_deconfigure_result = 0;
    assert(eval_source("PWM.init(2, 1250);"));
    /* Retry must deconfigure the retained uncertain timer and configure it
     * again rather than attaching to stale paused state. */
    assert(mcujs_test_ledc_timer_config_calls == failed_timer_calls + 2u);
    assert(mcujs_test_ledc_channel_config_calls == failed_channel_calls + 1u);
    assert(eval_source("PWM.stop(2); PWM.setDuty(1, 0.5); PWM.stop(1);"));
    assert(eval_source("GPIO.init(1, GPIO.OUTPUT); GPIO.init(2, GPIO.OUTPUT);"));

    /* Final cleanup recovers all four timer resources; no orphan can cause
     * false exhaustion. */
    assert(eval_source(
        "PWM.init(1, 1000); PWM.init(2, 1250); "
        "PWM.init(3, 2000); PWM.init(4, 4000);"));
    assert(eval_source("PWM.stop(1); PWM.stop(2); PWM.stop(3); PWM.stop(4);"));
}

static void test_esp_mismatch_rollback_retry(void) {
    mcujs_test_ledc_actual_frequency = 1249u;
    mcujs_test_ledc_timer_deconfigure_result = 0x103;
    assert(assert_operational_error("PWM.init(1, 1250)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(mcujs_test_ledc_timer_deconfigure_calls > 0u);
    assert(assert_operational_error("GPIO.init(1, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY", -1));

    mcujs_test_ledc_actual_frequency = 0u;
    unsigned failed_config_calls = mcujs_test_ledc_timer_config_calls;
    assert(eval_source(
        "PWM.init(2, 1250); PWM.init(3, 2000); PWM.init(4, 4000);"));
    assert(mcujs_test_ledc_timer_config_calls == failed_config_calls + 3u);
    assert(assert_operational_error("PWM.init(5, 5000)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED", 4));

    mcujs_test_ledc_timer_deconfigure_result = 0;
    assert(eval_source(
        "PWM.stop(1); PWM.stop(2); PWM.stop(3); PWM.stop(4);"));
    assert(mcujs_test_gpio_is_output(1));
    assert(mcujs_test_gpio_level_at(1) == 0);
    assert(mcujs_test_gpio_pull_at(1) == 0);
    assert(eval_source("GPIO.init(1, GPIO.OUTPUT);"));

    /* Cleanup of the uncertain timer must restore all four timer slots. */
    assert(eval_source(
        "PWM.init(2, 1000); PWM.init(3, 1250); "
        "PWM.init(4, 2000); PWM.init(5, 4000);"));
    assert(assert_operational_error("PWM.init(6, 5000)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED", 4));
    assert(eval_source("PWM.stop(2); PWM.init(6, 5000);"));
    assert(eval_source(
        "PWM.stop(3); PWM.stop(4); PWM.stop(5); PWM.stop(6);"));
}
#else
static void test_rp_resources_and_reinit(void) {
    assert(eval_source("PWM.init(0, 1250); PWM.init(1, 1250);"));
    unsigned disable_calls = mcujs_test_pwm_disable_calls;
    assert(eval_source("PWM.stop(0);"));
    assert(mcujs_test_pwm_disable_calls == disable_calls);
    assert(eval_source("PWM.setDuty(1, 1 / 64);"));
    assert(mcujs_test_pwm_level == 1000u);
    assert(eval_source("PWM.stop(1);"));
    assert(mcujs_test_pwm_disable_calls == disable_calls + 1u);
    assert(mcujs_test_gpio_is_output(0));
    assert(mcujs_test_gpio_level_at(0) == 0);
    assert(mcujs_test_gpio_pull_at(0) == 0);
    assert(mcujs_test_gpio_is_output(1));
    assert(mcujs_test_gpio_level_at(1) == 0);
    assert(mcujs_test_gpio_pull_at(1) == 0);
    assert(eval_source("GPIO.init(0, GPIO.OUTPUT); GPIO.set(0, true);"));
    assert(mcujs_test_gpio_level_at(0) == 1);
    assert(eval_source("PWM.init(0, 1250); PWM.stop(0);"));
    assert(mcujs_test_gpio_is_output(0));
    assert(mcujs_test_gpio_level_at(0) == 0);

    assert(eval_source("PWM.init(0, 1250);"));
    unsigned config_calls = mcujs_test_pwm_config_calls;
    unsigned duty_calls = mcujs_test_pwm_duty_calls;
    unsigned enable_calls = mcujs_test_pwm_enable_calls;
    assert(assert_operational_error("PWM.init(16, 1250)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(mcujs_test_pwm_config_calls == config_calls);
    assert(mcujs_test_pwm_duty_calls == duty_calls);
    assert(mcujs_test_pwm_enable_calls == enable_calls);
    assert(eval_source("PWM.stop(0); PWM.init(16, 1250); PWM.stop(16);"));

    assert(eval_source(
        "PWM.init(0, 1250); PWM.init(1, 1250); PWM.init(2, 1250); "
        "PWM.init(3, 1250); PWM.init(4, 1250); PWM.init(5, 1250); "
        "PWM.init(6, 1250); PWM.init(7, 1250); PWM.init(8, 1250); "
        "PWM.init(9, 1250); PWM.init(10, 1250); PWM.init(11, 1250); "
        "PWM.init(12, 1250); PWM.init(13, 1250); PWM.init(14, 1250); "
        "PWM.init(15, 1250);"));
    config_calls = mcujs_test_pwm_config_calls;
    assert(assert_operational_error("PWM.init(16, 1250)",
                                    "ResourceBusyError", "EBUSY", -1));
    assert(mcujs_test_pwm_config_calls == config_calls);
    assert(eval_source("PWM.stop(0); PWM.init(16, 1250);"));
    assert(eval_source(
        "PWM.stop(1); PWM.stop(2); PWM.stop(3); PWM.stop(4); "
        "PWM.stop(5); PWM.stop(6); PWM.stop(7); PWM.stop(8); "
        "PWM.stop(9); PWM.stop(10); PWM.stop(11); PWM.stop(12); "
        "PWM.stop(13); PWM.stop(14); PWM.stop(15); PWM.stop(16);"));

    assert(eval_source("PWM.init(2, 1250); PWM.setDuty(2, 1 / 64);"));
    assert(mcujs_test_pwm_level == 1000u);
    assert(eval_source("PWM.init(2, 1000);"));
    assert(mcujs_test_pwm_level == 0u);
    assert(eval_source("PWM.setDuty(2, 0.5);"));
    assert(mcujs_test_pwm_level == (mcujs_test_pwm_wrap + 1u) / 2u);
    config_calls = mcujs_test_pwm_config_calls;
    duty_calls = mcujs_test_pwm_duty_calls;
    unsigned gpio_calls = mcujs_test_gpio_init_calls;
    enable_calls = mcujs_test_pwm_enable_calls;
    assert(assert_operational_error("PWM.init(2, 1100)",
                                    "NotSupportedError",
                                    "ERR_NOT_SUPPORTED", -1));
    assert(mcujs_test_pwm_config_calls == config_calls);
    assert(mcujs_test_pwm_duty_calls == duty_calls);
    assert(mcujs_test_gpio_init_calls == gpio_calls);
    assert(mcujs_test_pwm_enable_calls == enable_calls);
    assert(eval_source("PWM.setDuty(2, 0.5); PWM.stop(2);"));
}
#endif

int main(void) {
    mcujs_test_reset_backend();
    jerry_init(JERRY_INIT_EMPTY);
    install_module("PWM", js_create_pwm_module());
    install_module("GPIO", js_create_gpio_module());

#if defined(MCUJS_PLATFORM_ESP32)
    test_esp_sharing_and_exhaustion();
    test_esp_reinit_and_retryable_failures();
    test_esp_final_stop_deconfigure_retry();
    test_esp_channel_rollback_retry();
    test_esp_mismatch_rollback_retry();
    const char *backend = "esp32";
#else
    test_rp_resources_and_reinit();
#if defined(MCUJS_BOARD_PICO2)
    const char *backend = "rp2350";
#else
    const char *backend = "rp2040";
#endif
#endif

    jerry_cleanup();
    printf("PWM resource/lifecycle contract passed for %s\n", backend);
    return 0;
}
