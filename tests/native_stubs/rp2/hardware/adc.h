#ifndef MCUJS_TEST_RP2_ADC_H
#define MCUJS_TEST_RP2_ADC_H

#include "pico/stdlib.h"

#include <stdbool.h>
#include <stdint.h>

void adc_init(void);
void adc_gpio_init(uint pin);
void adc_select_input(uint channel);
uint16_t adc_read(void);
void adc_set_temp_sensor_enabled(bool enabled);

#endif
