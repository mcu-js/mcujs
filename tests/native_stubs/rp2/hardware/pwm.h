#ifndef MCUJS_TEST_RP2_PWM_H
#define MCUJS_TEST_RP2_PWM_H

#include "pico/stdlib.h"

#include <stdint.h>

uint pwm_gpio_to_slice_num(uint pin);
void pwm_set_clkdiv_int_frac(uint slice, uint8_t div_int, uint8_t div_frac4);
void pwm_set_wrap(uint slice, uint16_t wrap);
void pwm_set_gpio_level(uint pin, uint16_t level);
void pwm_set_enabled(uint slice, bool enabled);

#endif