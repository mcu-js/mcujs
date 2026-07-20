#ifndef MCUJS_TEST_ESP_ADC_CALI_SCHEME_H
#define MCUJS_TEST_ESP_ADC_CALI_SCHEME_H

#include "esp_adc/adc_cali.h"

typedef struct {
    adc_unit_t unit_id;
    adc_channel_t chan;
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
} adc_cali_curve_fitting_config_t;

esp_err_t adc_cali_create_scheme_curve_fitting(
    const adc_cali_curve_fitting_config_t *config,
    adc_cali_handle_t *handle);

#endif
