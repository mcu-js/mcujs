#include "bindings.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MCUJS_STRINGIFY_VALUE(value) #value
#define MCUJS_STRINGIFY(value) MCUJS_STRINGIFY_VALUE(value)

#if defined(MCUJS_PLATFORM_RP2)
#include "pin_policy.h"
#define ADC_PIN 26
#define ADC_LAST_PIN 28
#define ADC_LAST_CHANNEL 2
#elif defined(MCUJS_PLATFORM_ESP32)
#include "esp_err.h"
#include "pin_policy.h"
#define ADC_PIN 1
#define ADC_LAST_PIN 9
#define ADC_LAST_CHANNEL 8
#else
#error "ADC backend test requires an explicit platform"
#endif

static bool eval_source(const char *source) {
    jerry_value_t result = jerry_eval((const jerry_char_t *)source,
                                      strlen(source), JERRY_PARSE_NO_OPTS);
    if (jerry_value_is_exception(result)) {
        jerry_value_t error = jerry_exception_value(result, true);
        jerry_value_t text = jerry_value_to_string(error);
        char buffer[256];
        jerry_size_t size = jerry_string_size(text, JERRY_ENCODING_UTF8);
        if (size >= sizeof(buffer)) size = sizeof(buffer) - 1;
        jerry_string_to_buffer(text, JERRY_ENCODING_UTF8,
                               (jerry_char_t *)buffer, size);
        buffer[size] = '\0';
        fprintf(stderr, "ADC backend test exception: %s\n", buffer);
        jerry_value_free(text);
        jerry_value_free(error);
        return false;
    }
    jerry_value_free(result);
    return true;
}

static void install_module(const char *name, jerry_value_t module) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t result = jerry_object_set_sz(global, name, module);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(global);
    jerry_value_free(module);
}

static bool assert_operational_error(const char *operation, const char *name,
                                     const char *code, const char *resource,
                                     int pin, bool native_code) {
    char source[1400];
    int written;
    if (pin >= 0) {
        written = snprintf(
            source, sizeof(source),
            "(function () { var error; try { %s; } catch (caught) { error = caught; } "
            "if (!(error instanceof Error)) throw new Error('missing %s'); "
            "if (error.name !== '%s') throw new Error('wrong %s name: ' + error.name); "
            "if (error.code !== '%s') throw new Error('wrong %s code: ' + error.code); "
            "if (error.resource !== '%s') throw new Error('wrong %s resource: ' + error.resource); "
            "if (error.pin !== %d) throw new Error('wrong %s pin: ' + error.pin); "
            "if (%s && typeof error.nativeCode !== 'number') throw new Error('%s nativeCode missing'); "
            "if (typeof error.message !== 'string' || error.message.length === 0) throw new Error('%s message missing'); "
            "}());",
            operation, code, name, code, code, code, resource, code, pin, code,
            native_code ? "true" : "false", code, code);
    } else {
        written = snprintf(
            source, sizeof(source),
            "(function () { var error; try { %s; } catch (caught) { error = caught; } "
            "if (!(error instanceof Error)) throw new Error('missing %s'); "
            "if (error.name !== '%s') throw new Error('wrong %s name: ' + error.name); "
            "if (error.code !== '%s') throw new Error('wrong %s code: ' + error.code); "
            "if (error.resource !== '%s') throw new Error('wrong %s resource: ' + error.resource); "
            "if (%s && typeof error.nativeCode !== 'number') throw new Error('%s nativeCode missing'); "
            "if (typeof error.message !== 'string' || error.message.length === 0) throw new Error('%s message missing'); "
            "}());",
            operation, code, name, code, code, code, resource, code,
            native_code ? "true" : "false", code, code);
    }
    return written > 0 && (size_t)written < sizeof(source) && eval_source(source);
}

int main(void) {
    mcujs_test_reset_backend();
    jerry_init(JERRY_INIT_EMPTY);
    install_module("adc", js_create_adc_module());

    assert(eval_source(
        "(function () {\n"
        "  function assert(condition, message) { if (!condition) throw new Error(message); }\n"
        "  var names = ['readPin', 'readChannel', 'readVoltagePin', 'readVoltageChannel', 'readTempC'];\n"
        "  for (var i = 0; i < names.length; i++) assert(typeof adc[names[i]] === 'function', names[i] + ' missing');\n"
#if defined(MCUJS_PLATFORM_RP2)
        "  assert(adc.TEMP === 4, 'RP TEMP compatibility alias missing');\n"
        "  assert(adc.VSYS === 3, 'RP VSYS compatibility alias missing');\n"
#else
        "  assert(!('TEMP' in adc), 'ESP TEMP alias must be absent');\n"
        "  assert(!('VSYS' in adc), 'ESP VSYS alias must be absent');\n"
#endif
        "}());"));

    unsigned raw_calls = mcujs_test_adc_read_calls;
    unsigned calibrated_calls = mcujs_test_adc_calibrated_calls;
    assert(eval_source(
        "(function () {\n"
        "  function expect(call, constructor, message) { var error; try { call(); } catch (caught) { error = caught; } if (!(error instanceof constructor) || ('code' in error)) throw new Error(message + ': ' + error); }\n"
        "  var methods = ['readPin', 'readChannel', 'readVoltagePin', 'readVoltageChannel'];\n"
        "  for (var i = 0; i < methods.length; i++) {\n"
        "    var method = methods[i];\n"
        "    expect(function () { adc[method](); }, TypeError, method + ' accepted missing input');\n"
        "    expect(function () { adc[method]('0'); }, TypeError, method + ' accepted string input');\n"
        "    expect(function () { adc[method](NaN); }, TypeError, method + ' accepted NaN');\n"
        "    expect(function () { adc[method](Infinity); }, TypeError, method + ' accepted infinity');\n"
        "    expect(function () { adc[method](0.5); }, RangeError, method + ' accepted fractional input');\n"
        "  }\n"
#if defined(MCUJS_PLATFORM_RP2)
        "  expect(function () { adc.readPin(25); }, RangeError, 'RP unsafe pin accepted');\n"
        "  expect(function () { adc.readChannel(5); }, RangeError, 'RP invalid channel accepted');\n"
#else
        "  expect(function () { adc.readPin(0); }, RangeError, 'ESP unexposed pin accepted');\n"
        "  expect(function () { adc.readPin(10); }, RangeError, 'ESP internal pin accepted');\n"
        "  expect(function () { adc.readChannel(9); }, RangeError, 'ESP ADC1 channel 9 accepted');\n"
#endif
        "}());"));
    assert(mcujs_test_adc_read_calls == raw_calls);
    assert(mcujs_test_adc_calibrated_calls == calibrated_calls);

#if defined(MCUJS_PLATFORM_ESP32)
    mcujs_test_adc_unit_result = ESP_FAIL;
    assert(assert_operational_error("adc.readPin(1)", "Error", "EIO",
                                    "adc", 1, true));
    mcujs_test_adc_unit_result = ESP_OK;
#endif

    mcujs_test_adc_raw_value = 2048;
    assert(eval_source(
        "(function () {\n"
        "  function assert(condition, message) { if (!condition) throw new Error(message); }\n"
        "  assert(adc.readPin(" MCUJS_STRINGIFY(ADC_PIN) ") === 2048, 'raw pin units changed');\n"
        "  assert(adc.readChannel(0) === 2048, 'raw channel units changed');\n"
        "  assert(adc.readPin(" MCUJS_STRINGIFY(ADC_LAST_PIN) ") === 2048, 'last ADC pin rejected');\n"
        "  assert(adc.readChannel(" MCUJS_STRINGIFY(ADC_LAST_CHANNEL) ") === 2048, 'last ADC channel rejected');\n"
        "}());"));

#if defined(MCUJS_PLATFORM_ESP32)
    mcujs_test_adc_calibration_result = ESP_FAIL;
    assert(assert_operational_error("adc.readVoltagePin(1)", "Error", "EIO",
                                    "adc", 1, true));
    assert(mcujs_pin_claim(1, MCUJS_PIN_OWNER_PWM));
    mcujs_pin_release(1, MCUJS_PIN_OWNER_PWM);
    mcujs_test_adc_calibration_result = ESP_OK;
    mcujs_test_adc_millivolts = 1234;
    assert(eval_source(
        "(function () { var value = adc.readVoltagePin(1); var delta = value - 1.234; "
        "if (delta < 0) delta = -delta; if (delta > 0.000001) throw new Error('ESP voltage is not volts: ' + value); "
        "value = adc.readVoltageChannel(0); delta = value - 1.234; if (delta < 0) delta = -delta; "
        "if (delta > 0.000001) throw new Error('ESP channel voltage is not volts: ' + value); }());"));

    mcujs_test_temperature_install_result = ESP_FAIL;
    assert(assert_operational_error("adc.readTempC()", "Error", "EIO",
                                    "temperatureSensor", -1, true));
    mcujs_test_temperature_install_result = ESP_OK;
    mcujs_test_temperature_celsius = 31.25f;
    assert(eval_source("if (adc.readTempC() !== 31.25) throw new Error('ESP temperature is not Celsius');"));

    mcujs_test_temperature_read_result = ESP_FAIL;
    assert(assert_operational_error("adc.readTempC()", "Error", "EIO",
                                    "temperatureSensor", -1, true));
    mcujs_test_temperature_read_result = ESP_OK;

    /* A failed disable leaves the sensor live. The next call must retry the
     * read/disable sequence without attempting a second enable first. */
    mcujs_test_temperature_disable_result = ESP_FAIL;
    assert(assert_operational_error("adc.readTempC()", "Error", "EIO",
                                    "temperatureSensor", -1, true));
    mcujs_test_temperature_disable_result = ESP_OK;
    mcujs_test_temperature_enable_result = ESP_ERR_INVALID_STATE;
    assert(eval_source("if (adc.readTempC() !== 31.25) throw new Error('ESP temperature cleanup retry failed');"));
    mcujs_test_temperature_enable_result = ESP_OK;
    assert(eval_source("if (adc.readTempC() !== 31.25) throw new Error('ESP temperature did not return to idle');"));

    mcujs_test_adc_config_result = ESP_ERR_INVALID_STATE;
    assert(assert_operational_error("adc.readChannel(0)", "ResourceBusyError",
                                    "EBUSY", "adc", 1, true));
    mcujs_test_adc_config_result = ESP_OK;

    mcujs_test_adc_read_result = ESP_FAIL;
    mcujs_test_gpio_reset_result = ESP_FAIL;
    assert(assert_operational_error("adc.readPin(3)", "Error", "EIO",
                                    "adc", 3, true));
    assert(!mcujs_pin_claim(3, MCUJS_PIN_OWNER_PWM));
    assert(!mcujs_pin_gpio_access_allowed(3));
    mcujs_test_gpio_reset_result = ESP_OK;
    mcujs_test_adc_read_result = ESP_OK;
    assert(eval_source("adc.readPin(3);"));
    assert(mcujs_pin_gpio_access_allowed(3));
    assert(mcujs_pin_claim(3, MCUJS_PIN_OWNER_PWM));

    assert(mcujs_pin_claim(2, MCUJS_PIN_OWNER_PWM));
    assert(assert_operational_error("adc.readPin(2)", "ResourceBusyError",
                                    "EBUSY", "adc", 2, false));
    mcujs_pin_release(2, MCUJS_PIN_OWNER_PWM);
    assert(eval_source("adc.readPin(2);"));
    assert(mcujs_pin_claim(2, MCUJS_PIN_OWNER_PWM));

    assert(mcujs_pin_claim(4, MCUJS_PIN_OWNER_GPIO));
    assert(eval_source("adc.readPin(4);"));
    assert(mcujs_pin_owner(4) == MCUJS_PIN_OWNER_NONE);
    assert(mcujs_pin_claim(4, MCUJS_PIN_OWNER_PWM));
#else
    assert(eval_source(
        "(function () { var value = adc.readVoltagePin(26); var expected = 2048 * 3.3 / 4095; "
        "var delta = value - expected; if (delta < 0) delta = -delta; "
        "if (delta > 0.0000001) throw new Error('RP voltage is not volts: ' + value); "
        "if (adc.readChannel(adc.TEMP) !== 2048) throw new Error('TEMP compatibility channel failed'); "
        "if (adc.readChannel(adc.VSYS) !== 2048) throw new Error('VSYS compatibility channel failed'); "
        "value = adc.readTempC(); expected = 27 - ((2048 * 3.3 / 4095) - 0.706) / 0.001721; "
        "delta = value - expected; if (delta < 0) delta = -delta; "
        "if (delta > 0.0000001) throw new Error('RP temperature is not Celsius: ' + value); }());"));

    assert(mcujs_rp2_pin_claim(27, MCUJS_RP2_PIN_OWNER_PWM));
    assert(assert_operational_error("adc.readPin(27)", "ResourceBusyError",
                                    "EBUSY", "adc", 27, false));
    mcujs_rp2_pin_release(27, MCUJS_RP2_PIN_OWNER_PWM);
    assert(eval_source("adc.readPin(27);"));
    assert(mcujs_rp2_pin_claim(27, MCUJS_RP2_PIN_OWNER_PWM));
#endif

    jerry_cleanup();
#if defined(MCUJS_PLATFORM_ESP32)
    puts("real ESP32 ADC binding contract passed");
#else
    puts("real RP ADC binding contract passed");
#endif
    return 0;
}
