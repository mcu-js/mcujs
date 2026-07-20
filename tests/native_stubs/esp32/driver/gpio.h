#ifndef MCUJS_TEST_ESP_GPIO_H
#define MCUJS_TEST_ESP_GPIO_H

#include "esp_err.h"

#include <stdbool.h>

typedef int gpio_num_t;

#define GPIO_NUM_MAX 49
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0 && (pin) < GPIO_NUM_MAX)
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) GPIO_IS_VALID_GPIO(pin)

#define GPIO_MODE_INPUT 1
#define GPIO_MODE_INPUT_OUTPUT 2
#define GPIO_FLOATING 0
#define GPIO_PULLUP_ONLY 1
#define GPIO_PULLDOWN_ONLY 2

esp_err_t gpio_reset_pin(gpio_num_t pin);
esp_err_t gpio_set_direction(gpio_num_t pin, int mode);
esp_err_t gpio_set_pull_mode(gpio_num_t pin, int mode);
esp_err_t gpio_set_level(gpio_num_t pin, int level);
int gpio_get_level(gpio_num_t pin);

#endif
