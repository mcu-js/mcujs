/* MCU.js ADC binding for ESP32-S3 ADC1 and temperature sensor. */

#include "binding_utils.h"
#include "bindings.h"
#include "jerryscript.h"
#include "pin_policy.h"

#include "driver/temperature_sensor.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

static adc_oneshot_unit_handle_t s_adc_unit;
static adc_cali_handle_t s_adc_cali;
static temperature_sensor_handle_t s_temp_sensor;

static bool adc_pin_to_channel(int pin, adc_channel_t *channel) {
    if (pin < 1 || pin > 9) {
        return false;
    }
    *channel = (adc_channel_t)(pin - 1);
    return true;
}

static bool adc_number_to_channel(int number, adc_channel_t *channel, int *pin) {
    if (number < 0 || number > 8) {
        return false;
    }
    *channel = (adc_channel_t)number;
    *pin = number + 1;
    return true;
}

static esp_err_t ensure_adc(void) {
    if (s_adc_unit != NULL) {
        return ESP_OK;
    }
    adc_oneshot_unit_init_cfg_t unit = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    return adc_oneshot_new_unit(&unit, &s_adc_unit);
}

static esp_err_t ensure_calibration(void) {
    if (s_adc_cali != NULL) {
        return ESP_OK;
    }
    adc_cali_curve_fitting_config_t config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_cali_create_scheme_curve_fitting(&config, &s_adc_cali);
}

static esp_err_t configure_channel(adc_channel_t channel) {
    esp_err_t err = ensure_adc();
    if (err != ESP_OK) {
        return err;
    }
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_oneshot_config_channel(s_adc_unit, channel, &config);
}

static jerry_value_t read_adc(int pin, adc_channel_t channel, bool calibrated) {
    if (!mcujs_pin_can_claim(pin, MCUJS_PIN_OWNER_ADC)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "ADC pin is owned by another peripheral");
    }
    if (!mcujs_pin_claim(pin, MCUJS_PIN_OWNER_ADC)) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "ADC pin claim failed");
    }
    esp_err_t err = configure_channel(channel);
    int result = 0;
    if (err == ESP_OK && calibrated) {
        err = ensure_calibration();
    }
    if (err == ESP_OK) {
        err = calibrated
                  ? adc_oneshot_get_calibrated_result(s_adc_unit, s_adc_cali, channel, &result)
                  : adc_oneshot_read(s_adc_unit, channel, &result);
    }
    if (err != ESP_OK) {
        (void)gpio_reset_pin((gpio_num_t)pin);
        mcujs_pin_release(pin, MCUJS_PIN_OWNER_ADC);
        return jerry_throw_sz(JERRY_ERROR_COMMON,
                              calibrated ? "ADC calibration/read failed" : "ADC read failed");
    }
    return jerry_number(calibrated ? (double)result / 1000.0 : (double)result);
}

static jerry_value_t adc_read_pin_common(const jerry_value_t args[], jerry_length_t argc,
                                         bool calibrated) {
    int pin;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "ADC pin must be a finite number",
                              "ADC pin must be an integer");
    }
    adc_channel_t channel;
    if (!mcujs_pin_is_peripheral(pin) || !adc_pin_to_channel(pin, &channel)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC pin (use GPIO1..GPIO9)");
    }
    return read_adc(pin, channel, calibrated);
}

static jerry_value_t adc_read_channel_common(const jerry_value_t args[], jerry_length_t argc,
                                             bool calibrated) {
    int number;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &number);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "ADC channel must be a finite number",
                              "ADC channel must be an integer");
    }
    adc_channel_t channel;
    int pin;
    if (!adc_number_to_channel(number, &channel, &pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC channel (use 0..8)");
    }
    return read_adc(pin, channel, calibrated);
}

static jerry_value_t adc_read_pin_handler(const jerry_call_info_t *info,
                                          const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    return adc_read_pin_common(args, argc, false);
}

static jerry_value_t adc_read_channel_handler(const jerry_call_info_t *info,
                                              const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    return adc_read_channel_common(args, argc, false);
}

static jerry_value_t adc_read_voltage_pin_handler(const jerry_call_info_t *info,
                                                  const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    return adc_read_pin_common(args, argc, true);
}

static jerry_value_t adc_read_voltage_channel_handler(const jerry_call_info_t *info,
                                                      const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    return adc_read_channel_common(args, argc, true);
}

static esp_err_t ensure_temperature_sensor(void) {
    if (s_temp_sensor != NULL) {
        return ESP_OK;
    }
    temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    return temperature_sensor_install(&config, &s_temp_sensor);
}

static jerry_value_t adc_read_temp_handler(const jerry_call_info_t *info,
                                           const jerry_value_t args[], jerry_length_t argc) {
    (void)info;
    (void)args;
    (void)argc;
    esp_err_t err = ensure_temperature_sensor();
    if (err != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Temperature sensor initialization failed");
    }
    err = temperature_sensor_enable(s_temp_sensor);
    float celsius = 0;
    if (err == ESP_OK) {
        err = temperature_sensor_get_celsius(s_temp_sensor, &celsius);
    }
    esp_err_t disable_err = temperature_sensor_disable(s_temp_sensor);
    if (err != ESP_OK || disable_err != ESP_OK) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Temperature sensor read failed");
    }
    return jerry_number((double)celsius);
}

jerry_value_t js_create_adc_module(void) {
    jerry_value_t adc = jerry_object();
    js_set_function(adc, "readPin", adc_read_pin_handler);
    js_set_function(adc, "readChannel", adc_read_channel_handler);
    js_set_function(adc, "readVoltagePin", adc_read_voltage_pin_handler);
    js_set_function(adc, "readVoltageChannel", adc_read_voltage_channel_handler);
    js_set_function(adc, "readTempC", adc_read_temp_handler);
    return adc;
}

void js_bind_adc(void) {
    jerry_value_t adc = js_create_adc_module();
    js_register_global("adc", adc);
    jerry_value_free(adc);
}
