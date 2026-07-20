#include "bindings.h"
#include "runtime_features.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(MCUJS_PLATFORM_RP2)
#include "pin_policy.h"
#elif defined(MCUJS_PLATFORM_ESP32)
#include "esp_err.h"
#include "led_strip.h"
#include "pin_policy.h"
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
        fprintf(stderr, "NeoPixel backend test exception: %s\n", buffer);
        jerry_value_free(text);
        jerry_value_free(error);
        return false;
    }
    jerry_value_free(result);
    return true;
}

static void install_module(void) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t module = js_create_neopixel_module();
    jerry_value_t result = jerry_object_set_sz(global, "neopixel", module);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(module);

    jerry_value_t maximum = jerry_number(MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH);
    result = jerry_object_set_sz(global, "__neopixelMax", maximum);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(maximum);
    jerry_value_free(global);
}

static const char s_strict_source[] =
    "(function () {\n"
    "  function assert(condition, message) { if (!condition) throw new Error(message); }\n"
    "  function capture(call, message) { try { call(); } catch (error) { return error; } throw new Error('expected exception: ' + message); }\n"
    "  function expect(call, constructor, message) { var error = capture(call, message); assert(error instanceof constructor, message + ': ' + error); assert(!('code' in error), message + ' gained operational code'); return error; }\n"
    "  function operational(call, name, code) { var error = capture(call, code); assert(error instanceof Error, code + ' missing Error'); assert(error.name === name, code + ' wrong name: ' + error.name); assert(error.code === code, code + ' wrong code: ' + error.code); assert(error.resource === 'neopixel', code + ' wrong resource'); return error; }\n"
    "  assert(JSON.stringify(Object.keys(neopixel).sort()) === JSON.stringify(['clear','init','setPixel','show']), 'module exports drifted');\n"
    "  expect(function () { neopixel.init(); }, TypeError, 'missing options');\n"
    "  expect(function () { neopixel.init(null); }, TypeError, 'null options');\n"
    "  expect(function () { neopixel.init([]); }, TypeError, 'array options');\n"
    "  expect(function () { neopixel.init(function () {}); }, TypeError, 'function options');\n"
    "  expect(function () { neopixel.init({pin: 3, length: 1, extra: true}); }, RangeError, 'unknown option');\n"
    "  expect(function () { neopixel.init({length: 1}); }, TypeError, 'missing pin');\n"
    "  expect(function () { neopixel.init({pin: 3}); }, TypeError, 'missing length');\n"
    "  expect(function () { neopixel.init({pin: '3', length: 1}); }, TypeError, 'string pin');\n"
    "  expect(function () { neopixel.init({pin: NaN, length: 1}); }, TypeError, 'NaN pin');\n"
    "  expect(function () { neopixel.init({pin: Infinity, length: 1}); }, TypeError, 'infinite pin');\n"
    "  expect(function () { neopixel.init({pin: 3.5, length: 1}); }, RangeError, 'fractional pin');\n"
    "  expect(function () { neopixel.init({pin: 23, length: 1}); }, RangeError, 'unadvertised pin');\n"
    "  expect(function () { neopixel.init({pin: 3, length: '1'}); }, TypeError, 'string length');\n"
    "  expect(function () { neopixel.init({pin: 3, length: NaN}); }, TypeError, 'NaN length');\n"
    "  expect(function () { neopixel.init({pin: 3, length: Infinity}); }, TypeError, 'infinite length');\n"
    "  expect(function () { neopixel.init({pin: 3, length: 1.5}); }, RangeError, 'fractional length');\n"
    "  expect(function () { neopixel.init({pin: 3, length: 0}); }, RangeError, 'zero length');\n"
    "  expect(function () { neopixel.init({pin: 3, length: __neopixelMax + 1}); }, RangeError, 'maximum plus one');\n"
    "  expect(function () { neopixel.init({pin: 3, length: 1, order: 1}); }, TypeError, 'numeric order');\n"
    "  expect(function () { neopixel.init({pin: 3, length: 1, order: 'BRG'}); }, RangeError, 'unknown order');\n"
    "  expect(function () { neopixel.init({pin: 3, length: 1, order: 'rgb'}); }, RangeError, 'lowercase order');\n"
    "  var later = 0; var sentinel = new URIError('pin getter sentinel'); var options = {};\n"
    "  Object.defineProperty(options, 'pin', {enumerable: true, get: function () { throw sentinel; }});\n"
    "  Object.defineProperty(options, 'length', {enumerable: true, get: function () { later++; return 1; }});\n"
    "  Object.defineProperty(options, 'order', {enumerable: true, get: function () { later++; return 'RGB'; }});\n"
    "  assert(capture(function () { neopixel.init(options); }, 'getter sentinel') === sentinel && later === 0, 'getter exception replaced or did not short-circuit');\n"
    "  expect(function () { neopixel.setPixel('0', 1, 2, 3); }, TypeError, 'string index before init');\n"
    "  expect(function () { neopixel.setPixel(NaN, 1, 2, 3); }, TypeError, 'NaN index before init');\n"
    "  expect(function () { neopixel.setPixel(0.5, 1, 2, 3); }, RangeError, 'fractional index before init');\n"
    "  expect(function () { neopixel.setPixel(-1, 1, 2, 3); }, RangeError, 'negative index before init');\n"
    "  expect(function () { neopixel.setPixel(0, '1', 2, 3); }, TypeError, 'string red before init');\n"
    "  expect(function () { neopixel.setPixel(0, 1, 2.5, 3); }, RangeError, 'fractional green before init');\n"
    "  expect(function () { neopixel.setPixel(0, 1, 2, 256); }, RangeError, 'wrapped blue before init');\n"
    "  operational(function () { neopixel.setPixel(0, 1, 2, 3); }, 'ResourceBusyError', 'EBUSY');\n"
    "  operational(function () { neopixel.show(); }, 'ResourceBusyError', 'EBUSY');\n"
    "  operational(function () { neopixel.clear(); }, 'ResourceBusyError', 'EBUSY');\n"
    "}());\n";

static void inject_create_failure(void) {
#if defined(MCUJS_PLATFORM_RP2)
    mcujs_test_pio_can_add_program = false;
#else
    mcujs_test_led_strip_new_result = ESP_ERR_NO_MEM;
#endif
}

static void clear_create_failure(void) {
#if defined(MCUJS_PLATFORM_RP2)
    mcujs_test_pio_can_add_program = true;
#else
    mcujs_test_led_strip_new_result = ESP_OK;
#endif
}

static bool claim_busy_pin(int pin) {
#if defined(MCUJS_PLATFORM_RP2)
    return mcujs_rp2_pin_claim(pin, MCUJS_RP2_PIN_OWNER_PWM);
#else
    return mcujs_pin_claim(pin, MCUJS_PIN_OWNER_PWM);
#endif
}

static void release_busy_pin(int pin) {
#if defined(MCUJS_PLATFORM_RP2)
    mcujs_rp2_pin_release(pin, MCUJS_RP2_PIN_OWNER_PWM);
#else
    mcujs_pin_release(pin, MCUJS_PIN_OWNER_PWM);
#endif
}

static bool pin_is_neopixel(int pin) {
#if defined(MCUJS_PLATFORM_RP2)
    return mcujs_rp2_pin_owner(pin) == MCUJS_RP2_PIN_OWNER_NEOPIXEL;
#else
    return mcujs_pin_owner(pin) == MCUJS_PIN_OWNER_NEOPIXEL;
#endif
}

static bool pin_is_free(int pin) {
#if defined(MCUJS_PLATFORM_RP2)
    return mcujs_rp2_pin_owner(pin) == MCUJS_RP2_PIN_OWNER_NONE;
#else
    return mcujs_pin_owner(pin) == MCUJS_PIN_OWNER_NONE;
#endif
}

int main(void) {
    mcujs_test_reset_backend();
    jerry_init(JERRY_INIT_EMPTY);
    install_module();

    assert(eval_source(s_strict_source));

    inject_create_failure();
    assert(eval_source(
        "(function () { var error; try { neopixel.init({pin: 3, length: 1}); } catch (caught) { error = caught; } "
        "if (!(error instanceof Error) || error.name !== 'ResourceExhaustedError' || "
        "error.code !== 'ERR_RESOURCE_EXHAUSTED' || error.resource !== 'neopixel') "
        "throw new Error('creation exhaustion was not normalized: ' + error); "
        "try { neopixel.setPixel(0, 1, 2, 3); } catch (busy) { "
        "if (busy.code === 'EBUSY') return; throw busy; } "
        "throw new Error('failed init left module initialized'); }());"));
    assert(pin_is_free(3));
    clear_create_failure();

    assert(eval_source(
        "neopixel.init({pin: 3, length: __neopixelMax}); "
        "neopixel.setPixel(__neopixelMax - 1, 1, 2, 3); "
        "(function () { var error; try { neopixel.setPixel(__neopixelMax, 1, 2, 3); } catch (caught) { error = caught; } "
        "if (!(error instanceof RangeError) || ('code' in error)) throw new Error('index maximum was not strict'); }()); "
        "neopixel.show(); "
        "(function () { var error; try { neopixel.init({pin: 3, length: __neopixelMax + 1}); } catch (caught) { error = caught; } "
        "if (!(error instanceof RangeError) || ('code' in error)) throw new Error('length maximum plus one was not strict'); }()); "
        "neopixel.show();"));
    assert(pin_is_neopixel(3));

#if defined(MCUJS_PLATFORM_RP2)
    assert(mcujs_test_neopixel_last_pin == 3u);
    assert(mcujs_test_neopixel_write_calls ==
           2u * MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH);
    assert(mcujs_test_neopixel_last_word == 0x02010300u);
#else
    assert(mcujs_test_led_strip_last_pin == 3);
    assert(mcujs_test_led_strip_last_length == MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH);
    assert(mcujs_test_led_strip_last_order == LED_STRIP_COLOR_COMPONENT_FMT_GRB);
    assert(mcujs_test_led_strip_set_calls == 1u);
    assert(mcujs_test_led_strip_last_index == MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH - 1u);
    assert(mcujs_test_led_strip_last_red == 1u);
    assert(mcujs_test_led_strip_last_green == 2u);
    assert(mcujs_test_led_strip_last_blue == 3u);
    assert(mcujs_test_led_strip_refresh_calls == 2u);
#endif

    assert(claim_busy_pin(5));
    assert(eval_source(
        "(function () { var error; try { neopixel.init({pin: 5, length: 1, order: 'RGB'}); } catch (caught) { error = caught; } "
        "if (!error || error.name !== 'ResourceBusyError' || error.code !== 'EBUSY' || error.pin !== 5) "
        "throw new Error('busy reinit was not normalized'); neopixel.show(); }());"));
    assert(pin_is_neopixel(3));
    release_busy_pin(5);

    assert(eval_source(
        "neopixel.init({pin: 4, length: 1, order: 'RGB'}); "
        "neopixel.setPixel(0, 4, 5, 6); neopixel.clear(); neopixel.show();"));
    assert(pin_is_free(3));
    assert(pin_is_neopixel(4));
#if defined(MCUJS_PLATFORM_ESP32)
    assert(mcujs_test_led_strip_last_order == LED_STRIP_COLOR_COMPONENT_FMT_RGB);
    assert(mcujs_test_led_strip_del_calls == 1u);
    assert(mcujs_test_led_strip_clear_calls >= 2u);

    mcujs_test_led_strip_del_result = ESP_ERR_TIMEOUT;
    assert(eval_source(
        "(function () { var error; try { neopixel.init({pin: 3, length: 1}); } catch (caught) { error = caught; } "
        "if (!error || error.name !== 'ResourceBusyError' || error.code !== 'EBUSY' || typeof error.nativeCode !== 'number') "
        "throw new Error('teardown failure was not quarantined'); neopixel.show(); }());"));
    assert(pin_is_neopixel(4));
    assert(pin_is_free(3));
    mcujs_test_led_strip_del_result = ESP_OK;
    assert(eval_source("neopixel.init({pin: 3, length: 1});"));
    assert(pin_is_free(4));
    assert(pin_is_neopixel(3));

    mcujs_test_led_strip_set_result = ESP_FAIL;
    assert(eval_source(
        "(function () { var error; try { neopixel.setPixel(0, 1, 2, 3); } catch (caught) { error = caught; } "
        "if (!error || error.code !== 'EIO' || typeof error.nativeCode !== 'number') throw new Error('set failure was not EIO'); }());"));
    mcujs_test_led_strip_set_result = ESP_OK;
    mcujs_test_led_strip_refresh_result = ESP_FAIL;
    assert(eval_source(
        "(function () { var error; try { neopixel.show(); } catch (caught) { error = caught; } "
        "if (!error || error.code !== 'EIO' || typeof error.nativeCode !== 'number') throw new Error('show failure was not EIO'); }());"));
    mcujs_test_led_strip_refresh_result = ESP_OK;
    mcujs_test_led_strip_clear_result = ESP_FAIL;
    assert(eval_source(
        "(function () { var error; try { neopixel.clear(); } catch (caught) { error = caught; } "
        "if (!error || error.code !== 'EIO' || typeof error.nativeCode !== 'number') throw new Error('clear failure was not EIO'); }());"));
    mcujs_test_led_strip_clear_result = ESP_OK;
#endif

    jerry_cleanup();
#if defined(MCUJS_PLATFORM_RP2)
    puts("production RP NeoPixel strict options, limits, order, lifecycle, and ownership passed");
#else
    puts("production ESP32 NeoPixel strict options, limits, order, lifecycle, ownership, and errors passed");
#endif
    return 0;
}
