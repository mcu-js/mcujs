#include "bindings.h"
#include "runtime_validation_backend_stubs.h"
#include "runtime_features.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(MCUJS_PLATFORM_RP2)
#include "hardware/pwm.h"
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
        fprintf(stderr, "backend validation test exception: %s\n", buffer);
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

static void install_number(const char *name, double number) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t value = jerry_number(number);
    jerry_value_t result = jerry_object_set_sz(global, name, value);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(value);
    jerry_value_free(global);
}

static const char s_strict_source[] =
    "(function () {\n"
    "  function assert(condition, message) { if (!condition) throw new Error(message); }\n"
    "  function capture(call) { try { call(); } catch (error) { return error; } throw new Error('expected exception'); }\n"
    "  function assertType(call, constructor, message) { var error = capture(call); assert(error instanceof constructor, message + ': ' + error); }\n"
    "  function assertOperational(call, name, code, resource) {\n"
    "    var error = capture(call);\n"
    "    assert(error instanceof Error, code + ' was not an Error');\n"
    "    assert(error.name === name, code + ' name mismatch: ' + error.name);\n"
    "    assert(error.code === code, code + ' code mismatch: ' + error.code);\n"
    "    assert(error.resource === resource, code + ' resource mismatch: ' + error.resource);\n"
    "    assert(typeof error.message === 'string' && error.message.length > 0, code + ' message missing');\n"
    "    return error;\n"
    "  }\n"
    "  assertType(function () { GPIO.set(3, 1); }, TypeError, 'GPIO state masked invalid boolean');\n"
    "  var gpioBusy = assertOperational(function () { GPIO.set(3, true); }, 'ResourceBusyError', 'EBUSY', 'gpio');\n"
    "  assert(gpioBusy.pin === 3, 'GPIO busy pin missing');\n"
    "  assert(!('code' in capture(function () { GPIO.set(3, 1); })), 'TypeError gained operational code');\n"
    "  assertType(function () { I2C.write(0, 0x50, ['1']); }, TypeError, 'I2C state masked invalid byte');\n"
    "  assertType(function () { I2C.read(0, 0x50, 257); }, RangeError, 'I2C state masked invalid length');\n"
    "  var sentinel = new URIError('I2C accessor sentinel');\n"
    "  var throwing = [0]; Object.defineProperty(throwing, '0', {get: function () { throw sentinel; }});\n"
    "  assert(capture(function () { I2C.write(0, 0x50, throwing); }) === sentinel, 'I2C accessor exception replaced');\n"
    "  var i2cBusy = assertOperational(function () { I2C.write(0, 0x50, [1]); }, 'ResourceBusyError', 'EBUSY', 'i2c');\n"
    "  assert(i2cBusy.bus === 0, 'I2C busy bus missing');\n"
    "  assertType(function () { PWM.setDuty(9, '0.5'); }, TypeError, 'PWM state masked invalid duty type');\n"
    "  assertType(function () { PWM.setDuty(9, 2); }, RangeError, 'PWM state masked invalid duty range');\n"
    "  assertOperational(function () { PWM.setDuty(9, 0.5); }, 'ResourceBusyError', 'EBUSY', 'pwm');\n"
    "  assertType(function () { GPIO.init('3', GPIO.OUTPUT); }, TypeError, 'GPIO numeric string accepted');\n"
    "  assertType(function () { GPIO.init(3.5, GPIO.OUTPUT); }, RangeError, 'GPIO fractional pin accepted');\n"
    "  GPIO.init(3, GPIO.OUTPUT);\n"
    "  assertType(function () { GPIO.set(3, 1); }, TypeError, 'GPIO truthy value accepted');\n"
    "  assertType(function () { I2C.init(0.5, 1, 2, 100000); }, RangeError, 'I2C fractional bus accepted');\n"
    "  assertType(function () { I2C.init(0, 1, 2, 100000.5); }, RangeError, 'I2C fractional baudrate accepted');\n"
    "  I2C.init(0, __i2cSda, __i2cScl, 100000);\n"
    "  assertType(function () { I2C.write(0, 0x50, ['1']); }, TypeError, 'I2C numeric string byte accepted');\n"
    "  assertType(function () { I2C.write(0, 0x50, [1.5]); }, RangeError, 'I2C fractional byte accepted');\n"
    "  assertType(function () { I2C.write(0, 0x50, [256]); }, RangeError, 'I2C wrapped byte accepted');\n"
    "  var oversized = []; for (var i = 0; i < 257; i++) oversized.push(0);\n"
    "  assertType(function () { I2C.write(0, 0x50, oversized); }, RangeError, 'I2C oversized transfer accepted');\n"
    "  var maximum = []; for (var j = 0; j < 256; j++) maximum.push(j & 255);\n"
    "  assert(I2C.write(0, 0x50, maximum) === 256, 'I2C exact maximum rejected');\n"
    "  assertType(function () { PWM.init('9', 1000); }, TypeError, 'PWM numeric string pin accepted');\n"
    "  assertType(function () { PWM.init(9, 1000.5); }, RangeError, 'PWM fractional frequency accepted');\n"
    "  PWM.init(9, 1000);\n"
    "  assertType(function () { PWM.setDuty(9, 2); }, RangeError, 'legacy raw PWM duty accepted');\n"
    "  PWM.setDuty(9, 0.5);\n"
    "}());\n";

static bool assert_operational_error(const char *operation, const char *name,
                                     const char *code) {
    char source[1024];
    int written = snprintf(
        source, sizeof(source),
        "(function () { var error; try { %s; } catch (caught) { error = caught; } "
        "if (!(error instanceof Error)) throw new Error('missing %s'); "
        "if (error.name !== '%s') throw new Error('wrong %s name: ' + error.name); "
        "if (error.code !== '%s') throw new Error('wrong %s code: ' + error.code); "
        "if (typeof error.message !== 'string' || error.message.length === 0) throw new Error('%s message missing'); "
        "if (typeof error.resource !== 'string' || error.resource.length === 0) throw new Error('%s resource missing'); "
        "if ((error.code === 'EIO' || error.code === 'ENXIO') && typeof error.nativeCode !== 'number') throw new Error('%s nativeCode missing'); "
        "if (error.code === 'ERR_RESOURCE_EXHAUSTED' && typeof error.limit !== 'number') throw new Error('%s limit missing'); "
        "}());",
        operation, code, name, code, code, code, code, code, code, code);
    return written > 0 && (size_t)written < sizeof(source) && eval_source(source);
}

static bool assert_uncoded_range_error(const char *operation) {
    char source[512];
    int written = snprintf(
        source, sizeof(source),
        "(function () { var error; try { %s; } catch (caught) { error = caught; } "
        "if (!(error instanceof RangeError)) throw new Error('missing RangeError'); "
        "if ('code' in error) throw new Error('RangeError gained operational code: ' + error.code); "
        "}());",
        operation);
    return written > 0 && (size_t)written < sizeof(source) && eval_source(source);
}

int main(void) {
    mcujs_test_reset_backend();
    jerry_init(JERRY_INIT_EMPTY);
    install_module("GPIO", js_create_gpio_module());
    install_module("I2C", js_create_i2c_module());
    install_module("PWM", js_create_pwm_module());
    install_number("__pwmMin", MCUJS_RUNTIME_PWM_MIN_HZ);
    install_number("__pwmMax", MCUJS_RUNTIME_PWM_MAX_HZ);
#if defined(MCUJS_PLATFORM_ESP32)
    install_number("__i2cSda", 5);
    install_number("__i2cScl", 6);
    install_number("__pwmProbePin", 4);
#else
    install_number("__i2cSda", 4);
    install_number("__i2cScl", 5);
    install_number("__pwmProbePin", 10);
#endif

    assert(eval_source(s_strict_source));
    assert(mcujs_test_i2c_write_calls == 1);
    assert(mcujs_test_i2c_last_length == 256);

#if defined(MCUJS_PLATFORM_RP2)
    assert(mcujs_test_pwm_level == 31250u);
    assert(eval_source("PWM.setDuty(9, 0);"));
    assert(mcujs_test_pwm_level == 0u);
    assert(eval_source("PWM.setDuty(9, 0.5);"));
    assert(mcujs_test_pwm_level == 31250u);
    assert(eval_source("PWM.setDuty(9, 1);"));
    assert(mcujs_test_pwm_level == 62500u);
    unsigned pwm_duty_calls = mcujs_test_pwm_duty_calls;
#else
    assert(mcujs_test_ledc_duty == 512u);
    assert(eval_source("PWM.setDuty(9, 0);"));
    assert(mcujs_test_ledc_duty == 0u);
    assert(eval_source("PWM.setDuty(9, 0.5);"));
    assert(mcujs_test_ledc_duty == 512u);
    assert(eval_source("PWM.setDuty(9, 1);"));
    assert(mcujs_test_ledc_duty == 1024u);
    unsigned pwm_duty_calls = mcujs_test_ledc_duty_calls;
#endif
    assert(assert_operational_error("PWM.setDuty(9, 1 / 3)",
                                    "NotSupportedError",
                                    "ERR_NOT_SUPPORTED"));
#if defined(MCUJS_PLATFORM_RP2)
    assert(mcujs_test_pwm_duty_calls == pwm_duty_calls);
#else
    assert(mcujs_test_ledc_duty_calls == pwm_duty_calls);
#endif

    assert(eval_source("PWM.init(__pwmProbePin, __pwmMin);"));
#if defined(MCUJS_PLATFORM_RP2)
    assert((uint64_t)125000000u * 16u ==
           (uint64_t)MCUJS_RUNTIME_PWM_MIN_HZ *
               mcujs_test_pwm_divider_scaled * (mcujs_test_pwm_wrap + 1u));
#endif
    assert(eval_source("PWM.stop(__pwmProbePin); PWM.init(__pwmProbePin, __pwmMax);"));
#if defined(MCUJS_PLATFORM_RP2)
    assert((uint64_t)125000000u * 16u ==
           (uint64_t)MCUJS_RUNTIME_PWM_MAX_HZ *
               mcujs_test_pwm_divider_scaled * (mcujs_test_pwm_wrap + 1u));
#endif
    assert(eval_source(
        "(function () { try { PWM.stop(__pwmProbePin); "
        "PWM.init(__pwmProbePin, __pwmMax + 1); } catch (error) { "
        "if (error instanceof RangeError) return; throw error; } "
        "throw new Error('expected PWM maximum-plus-one RangeError'); }());"));

#if defined(MCUJS_PLATFORM_ESP32)
    mcujs_test_ledc_resolution = 14;
    assert(eval_source("PWM.init(4, 1300); PWM.setDuty(4, 1);"));
    assert(mcujs_test_ledc_configured_resolution == 13u);
    assert(mcujs_test_ledc_duty == 8192u);
    assert(eval_source("PWM.stop(4);"));
    mcujs_test_ledc_resolution = 10;

    mcujs_test_ledc_resolution = 14;
    assert(eval_source("PWM.init(4, 1300); PWM.setDuty(4, 0.25);"));
    unsigned pwm_stop_calls = mcujs_test_ledc_stop_calls;
    unsigned gpio_reset_calls = mcujs_test_gpio_reset_calls;
    unsigned preserved_duty_calls = mcujs_test_ledc_duty_calls;
    mcujs_test_ledc_actual_frequency = 1599;
    assert(assert_operational_error("PWM.init(4, 1600)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    mcujs_test_ledc_actual_frequency = 0;
    assert(mcujs_test_ledc_stop_calls == pwm_stop_calls);
    assert(mcujs_test_gpio_reset_calls == gpio_reset_calls);
    assert(eval_source("PWM.setDuty(4, 0.5);"));
    assert(mcujs_test_ledc_duty_calls == preserved_duty_calls + 1u);
    assert(mcujs_test_ledc_duty == 4096u);
    assert(assert_operational_error("GPIO.init(4, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("PWM.stop(4);"));
    mcujs_test_ledc_resolution = 10;

    assert(eval_source(
        "(function () { function expectRange(call, message) { var error; "
        "try { call(); } catch (caught) { error = caught; } "
        "if (!(error instanceof RangeError) || ('code' in error)) "
        "throw new Error(message + ': ' + error); } "
        "var pins = [-1, 10, 21]; for (var i = 0; i < pins.length; i++) { "
        "var pin = pins[i]; expectRange(function () { PWM.setDuty(pin, 0.5); }, "
        "'invalid PWM setDuty pin was not RangeError'); "
        "expectRange(function () { PWM.stop(pin); }, "
        "'invalid PWM stop pin was not RangeError'); } }());"));

    unsigned i2c_delete_calls = mcujs_test_i2c_driver_delete_calls;
    unsigned i2c_install_calls = mcujs_test_i2c_driver_install_calls;
    unsigned i2c_param_calls = mcujs_test_i2c_param_config_calls;
    assert(assert_uncoded_range_error("I2C.init(0, 1, 2, 100000)"));
    assert(assert_operational_error("I2C.init(0, 5, 6, 1)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    assert(assert_operational_error("I2C.init(0, 5, 6, 110000)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    assert(mcujs_test_i2c_driver_delete_calls == i2c_delete_calls);
    assert(mcujs_test_i2c_driver_install_calls == i2c_install_calls);
    assert(mcujs_test_i2c_param_config_calls == i2c_param_calls);
    assert(eval_source("I2C.write(0, 0x50, [1]);"));
    assert(eval_source("I2C.init(0, 5, 6, 1000000);"));
    assert(eval_source(
        "(function () { var error; try { I2C.init(0, 5, 6, 1000001); } "
        "catch (caught) { error = caught; } "
        "if (!(error instanceof RangeError) || ('code' in error)) "
        "throw new Error('I2C maximum-plus-one was not uncoded RangeError'); "
        "I2C.write(0, 0x50, [1]); }());"));

    mcujs_test_ledc_actual_frequency = 1099;
    assert(assert_operational_error("PWM.init(4, 1100)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    mcujs_test_ledc_actual_frequency = 0;
    assert(assert_operational_error("GPIO.init(5, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source(
        "(function () { function expect(call, constructor) { try { call(); } "
        "catch (error) { if (error instanceof constructor) return; throw error; } "
        "throw new Error('expected PWM exception'); } "
        "expect(function () { PWM.init('4', 1000); }, TypeError); "
        "expect(function () { PWM.init(4, 1000.5); }, RangeError); }());"));
    mcujs_test_ledc_resolution = 0;
    assert(assert_operational_error("PWM.init(4, 1100)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    mcujs_test_ledc_resolution = 10;
    assert(eval_source("PWM.init(4, 1100);"));
    mcujs_test_ledc_resolution = 0;
    assert(assert_operational_error("PWM.init(4, 1200)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    mcujs_test_ledc_resolution = 10;
    assert(eval_source("PWM.setDuty(4, 0.5);"));
    assert(eval_source(
        "(function () { try { PWM.setDuty(4, 2); } catch (error) { "
        "if (error instanceof RangeError) return; throw error; } "
        "throw new Error('legacy raw PWM duty accepted'); }());"));
    mcujs_test_ledc_result = 0x103;
    assert(assert_operational_error("PWM.setDuty(4, 0.5)",
                                    "ResourceBusyError", "EBUSY"));
    mcujs_test_ledc_result = 0;
    assert(eval_source("PWM.init(7, 2000); PWM.init(8, 3000);"));
    assert(assert_operational_error("PWM.init(2, 4000)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED"));
    mcujs_test_i2c_result = -1;
    const char *backend_name = "esp32";
#else
    unsigned gpio_init_calls = mcujs_test_gpio_init_calls;
    unsigned pwm_config_calls = mcujs_test_pwm_config_calls;
    assert(eval_source(
        "(function () { function expectRange(call) { try { call(); } "
        "catch (error) { if (error instanceof RangeError) return; throw error; } "
        "throw new Error('expected safe-pin RangeError'); } "
        "expectRange(function () { GPIO.init(23, GPIO.OUTPUT); }); "
        "expectRange(function () { GPIO.init(24, GPIO.INPUT); }); "
        "expectRange(function () { GPIO.init(29, GPIO.OUTPUT); }); "
        "expectRange(function () { PWM.init(23, 1000); }); "
        "expectRange(function () { PWM.init(29, 1000); }); }());"));
    assert(mcujs_test_gpio_init_calls == gpio_init_calls);
    assert(mcujs_test_pwm_config_calls == pwm_config_calls);

    gpio_init_calls = mcujs_test_gpio_init_calls;
    assert(eval_source(
        "(function () { var error; try { GPIO.init(3, 99); } catch (caught) { error = caught; } "
        "if (!(error instanceof RangeError)) throw new Error('invalid GPIO mode accepted'); "
        "GPIO.set(3, true); if (GPIO.get(3) !== true) throw new Error('GPIO state was destroyed'); }());"));
    assert(mcujs_test_gpio_init_calls == gpio_init_calls);

    unsigned i2c_init_calls = mcujs_test_i2c_init_calls;
    assert(assert_uncoded_range_error("I2C.init(0, 10, 11, 100000)"));
    assert(assert_operational_error("I2C.init(0, 4, 5, 1)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    assert(assert_operational_error("I2C.init(0, 4, 5, 110000)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    assert(mcujs_test_i2c_init_calls == i2c_init_calls);
    assert(eval_source("I2C.write(0, 0x50, [1]);"));

    pwm_config_calls = mcujs_test_pwm_config_calls;
    assert(assert_operational_error("PWM.init(10, 1100)",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
    assert(mcujs_test_pwm_config_calls == pwm_config_calls);

    assert(assert_operational_error("PWM.init(8, 1250)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("PWM.init(8, 1000); PWM.setDuty(8, 0.25);"));
    assert(assert_operational_error("PWM.init(8, 1250)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("PWM.setDuty(8, 0.5); PWM.setDuty(9, 0.5);"));
    assert(eval_source("PWM.stop(8); PWM.stop(9); PWM.init(12, 1000);"));
    assert(assert_operational_error("GPIO.init(12, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("PWM.setDuty(12, 0.5); PWM.stop(12); "
                       "GPIO.init(12, GPIO.OUTPUT); GPIO.set(12, true);"));

    assert(pwm_gpio_to_slice_num(0) == pwm_gpio_to_slice_num(16));
    assert(pwm_gpio_to_channel(0) == pwm_gpio_to_channel(16));
    assert(pwm_gpio_to_slice_num(1) == pwm_gpio_to_slice_num(17));
    assert(pwm_gpio_to_channel(1) == pwm_gpio_to_channel(17));
    assert(pwm_gpio_to_slice_num(32) == pwm_gpio_to_slice_num(40));
    assert(pwm_gpio_to_channel(32) == pwm_gpio_to_channel(40));
    assert(pwm_gpio_to_slice_num(33) == pwm_gpio_to_slice_num(41));
    assert(pwm_gpio_to_channel(33) == pwm_gpio_to_channel(41));
    pwm_config_calls = mcujs_test_pwm_config_calls;
    assert(eval_source("PWM.init(0, 1000);"));
    unsigned claimed_output_config_calls = mcujs_test_pwm_config_calls;
    assert(claimed_output_config_calls > pwm_config_calls);
    assert(assert_operational_error("PWM.init(16, 1000)",
                                    "ResourceBusyError", "EBUSY"));
    assert(mcujs_test_pwm_config_calls == claimed_output_config_calls);
    assert(eval_source("PWM.setDuty(0, 0.5); PWM.stop(0); "
                       "PWM.init(16, 1000); PWM.setDuty(16, 0.5); "
                       "PWM.stop(16);"));
    assert(eval_source("PWM.init(1, 1000);"));
    claimed_output_config_calls = mcujs_test_pwm_config_calls;
    assert(assert_operational_error("PWM.init(17, 1000)",
                                    "ResourceBusyError", "EBUSY"));
    assert(mcujs_test_pwm_config_calls == claimed_output_config_calls);
    assert(eval_source("PWM.stop(1); PWM.init(17, 1000); PWM.stop(17);"));

    assert(assert_operational_error("PWM.init(3, 1000)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("GPIO.set(3, true);"));
    assert(assert_operational_error("PWM.init(4, 1000)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("GPIO.init(7, GPIO.OUTPUT);"));
    assert(assert_operational_error("I2C.init(1, 6, 7, 100000)",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("GPIO.init(6, GPIO.OUTPUT);"));
    assert(eval_source("I2C.init(0, 8, 9, 100000); "
                       "GPIO.init(4, GPIO.OUTPUT);"));
    assert(assert_operational_error("GPIO.init(8, GPIO.OUTPUT)",
                                    "ResourceBusyError", "EBUSY"));
    mcujs_test_i2c_result = 1;
    assert(assert_operational_error("I2C.write(0, 0x50, [1, 2])",
                                    "Error", "EIO"));
    mcujs_test_i2c_result = -1;
    const char *backend_name = "rp2";
#endif
    assert(assert_operational_error("I2C.write(0, 0x50, [1])", "Error", "ENXIO"));

#if defined(MCUJS_PLATFORM_ESP32)
    mcujs_test_i2c_result = 0x103;
    assert(assert_operational_error("I2C.read(0, 0x50, 1)",
                                    "ResourceBusyError", "EBUSY"));
    mcujs_test_i2c_result = 0x555;
#else
    mcujs_test_i2c_result = -2;
#endif
    assert(assert_operational_error("I2C.read(0, 0x50, 1)", "Error", "EIO"));

#if defined(MCUJS_PLATFORM_ESP32)
    mcujs_test_i2c_result = 0x101;
    assert(assert_operational_error("I2C.read(0, 0x50, 1)",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED"));
    mcujs_test_i2c_result = 0x106;
    assert(assert_operational_error("I2C.read(0, 0x50, 1)",
                                    "NotSupportedError",
                                    "ERR_NOT_SUPPORTED"));
#endif

    jerry_cleanup();
    printf("real %s GPIO/I2C/PWM validation/error contract passed\n",
           backend_name);
    return 0;
}
