#include "bindings.h"
#include "pin_policy.h"
#include "runtime_features.h"
#include "runtime_validation_backend_stubs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(MCUJS_PLATFORM_ESP32)
#include "esp_err.h"
#else
#include "hardware/spi.h"
#endif

#define EXPECTED_SPI_MAX MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES
#define EXPECTED_DEFAULT_BUS MCUJS_RUNTIME_SPI_DEFAULT_BUS
#define EXPECTED_SCK MCUJS_RUNTIME_SPI_DEFAULT_SCK
#define EXPECTED_MOSI MCUJS_RUNTIME_SPI_DEFAULT_MOSI
#define EXPECTED_MISO MCUJS_RUNTIME_SPI_DEFAULT_MISO

typedef struct {
    int bus;
    int sck;
    int mosi;
    int miso;
} spi_route_t;

#define MCUJS_SPI_TEST_ROUTE(bus, sck, mosi, miso) {bus, sck, mosi, miso},
static const spi_route_t s_routes[] = {
    MCUJS_RUNTIME_SPI_ROUTES(MCUJS_SPI_TEST_ROUTE)
};
#undef MCUJS_SPI_TEST_ROUTE

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
        fprintf(stderr, "SPI backend test exception: %s\n", buffer);
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

static bool assert_operational_error(const char *operation, const char *name,
                                     const char *code) {
    char source[1024];
    int written = snprintf(
        source, sizeof(source),
        "(function () { var error; try { %s; } catch (caught) { error = caught; } "
        "if (!(error instanceof Error)) throw new Error('missing %s'); "
        "if (error.name !== '%s') throw new Error('wrong %s name: ' + error.name); "
        "if (error.code !== '%s') throw new Error('wrong %s code: ' + error.code); "
        "if (error.resource !== 'spi') throw new Error('%s resource mismatch'); "
        "if (typeof error.message !== 'string' || error.message.length === 0) "
        "throw new Error('%s message missing'); }());",
        operation, code, name, code, code, code, code, code);
    return written > 0 && (size_t)written < sizeof(source) && eval_source(source);
}

int main(void) {
    mcujs_test_reset_backend();
    jerry_init(JERRY_INIT_EMPTY);
    install_module("SPI", js_create_spi_module());
    install_number("__spiMax", EXPECTED_SPI_MAX);
    install_number("__spiBus", EXPECTED_DEFAULT_BUS);
    install_number("__spiOtherBus", EXPECTED_DEFAULT_BUS == 0 ? 1 : 0);
    install_number("__spiSck", EXPECTED_SCK);
    install_number("__spiMosi", EXPECTED_MOSI);
    install_number("__spiMiso", EXPECTED_MISO);
    install_number("__spiMinHz", MCUJS_RUNTIME_SPI_MIN_HZ);
    install_number("__spiMaxHz", MCUJS_RUNTIME_SPI_MAX_HZ);
    const spi_route_t *other_route = NULL;
    for (size_t i = 0; i < sizeof(s_routes) / sizeof(s_routes[0]); i++) {
        if (s_routes[i].bus != EXPECTED_DEFAULT_BUS) {
            other_route = &s_routes[i];
            break;
        }
    }
#if defined(MCUJS_BOARD_PICO)
    assert(other_route != NULL);
#endif
    install_number("__spiHasOtherRoute", other_route != NULL ? 1 : 0);
    install_number("__spiOtherSck", other_route != NULL ? other_route->sck : 0);
    install_number("__spiOtherMosi", other_route != NULL ? other_route->mosi : 0);
    install_number("__spiOtherMiso", other_route != NULL ? other_route->miso : 0);

    assert(eval_source(
        "(function () {\n"
        "function assert(c, m) { if (!c) throw new Error(m); }\n"
        "function capture(f) { try { f(); } catch (e) { return e; } throw new Error('expected exception'); }\n"
        "function expect(f, ctor, m) { var e = capture(f); assert(e instanceof ctor && !('code' in e), m + ': ' + e); }\n"
        "expect(function () { SPI.init(null); }, TypeError, 'null options accepted');\n"
        "expect(function () { SPI.init([]); }, TypeError, 'array options accepted');\n"
        "expect(function () { SPI.init({}); }, TypeError, 'missing frequency accepted');\n"
        "expect(function () { SPI.init({frequency: '1250000'}); }, TypeError, 'string frequency accepted');\n"
        "expect(function () { SPI.init({frequency: 1250000.5}); }, RangeError, 'fractional frequency accepted');\n"
        "expect(function () { SPI.init({frequency: 1250000, mode: '0'}); }, TypeError, 'string mode accepted');\n"
        "expect(function () { SPI.init({frequency: 1250000, mode: 0.5}); }, RangeError, 'fractional mode accepted');\n"
        "expect(function () { SPI.init({frequency: 1250000, mode: 1}); }, RangeError, 'unadvertised mode accepted');\n"
        "expect(function () { SPI.init({frequency: __spiMinHz - 1}); }, RangeError, 'below-minimum frequency accepted');\n"
        "expect(function () { SPI.init({frequency: __spiMaxHz + 1}); }, RangeError, 'above-maximum frequency accepted');\n"
        "expect(function () { SPI.init({frequency: 1250000, bitsPerWord: 8}); }, RangeError, 'selectable word size invented');\n"
        "expect(function () { SPI.init({frequency: 1250000, bitOrder: 'msb'}); }, RangeError, 'selectable bit order invented');\n"
        "expect(function () { SPI.init({frequency: 1250000, sck: __spiSck}); }, RangeError, 'partial route accepted');\n"
        "expect(function () { SPI.init({bus: __spiOtherBus, frequency: 1250000}); }, RangeError, 'non-default bus selected default pins');\n"
        "expect(function () { SPI.init({bus: __spiBus, sck: __spiMiso, mosi: __spiMosi, miso: __spiSck, frequency: 1250000}); }, RangeError, 'unlisted route accepted');\n"
        "var sentinel = new URIError('SPI options getter sentinel'); var later = 0; var options = {};\n"
        "Object.defineProperty(options, 'bus', {enumerable: true, get: function () { throw sentinel; }});\n"
        "Object.defineProperty(options, 'frequency', {enumerable: true, get: function () { later++; return 1250000; }});\n"
        "assert(capture(function () { SPI.init(options); }) === sentinel && later === 0, 'getter exception replaced or did not short-circuit');\n"
        "expect(function () { SPI.transfer(__spiBus); }, TypeError, 'missing transfer data accepted');\n"
        "expect(function () { SPI.transfer(__spiBus, []); }, RangeError, 'empty transfer accepted');\n"
        "expect(function () { SPI.transfer(__spiBus, ['1']); }, TypeError, 'string byte accepted');\n"
        "expect(function () { SPI.transfer(__spiBus, [1.5]); }, RangeError, 'fractional byte accepted');\n"
        "var tooLarge = []; for (var i = 0; i < __spiMax + 1; i++) tooLarge.push(0);\n"
        "expect(function () { SPI.transfer(__spiBus, tooLarge); }, RangeError, 'maximum+1 accepted');\n"
        "var byteSentinel = new SyntaxError('SPI byte getter sentinel'); var throwing = [0];\n"
        "Object.defineProperty(throwing, '0', {get: function () { throw byteSentinel; }});\n"
        "assert(capture(function () { SPI.transfer(__spiBus, throwing); }) === byteSentinel, 'byte getter exception replaced');\n"
        "var busy = capture(function () { SPI.transfer(__spiBus, 1); });\n"
        "assert(busy.name === 'ResourceBusyError' && busy.code === 'EBUSY' && busy.resource === 'spi', 'uninitialized transfer error mismatch');\n"
        "if (__spiHasOtherRoute) SPI.init({bus: __spiOtherBus, sck: __spiOtherSck, mosi: __spiOtherMosi, miso: __spiOtherMiso, frequency: 1250000, mode: 0});\n"
        "SPI.init({frequency: 1250000});\n"
        "assert(SPI.transfer(__spiBus, 165) === 165, 'scalar return changed');\n"
        "var pair = SPI.transfer(__spiBus, [0, 255]);\n"
        "assert(pair.length === 2 && pair[0] === 0 && pair[1] === 255, 'array return changed');\n"
        "var maximum = []; for (var j = 0; j < __spiMax; j++) maximum.push(j & 255);\n"
        "var received = SPI.transfer(__spiBus, maximum);\n"
        "assert(received.length === __spiMax && received[0] === 0 && received[__spiMax - 1] === ((__spiMax - 1) & 255), 'exact maximum changed');\n"
        "SPI.init(__spiBus, __spiSck, __spiMosi, __spiMiso, 1250000);\n"
#if defined(MCUJS_PLATFORM_RP2)
        "assert(typeof SPI.writeBufferDMA === 'function', 'RP DMA compatibility method absent');\n"
#else
        "assert(!('writeBufferDMA' in SPI), 'ESP invented DMA compatibility method');\n"
#endif
        "}());"));

#if defined(MCUJS_PLATFORM_RP2)
    assert(mcujs_test_spi_last_init_bus == EXPECTED_DEFAULT_BUS);
    assert(mcujs_test_spi_last_format_bits == 8u);
    assert(mcujs_test_spi_last_format_order == SPI_MSB_FIRST);
    assert(mcujs_test_spi_last_format_cpol == SPI_CPOL_0);
    assert(mcujs_test_spi_last_format_cpha == SPI_CPHA_0);
    assert(mcujs_test_spi_last_transfer_length == EXPECTED_SPI_MAX);
    mcujs_test_spi_transfer_result = -1;
#else
    assert(mcujs_test_spi_last_host == EXPECTED_DEFAULT_BUS);
    assert(mcujs_test_spi_last_frequency == 1250000);
    assert(mcujs_test_spi_last_mode == 0);
    assert(mcujs_test_spi_last_transfer_length == EXPECTED_SPI_MAX);
    mcujs_test_spi_transmit_result = ESP_FAIL;
#endif
    assert(assert_operational_error("SPI.transfer(__spiBus, [1])", "Error", "EIO"));

#if defined(MCUJS_PLATFORM_RP2)
    mcujs_test_spi_transfer_result = 0;
    mcujs_test_spi_init_result = 1000000;
    assert(assert_operational_error("SPI.init({frequency: 1250000})",
                                    "NotSupportedError", "ERR_NOT_SUPPORTED"));
#else
    mcujs_test_spi_transmit_result = ESP_OK;
    mcujs_test_spi_actual_frequency = 1000000;
    assert(assert_operational_error("SPI.init({frequency: 1250000})",
                                    "NotSupportedError",
                                    "ERR_NOT_SUPPORTED"));
    mcujs_test_spi_actual_frequency = 0;
    assert(eval_source("SPI.transfer(__spiBus, [1]);"));

    mcujs_test_spi_remove_result = ESP_ERR_INVALID_STATE;
    assert(assert_operational_error("SPI.init({frequency: 1250000})",
                                    "ResourceBusyError", "EBUSY"));
    mcujs_test_spi_remove_result = ESP_OK;
    assert(eval_source("SPI.transfer(__spiBus, [1]);"));

    mcujs_test_spi_bus_free_result = ESP_ERR_INVALID_STATE;
    assert(assert_operational_error("SPI.init({frequency: 1250000})",
                                    "ResourceBusyError", "EBUSY"));
    assert(mcujs_pin_owner(EXPECTED_SCK) ==
           (EXPECTED_DEFAULT_BUS == 0 ? MCUJS_PIN_OWNER_SPI0
                                      : MCUJS_PIN_OWNER_SPI1));
    mcujs_test_spi_bus_free_result = ESP_OK;
    assert(assert_operational_error("SPI.transfer(__spiBus, [1])",
                                    "ResourceBusyError", "EBUSY"));
    assert(eval_source("SPI.init({frequency: 1250000});"));

    mcujs_test_spi_bus_init_result = ESP_ERR_NO_MEM;
    assert(assert_operational_error("SPI.init({frequency: 1250000})",
                                    "ResourceExhaustedError",
                                    "ERR_RESOURCE_EXHAUSTED"));
    mcujs_test_spi_bus_init_result = ESP_OK;
    assert(assert_operational_error("SPI.transfer(__spiBus, [1])",
                                    "ResourceBusyError", "EBUSY"));

    mcujs_test_spi_add_result = ESP_FAIL;
    mcujs_test_spi_bus_free_result = ESP_ERR_INVALID_STATE;
    assert(assert_operational_error("SPI.init({frequency: 1250000})",
                                    "ResourceBusyError", "EBUSY"));
    assert(mcujs_pin_owner(EXPECTED_SCK) ==
           (EXPECTED_DEFAULT_BUS == 0 ? MCUJS_PIN_OWNER_SPI0
                                      : MCUJS_PIN_OWNER_SPI1));
    mcujs_test_spi_add_result = ESP_OK;
    mcujs_test_spi_bus_free_result = ESP_OK;
    assert(eval_source("SPI.init({frequency: 1250000});"));
#endif

    jerry_cleanup();
#if defined(MCUJS_PLATFORM_ESP32)
    puts("production ESP32 SPI options, 64-byte boundary, lifecycle, and errors passed");
#else
    puts("production RP SPI options, 256-byte boundary, format, DMA gate, and errors passed");
#endif
    return 0;
}
