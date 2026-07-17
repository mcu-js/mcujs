/*
 * mcujs - ADC Bindings
 *
 * Implements: adc.readPin(), adc.readChannel(), adc.readVoltagePin(),
 *             adc.readVoltageChannel(), adc.readTempC()
 * Constants: adc.TEMP, adc.VSYS
 */

#include "bindings.h"
#include "jerryscript.h"
#include "board_config.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name,
                            jerry_external_handler_t handler);
extern void js_set_number(jerry_value_t object, const char *name, double value);
extern void js_register_global(const char *name, jerry_value_t object);
extern double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, double default_value);

#define ADC_TEMP_CHANNEL 4
#define ADC_MAX_VALUE 4095.0
#define ADC_DEFAULT_VREF 3.3

static bool s_adc_initialized = false;
static bool s_temp_enabled = false;

static void adc_init_once(void) {
    if (!s_adc_initialized) {
        adc_init();
        s_adc_initialized = true;
    }
}

static void adc_enable_temp_sensor(void) {
    if (!s_temp_enabled) {
        adc_set_temp_sensor_enabled(true);
        s_temp_enabled = true;
    }
}

static bool adc_pin_to_channel(uint pin, uint *channel) {
    if (pin == MCUJS_ADC0_PIN) {
        *channel = 0;
        return true;
    }
    if (pin == MCUJS_ADC1_PIN) {
        *channel = 1;
        return true;
    }
    if (pin == MCUJS_ADC2_PIN) {
        *channel = 2;
        return true;
    }
    if (pin == MCUJS_ADC_VSYS_PIN) {
        *channel = 3;
        return true;
    }
    return false;
}

static uint16_t adc_read_channel(uint channel) {
    adc_select_input(channel);
    return adc_read();
}

static jerry_value_t adc_read_pin_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;

    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "adc.readPin requires a pin");
    }

    uint pin = (uint)js_get_number_arg(args, argc, 0, 0);
    uint channel = 0;

    if (!adc_pin_to_channel(pin, &channel)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC pin");
    }

    adc_init_once();
    adc_gpio_init(pin);

    return jerry_number((double)adc_read_channel(channel));
}

static jerry_value_t adc_read_channel_handler(const jerry_call_info_t *call_info_p,
                                              const jerry_value_t args[],
                                              const jerry_length_t argc) {
    (void)call_info_p;

    if (argc < 1) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "adc.readChannel requires a channel");
    }

    uint channel = (uint)js_get_number_arg(args, argc, 0, 0);

    if (channel > ADC_TEMP_CHANNEL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC channel");
    }

    adc_init_once();

    if (channel == ADC_TEMP_CHANNEL) {
        adc_enable_temp_sensor();
    }

    return jerry_number((double)adc_read_channel(channel));
}

static double adc_raw_to_voltage(uint16_t raw) {
    return ((double)raw * ADC_DEFAULT_VREF) / ADC_MAX_VALUE;
}

static jerry_value_t adc_read_voltage_pin_handler(const jerry_call_info_t *call_info_p,
                                                  const jerry_value_t args[],
                                                  const jerry_length_t argc) {
    (void)call_info_p;

    jerry_value_t raw = adc_read_pin_handler(call_info_p, args, argc);
    if (jerry_value_is_exception(raw)) {
        return raw;
    }

    double raw_value = jerry_value_as_number(raw);
    jerry_value_free(raw);

    return jerry_number(adc_raw_to_voltage((uint16_t)raw_value));
}

static jerry_value_t adc_read_voltage_channel_handler(const jerry_call_info_t *call_info_p,
                                                      const jerry_value_t args[],
                                                      const jerry_length_t argc) {
    (void)call_info_p;

    jerry_value_t raw = adc_read_channel_handler(call_info_p, args, argc);
    if (jerry_value_is_exception(raw)) {
        return raw;
    }

    double raw_value = jerry_value_as_number(raw);
    jerry_value_free(raw);

    return jerry_number(adc_raw_to_voltage((uint16_t)raw_value));
}

static jerry_value_t adc_read_temp_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    (void)args;
    (void)argc;

    adc_init_once();
    adc_enable_temp_sensor();

    uint16_t raw = adc_read_channel(ADC_TEMP_CHANNEL);
    double voltage = adc_raw_to_voltage(raw);
    double temp_c = 27.0 - (voltage - 0.706) / 0.001721;

    return jerry_number(temp_c);
}

jerry_value_t js_create_adc_module(void) {
    jerry_value_t adc = jerry_object();

    js_set_function(adc, "readPin", adc_read_pin_handler);
    js_set_function(adc, "readChannel", adc_read_channel_handler);
    js_set_function(adc, "readVoltagePin", adc_read_voltage_pin_handler);
    js_set_function(adc, "readVoltageChannel", adc_read_voltage_channel_handler);
    js_set_function(adc, "readTempC", adc_read_temp_handler);

    js_set_number(adc, "TEMP", ADC_TEMP_CHANNEL);
    js_set_number(adc, "VSYS", 3);

    return adc;
}

void js_bind_adc(void) {
    jerry_value_t adc = js_create_adc_module();
    js_register_global("adc", adc);
    jerry_value_free(adc);
}
