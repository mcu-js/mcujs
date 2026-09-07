#include "bindings.h"
#include "board_config.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern jerry_value_t mcujs_rp2_create_board_module(void);

/* Native binding seam only: physical QSPI sampling is verified on hardware. */
#if MCUJS_TEST_EXPECT_BUTTON
static bool button_pressed;
static unsigned button_samples;
bool boot_button_pressed(void) {
    button_samples++;
    return button_pressed;
}
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
        fprintf(stderr, "RP board surface exception: %s\n", buffer);
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

int main(void) {
    mcujs_test_reset_backend();
    jerry_init(JERRY_INIT_EMPTY);
    install_module("board", mcujs_rp2_create_board_module());
    install_module("GPIO", js_create_gpio_module());
    install_module("adc", js_create_adc_module());
#if MCUJS_TEST_EXPECT_BUTTON
    assert(eval_source("if (board.buttonPressed() !== false) throw new Error('released button');"));
    button_pressed = true;
    assert(eval_source("if (board.buttonPressed() !== true) throw new Error('pressed button');"));
    button_pressed = false;
    assert(eval_source("if (board.buttonPressed() !== false) throw new Error('button release');"));
    assert(button_samples == 3);
    assert(eval_source(
        "[true, false, 0, 1, null, undefined, {}, 'pressed'].forEach(function (value) {"
        " var error; try { board.buttonPressed(value); } catch (caught) { error = caught; }"
        " if (!(error instanceof TypeError)) throw new Error('button accepted argument');"
        "});"));
    assert(button_samples == 3); /* Reject writes before touching the sampler. */
#else
    assert(eval_source("if ('buttonPressed' in board) throw new Error('button method must be absent');"));
#endif
#if MCUJS_TEST_EXPECT_NEOPIXEL
    install_module("neopixel", js_create_neopixel_module());
#endif

#if MCUJS_TEST_EXPECT_LED_PIN
    assert(eval_source("if (board.ledPin !== " MCUJS_TEST_LED_PIN_STRING ") throw new Error('ledPin missing');"));
#else
    assert(eval_source("if ('ledPin' in board) throw new Error('ledPin must be absent');"));
#endif
#if MCUJS_TEST_EXPECT_LED_METHOD
    assert(eval_source("if (typeof board.led !== 'function') throw new Error('board.led missing');"));
#else
    assert(eval_source("if ('led' in board) throw new Error('board.led must be absent');"));
#endif
#if MCUJS_TEST_EXPECT_VSYS
    assert(eval_source("if (adc.VSYS !== 3) throw new Error('adc.VSYS missing');"));
#else
    assert(eval_source("if ('VSYS' in adc) throw new Error('adc.VSYS must be absent');"));
#endif

#ifdef MCUJS_TEST_INPUT_ONLY_PIN_STRING
    unsigned gpio_init_calls = mcujs_test_gpio_init_calls;
    assert(eval_source(
        "(function () { var error; try { GPIO.init(" MCUJS_TEST_INPUT_ONLY_PIN_STRING ", GPIO.OUTPUT); } "
        "catch (caught) { error = caught; } if (!(error instanceof RangeError)) "
        "throw new Error('input-only GPIO accepted output mode'); }());"));
    assert(mcujs_test_gpio_init_calls == gpio_init_calls);
    assert(eval_source("GPIO.init(" MCUJS_TEST_INPUT_ONLY_PIN_STRING ", GPIO.INPUT);"));
    assert(mcujs_test_gpio_init_calls == gpio_init_calls + 1u);
#endif

    unsigned adc_init_calls = mcujs_test_adc_init_calls;
    unsigned adc_gpio_init_calls = mcujs_test_adc_gpio_init_calls;
    unsigned adc_select_calls = mcujs_test_adc_select_calls;
    unsigned adc_read_calls = mcujs_test_adc_read_calls;
#if MCUJS_TEST_EXPECT_ADC_CHANNEL3
    assert(eval_source("adc.readChannel(3); adc.readVoltageChannel(3);"));
    assert(mcujs_test_adc_init_calls > adc_init_calls);
#if MCUJS_TEST_EXPECT_VSYS
    /* Pico channel 3 is its internal VSYS path, not an exposed ADC pad. */
    assert(mcujs_test_adc_gpio_init_calls == adc_gpio_init_calls);
#else
    assert(mcujs_test_adc_gpio_init_calls > adc_gpio_init_calls);
#endif
    assert(mcujs_test_adc_select_calls > adc_select_calls);
    assert(mcujs_test_adc_read_calls > adc_read_calls);
#else
    assert(eval_source(
        "(function () { function expectRange(call) { var error; try { call(); } "
        "catch (caught) { error = caught; } if (!(error instanceof RangeError)) "
        "throw new Error('undeclared ADC channel accepted'); } "
        "expectRange(function () { adc.readChannel(3); }); "
        "expectRange(function () { adc.readVoltageChannel(3); }); }());"));
    assert(mcujs_test_adc_init_calls == adc_init_calls);
    assert(mcujs_test_adc_gpio_init_calls == adc_gpio_init_calls);
    assert(mcujs_test_adc_select_calls == adc_select_calls);
    assert(mcujs_test_adc_read_calls == adc_read_calls);
#endif

#if MCUJS_TEST_EXPECT_NEOPIXEL
    assert(eval_source("neopixel.init({pin: 10, length: 1, order: 'RGB'});"));
    unsigned init_calls_before_invalid = mcujs_test_neopixel_init_calls;
    assert(eval_source(
        "(function () {\n"
        "  function expect(call, constructor, message) { var error; try { call(); } catch (caught) { error = caught; } if (!(error instanceof constructor)) throw new Error(message + ': ' + error); }\n"
        "  expect(function () { board.neopixel([0, 1, 256]); }, RangeError, 'wrapped array color');\n"
        "  expect(function () { board.neopixel([0, 1, 2, 3]); }, RangeError, 'oversized color array');\n"
        "  expect(function () { board.neopixel([[0, 0, 0], [1, 1, 1]]); }, RangeError, 'truncated pixel list');\n"
        "  expect(function () { board.neopixel({r: 1.5}); }, RangeError, 'fractional object color');\n"
        "  var sentinel = new URIError('board neopixel getter sentinel');\n"
        "  var color = {}; Object.defineProperty(color, 'r', {get: function () { throw sentinel; }});\n"
        "  var thrown; try { board.neopixel(color); } catch (error) { thrown = error; }\n"
        "  if (thrown !== sentinel) throw new Error('getter exception identity changed');\n"
        "  var arraySentinel = new SyntaxError('board neopixel array getter sentinel');\n"
        "  var arrayColor = [0]; Object.defineProperty(arrayColor, '0', {get: function () { throw arraySentinel; }});\n"
        "  thrown = undefined; try { board.neopixel(arrayColor); } catch (error) { thrown = error; }\n"
        "  if (thrown !== arraySentinel) throw new Error('array getter exception identity changed');\n"
        "}());"));
    assert(mcujs_test_neopixel_init_calls == init_calls_before_invalid);
    assert(eval_source("board.neopixel([1, 2, 3]);"));
    assert(mcujs_test_neopixel_last_pin == MCUJS_NEOPIXEL_PIN);
    assert(mcujs_test_neopixel_init_calls == init_calls_before_invalid + 1u);
    assert(mcujs_test_neopixel_write_calls == 1u);
    assert(mcujs_test_neopixel_last_word == 0x01020300u);
#endif

    jerry_cleanup();
    puts("RP board optional export surface passed");
    return 0;
}
