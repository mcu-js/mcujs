#ifndef MCUJS_TEST_RP2_STDLIB_H
#define MCUJS_TEST_RP2_STDLIB_H

#include <stdbool.h>
#include <stdint.h>

typedef unsigned int uint;

#define NUM_BANK0_GPIOS 30u
#define GPIO_OUT true
#define GPIO_IN false
#define GPIO_FUNC_I2C 3u
#define GPIO_FUNC_PWM 4u
#define GPIO_FUNC_SIO 5u

void gpio_init(uint pin);
void gpio_set_dir(uint pin, bool output);
void gpio_disable_pulls(uint pin);
void gpio_pull_up(uint pin);
void gpio_pull_down(uint pin);
void gpio_put(uint pin, bool value);
bool gpio_get(uint pin);
void gpio_set_function(uint pin, uint function);

#endif
