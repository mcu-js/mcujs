#ifndef MCUJS_RUNTIME_VALIDATION_BACKEND_STUBS_H
#define MCUJS_RUNTIME_VALIDATION_BACKEND_STUBS_H

#include <stddef.h>

extern int mcujs_test_i2c_result;
extern unsigned mcujs_test_i2c_write_calls;
extern unsigned mcujs_test_i2c_read_calls;
extern size_t mcujs_test_i2c_last_length;
#if defined(MCUJS_PLATFORM_RP2)
extern unsigned mcujs_test_gpio_init_calls;
extern unsigned mcujs_test_i2c_init_calls;
extern unsigned mcujs_test_pwm_config_calls;
extern unsigned mcujs_test_pwm_divider_scaled;
extern unsigned mcujs_test_pwm_wrap;
extern unsigned mcujs_test_pwm_duty_calls;
extern unsigned mcujs_test_pwm_level;
#endif
#if defined(MCUJS_PLATFORM_ESP32)
extern unsigned mcujs_test_i2c_param_config_calls;
extern unsigned mcujs_test_i2c_driver_install_calls;
extern unsigned mcujs_test_i2c_driver_delete_calls;
extern int mcujs_test_ledc_result;
extern unsigned mcujs_test_ledc_resolution;
extern unsigned mcujs_test_ledc_configured_resolution;
extern unsigned mcujs_test_ledc_actual_frequency;
extern unsigned mcujs_test_ledc_duty_calls;
extern unsigned mcujs_test_ledc_duty;
#endif

void mcujs_test_reset_backend(void);

#endif
