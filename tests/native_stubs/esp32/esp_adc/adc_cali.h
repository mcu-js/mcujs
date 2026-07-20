#ifndef MCUJS_TEST_ESP_ADC_CALI_H
#define MCUJS_TEST_ESP_ADC_CALI_H

#include "esp_adc/adc_oneshot.h"

typedef struct mcujs_test_adc_cali *adc_cali_handle_t;

esp_err_t adc_oneshot_get_calibrated_result(adc_oneshot_unit_handle_t unit,
                                             adc_cali_handle_t calibration,
                                             adc_channel_t channel,
                                             int *millivolts);

#endif
