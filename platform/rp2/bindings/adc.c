/*
 * mcujs - ADC Bindings
 *
 * Implements: adc.readPin(), adc.readChannel(), adc.readVoltagePin(),
 *             adc.readVoltageChannel(), adc.readTempC()
 * Constants: adc.TEMP, adc.VSYS
 */

#include "bindings.h"
#include "board_config.h"
#include "jerryscript.h"
#include "pin_policy.h"
#include "validation.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name,
                            jerry_external_handler_t handler);
extern void js_set_number(jerry_value_t object, const char *name, double value);
extern void js_register_global(const char *name, jerry_value_t object);

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

static jerry_value_t throw_adc_busy(int pin, const char *message) {
    const mcujs_error_details_t details = {
        .resource = "adc",
        .has_pin = true,
        .pin = pin,
    };
    return mcujs_throw_operational_error(MCUJS_ERROR_BUSY, message, &details);
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

static int adc_channel_to_pin(uint channel) {
    switch (channel) {
        case 0: return MCUJS_ADC0_PIN;
        case 1: return MCUJS_ADC1_PIN;
        case 2: return MCUJS_ADC2_PIN;
        case 3: return MCUJS_ADC_VSYS_PIN;
        default: return -1;
    }
}

static jerry_value_t prepare_adc_pin(int pin) {
    if (!mcujs_rp2_pin_can_claim(pin, MCUJS_RP2_PIN_OWNER_ADC)) {
        return throw_adc_busy(pin, "ADC pin is owned by another peripheral");
    }
    if (!mcujs_rp2_pin_claim(pin, MCUJS_RP2_PIN_OWNER_ADC)) {
        return throw_adc_busy(pin, "ADC pin claim failed");
    }
    adc_init_once();
    adc_gpio_init((uint)pin);
    return jerry_undefined();
}

static uint16_t adc_read_channel(uint channel) {
    adc_select_input(channel);
    return adc_read();
}

static jerry_value_t adc_read_pin_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;

    int pin;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &pin);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "ADC pin must be a finite number",
                              "ADC pin must be an integer");
    }

    uint channel = 0;
    if (!mcujs_rp2_adc_pin_allowed(pin) ||
        !adc_pin_to_channel((uint)pin, &channel)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC pin");
    }

    jerry_value_t prepared = prepare_adc_pin(pin);
    if (jerry_value_is_exception(prepared)) return prepared;
    jerry_value_free(prepared);
    return jerry_number((double)adc_read_channel(channel));
}

static jerry_value_t adc_read_channel_handler(const jerry_call_info_t *call_info_p,
                                              const jerry_value_t args[],
                                              const jerry_length_t argc) {
    (void)call_info_p;

    int channel;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &channel);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "ADC channel must be a finite number",
                              "ADC channel must be an integer");
    }
    if (channel < 0 || channel > ADC_TEMP_CHANNEL) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC channel");
    }

    if (channel == ADC_TEMP_CHANNEL) {
#if MCUJS_REGISTRY_ADC_TEMP_RAW_CHANNEL
        adc_init_once();
        adc_enable_temp_sensor();
#else
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC channel");
#endif
    } else {
        if (!mcujs_rp2_adc_channel_allowed(channel)) {
            return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC channel");
        }
        int pin = adc_channel_to_pin((uint)channel);
#if MCUJS_REGISTRY_ADC_VSYS
        if (channel == 3) {
            adc_init_once();
            return jerry_number((double)adc_read_channel((uint)channel));
        }
#endif
        if (pin < 0 || !mcujs_rp2_adc_pin_allowed(pin)) {
            return jerry_throw_sz(JERRY_ERROR_RANGE, "Invalid ADC channel");
        }
        jerry_value_t prepared = prepare_adc_pin(pin);
        if (jerry_value_is_exception(prepared)) return prepared;
        jerry_value_free(prepared);
    }
    return jerry_number((double)adc_read_channel((uint)channel));
}

static double adc_raw_to_voltage(uint16_t raw) {
    return ((double)raw * ADC_DEFAULT_VREF) / ADC_MAX_VALUE;
}

static jerry_value_t adc_read_voltage_pin_handler(
    const jerry_call_info_t *call_info_p, const jerry_value_t args[],
    const jerry_length_t argc) {
    jerry_value_t raw = adc_read_pin_handler(call_info_p, args, argc);
    if (jerry_value_is_exception(raw)) return raw;
    double raw_value = jerry_value_as_number(raw);
    jerry_value_free(raw);
    return jerry_number(adc_raw_to_voltage((uint16_t)raw_value));
}

static jerry_value_t adc_read_voltage_channel_handler(
    const jerry_call_info_t *call_info_p, const jerry_value_t args[],
    const jerry_length_t argc) {
    jerry_value_t raw = adc_read_channel_handler(call_info_p, args, argc);
    if (jerry_value_is_exception(raw)) return raw;
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
    return jerry_number(27.0 - (voltage - 0.706) / 0.001721);
}

jerry_value_t js_create_adc_module(void) {
    jerry_value_t adc = jerry_object();
    js_set_function(adc, "readPin", adc_read_pin_handler);
    js_set_function(adc, "readChannel", adc_read_channel_handler);
    js_set_function(adc, "readVoltagePin", adc_read_voltage_pin_handler);
    js_set_function(adc, "readVoltageChannel", adc_read_voltage_channel_handler);
    js_set_function(adc, "readTempC", adc_read_temp_handler);
    js_set_number(adc, "TEMP", ADC_TEMP_CHANNEL);
#if MCUJS_REGISTRY_ADC_VSYS
    js_set_number(adc, "VSYS", 3);
#endif
    return adc;
}

void js_bind_adc(void) {
    jerry_value_t adc = js_create_adc_module();
    js_register_global("adc", adc);
    jerry_value_free(adc);
}
