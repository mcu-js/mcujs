#ifndef MCUJS_TEST_ESP_ADC_ONESHOT_H
#define MCUJS_TEST_ESP_ADC_ONESHOT_H

#include "esp_err.h"

#include <stdbool.h>

typedef struct mcujs_test_adc_unit *adc_oneshot_unit_handle_t;
typedef int adc_channel_t;
typedef int adc_unit_t;
typedef int adc_atten_t;
typedef int adc_bitwidth_t;

enum {
    ADC_UNIT_1 = 0,
    ADC_ULP_MODE_DISABLE = 0,
    ADC_CHANNEL_0 = 0,
    ADC_BITWIDTH_12 = 12,
    ADC_ATTEN_DB_12 = 12,
};

typedef struct {
    adc_unit_t unit_id;
    int ulp_mode;
} adc_oneshot_unit_init_cfg_t;

typedef struct {
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
} adc_oneshot_chan_cfg_t;

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *config,
                               adc_oneshot_unit_handle_t *handle);
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle,
                                     adc_channel_t channel,
                                     const adc_oneshot_chan_cfg_t *config);
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle,
                           adc_channel_t channel, int *raw);

#endif
