#ifndef SD_TEST_PICO_H
#define SD_TEST_PICO_H
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;
#define GPIO_OUT 1
#define GPIO_FUNC_SPI 1
uint64_t time_us_64(void);
void sleep_ms(uint ms);
void gpio_init(uint pin);
void gpio_set_dir(uint pin, bool output);
void gpio_put(uint pin, bool value);
void gpio_set_function(uint pin, uint function);
void gpio_pull_up(uint pin);
#endif
