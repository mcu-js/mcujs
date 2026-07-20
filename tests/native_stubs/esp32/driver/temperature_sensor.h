#ifndef MCUJS_TEST_ESP_TEMPERATURE_SENSOR_H
#define MCUJS_TEST_ESP_TEMPERATURE_SENSOR_H

#include "esp_err.h"

typedef struct mcujs_test_temperature_sensor *temperature_sensor_handle_t;

typedef struct {
    int range_min;
    int range_max;
} temperature_sensor_config_t;

#define TEMPERATURE_SENSOR_CONFIG_DEFAULT(minimum, maximum) \
    ((temperature_sensor_config_t){.range_min = (minimum), .range_max = (maximum)})

esp_err_t temperature_sensor_install(const temperature_sensor_config_t *config,
                                     temperature_sensor_handle_t *handle);
esp_err_t temperature_sensor_enable(temperature_sensor_handle_t handle);
esp_err_t temperature_sensor_get_celsius(temperature_sensor_handle_t handle,
                                         float *celsius);
esp_err_t temperature_sensor_disable(temperature_sensor_handle_t handle);

#endif
