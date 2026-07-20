#include "bindings.h"
#include "validation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *const s_modes[] = {"input", "output"};

static jerry_value_t throw_status(mcujs_arg_status_t status) {
    if (status == MCUJS_ARG_OK) return jerry_undefined();
    return mcujs_throw_argument_error(
        status, "value must have the documented type",
        "value must satisfy the documented range");
}

static jerry_value_t required_handler(const jerry_call_info_t *call_info,
                                      const jerry_value_t args[],
                                      jerry_length_t argc) {
    (void)call_info;
    jerry_value_t value;
    mcujs_arg_status_t status = mcujs_get_required(args, argc, 0, &value);
    if (status != MCUJS_ARG_OK) return throw_status(status);
    return jerry_value_copy(value);
}

static jerry_value_t finite_handler(const jerry_call_info_t *call_info,
                                    const jerry_value_t args[],
                                    jerry_length_t argc) {
    (void)call_info;
    double value;
    mcujs_arg_status_t status = mcujs_get_finite_number(args, argc, 0, &value);
    if (status != MCUJS_ARG_OK) return throw_status(status);
    return jerry_number(value);
}

static jerry_value_t integer_handler(const jerry_call_info_t *call_info,
                                     const jerry_value_t args[],
                                     jerry_length_t argc) {
    (void)call_info;
    int value;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &value);
    if (status != MCUJS_ARG_OK) return throw_status(status);
    return jerry_number((double)value);
}

static jerry_value_t boolean_handler(const jerry_call_info_t *call_info,
                                     const jerry_value_t args[],
                                     jerry_length_t argc) {
    (void)call_info;
    bool value;
    mcujs_arg_status_t status = mcujs_get_boolean(args, argc, 0, &value);
    if (status != MCUJS_ARG_OK) return throw_status(status);
    return jerry_boolean(value);
}

static jerry_value_t enum_handler(const jerry_call_info_t *call_info,
                                  const jerry_value_t args[],
                                  jerry_length_t argc) {
    (void)call_info;
    size_t value;
    mcujs_arg_status_t status = mcujs_get_enum(
        args, argc, 0, s_modes, sizeof(s_modes) / sizeof(s_modes[0]), &value);
    if (status != MCUJS_ARG_OK) return throw_status(status);
    return jerry_number((double)value);
}

static jerry_value_t bytes_handler(const jerry_call_info_t *call_info,
                                   const jerry_value_t args[],
                                   jerry_length_t argc) {
    (void)call_info;
    uint8_t bytes[4];
    size_t length;
    jerry_value_t exception;
    mcujs_arg_status_t status = mcujs_get_byte_array(
        args, argc, 0, bytes, sizeof(bytes), 1, sizeof(bytes), &length,
        &exception);
    if (status == MCUJS_ARG_EXCEPTION) return exception;
    if (status != MCUJS_ARG_OK) return throw_status(status);

    unsigned checksum = 0;
    for (size_t i = 0; i < length; i++) checksum += bytes[i];
    return jerry_number((double)(length * 1000 + checksum));
}

static jerry_value_t number_range_handler(const jerry_call_info_t *call_info,
                                          const jerry_value_t args[],
                                          jerry_length_t argc) {
    (void)call_info;
    double value;
    mcujs_arg_status_t status =
        mcujs_get_number_range(args, argc, 0, -1.5, 1.5, &value);
    if (status != MCUJS_ARG_OK) return throw_status(status);
    return jerry_number(value);
}

static jerry_value_t integer_range_handler(const jerry_call_info_t *call_info,
                                           const jerry_value_t args[],
                                           jerry_length_t argc) {
    (void)call_info;
    int value;
    mcujs_arg_status_t status =
        mcujs_get_integer_range(args, argc, 0, -2, 2, &value);
    if (status != MCUJS_ARG_OK) return throw_status(status);
    return jerry_number((double)value);
}

static jerry_value_t not_supported_handler(const jerry_call_info_t *call_info,
                                           const jerry_value_t args[],
                                           jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    return mcujs_throw_operational_error(
        MCUJS_ERROR_NOT_SUPPORTED, "configuration cannot be represented", NULL);
}

static jerry_value_t busy_handler(const jerry_call_info_t *call_info,
                                  const jerry_value_t args[],
                                  jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    const mcujs_error_details_t details = {
        .resource = "gpio",
        .owner = "spi",
        .has_pin = true,
        .pin = 7,
        .has_bus = true,
        .bus = 1,
        .has_limit = true,
        .limit = 8,
        .has_native_code = true,
        .native_code = -5,
    };
    return mcujs_throw_operational_error(
        MCUJS_ERROR_BUSY, "resource is already owned", &details);
}

static jerry_value_t exhausted_handler(const jerry_call_info_t *call_info,
                                       const jerry_value_t args[],
                                       jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    return mcujs_throw_operational_error(
        MCUJS_ERROR_RESOURCE_EXHAUSTED, "timer pool exhausted", NULL);
}

static jerry_value_t no_device_handler(const jerry_call_info_t *call_info,
                                       const jerry_value_t args[],
                                       jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    return mcujs_throw_operational_error(
        MCUJS_ERROR_NO_DEVICE, "external device did not acknowledge", NULL);
}

static jerry_value_t io_handler(const jerry_call_info_t *call_info,
                                const jerry_value_t args[],
                                jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    return mcujs_throw_operational_error(
        MCUJS_ERROR_IO, "native driver operation failed", NULL);
}

static void set_function(jerry_value_t object, const char *name,
                         jerry_external_handler_t handler) {
    jerry_value_t function = jerry_function_external(handler);
    jerry_value_t result = jerry_object_set_sz(object, name, function);
    assert(!jerry_value_is_exception(result));
    assert(jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(function);
}

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
        fprintf(stderr, "validation test exception: %s\n", buffer);
        jerry_value_free(text);
        jerry_value_free(error);
        return false;
    }
    jerry_value_free(result);
    return true;
}

static const char s_test_source[] =
    "(function () {\n"
    "  function assert(condition, message) {\n"
    "    if (!condition) throw new Error(message);\n"
    "  }\n"
    "  function capture(call) {\n"
    "    try { call(); } catch (error) { return error; }\n"
    "    throw new Error('expected exception');\n"
    "  }\n"
    "  function assertType(call, constructor, message) {\n"
    "    var error = capture(call);\n"
    "    assert(error instanceof constructor, message + ': ' + error);\n"
    "  }\n"
    "  function assertOperational(call, name, code) {\n"
    "    var error = capture(call);\n"
    "    assert(error instanceof Error, code + ' is not an Error');\n"
    "    assert(error.name === name, code + ' name mismatch: ' + error.name);\n"
    "    assert(error.code === code, code + ' code mismatch: ' + error.code);\n"
    "    assert(typeof error.message === 'string' && error.message.length > 0, code + ' message missing');\n"
    "    return error;\n"
    "  }\n"
    "\n"
    "  assert(__backend === 'shared', 'helper test must be backend-neutral');\n"
    "  assert(validation.required('ok') === 'ok', 'required value changed');\n"
    "  assertType(function () { validation.required(); }, TypeError, 'missing required argument');\n"
    "  assertType(function () { validation.required(undefined); }, TypeError, 'undefined required argument');\n"
    "  assert(validation.finite(-1.25) === -1.25, 'finite number changed');\n"
    "  assertType(function () { validation.finite('1'); }, TypeError, 'numeric string accepted');\n"
    "  assertType(function () { validation.finite(NaN); }, TypeError, 'NaN accepted');\n"
    "  assertType(function () { validation.finite(Infinity); }, TypeError, 'Infinity accepted');\n"
    "  assert(validation.integer(-2) === -2, 'integer changed');\n"
    "  assertType(function () { validation.integer(); }, TypeError, 'missing integer accepted');\n"
    "  assertType(function () { validation.integer(1.5); }, RangeError, 'fractional integer accepted');\n"
    "  assertType(function () { validation.integer(2147483648); }, RangeError, 'overflowing integer accepted');\n"
    "  assert(validation.boolean(true) === true, 'boolean changed');\n"
    "  assertType(function () { validation.boolean(1); }, TypeError, 'truthy number accepted');\n"
    "  assert(validation.enumValue('input') === 0, 'enum index mismatch');\n"
    "  assertType(function () { validation.enumValue(0); }, TypeError, 'numeric enum accepted');\n"
    "  assertType(function () { validation.enumValue('INPUT'); }, RangeError, 'undocumented enum normalization occurred');\n"
    "  assertType(function () { validation.enumValue('input\\0'); }, RangeError, 'embedded-NUL enum prefix accepted');\n"
    "  assertType(function () { validation.enumValue('input\\0shadow'); }, RangeError, 'embedded-NUL enum suffix accepted');\n"
    "  assert(validation.bytes([0, 255]) === 2255, 'byte array changed');\n"
    "  assertType(function () { validation.bytes(1); }, TypeError, 'non-array bytes accepted');\n"
    "  assertType(function () { validation.bytes(['1']); }, TypeError, 'coerced byte accepted');\n"
    "  assertType(function () { validation.bytes([1.5]); }, RangeError, 'fractional byte accepted');\n"
    "  assertType(function () { validation.bytes([256]); }, RangeError, 'wrapped byte accepted');\n"
    "  assertType(function () { validation.bytes([]); }, RangeError, 'undersized byte array accepted');\n"
    "  assertType(function () { validation.bytes([0, 1, 2, 3, 4]); }, RangeError, 'truncated byte array accepted');\n"
    "  var accessorError = new Error('byte accessor sentinel');\n"
    "  var throwingBytes = [0];\n"
    "  Object.defineProperty(throwingBytes, '0', {get: function () { throw accessorError; }});\n"
    "  assert(capture(function () { validation.bytes(throwingBytes); }) === accessorError, 'byte accessor exception was replaced');\n"
    "  assert(validation.numberRange(1.5) === 1.5, 'number range maximum rejected');\n"
    "  assertType(function () { validation.numberRange(1.5001); }, RangeError, 'number above range accepted');\n"
    "  assertType(function () { validation.numberRange(NaN); }, TypeError, 'non-finite range value accepted');\n"
    "  assert(validation.integerRange(-2) === -2, 'integer range minimum rejected');\n"
    "  assertType(function () { validation.integerRange(1.1); }, RangeError, 'fractional ranged integer accepted');\n"
    "  assertType(function () { validation.integerRange(3); }, RangeError, 'integer above range accepted');\n"
    "\n"
    "  assertOperational(errors.notSupported, 'NotSupportedError', 'ERR_NOT_SUPPORTED');\n"
    "  var busy = assertOperational(errors.busy, 'ResourceBusyError', 'EBUSY');\n"
    "  assert(busy.resource === 'gpio', 'busy resource missing');\n"
    "  assert(busy.pin === 7, 'busy pin missing');\n"
    "  assert(busy.owner === 'spi', 'busy owner missing');\n"
    "  assert(busy.bus === 1, 'busy bus missing');\n"
    "  assert(busy.limit === 8, 'busy limit missing');\n"
    "  assert(busy.nativeCode === -5, 'busy nativeCode missing');\n"
    "  assertOperational(errors.exhausted, 'ResourceExhaustedError', 'ERR_RESOURCE_EXHAUSTED');\n"
    "  assertOperational(errors.noDevice, 'Error', 'ENXIO');\n"
    "  assertOperational(errors.io, 'Error', 'EIO');\n"
    "}());\n";

int main(void) {
    jerry_init(JERRY_INIT_EMPTY);

    jerry_value_t validation = jerry_object();
    set_function(validation, "required", required_handler);
    set_function(validation, "finite", finite_handler);
    set_function(validation, "integer", integer_handler);
    set_function(validation, "boolean", boolean_handler);
    set_function(validation, "enumValue", enum_handler);
    set_function(validation, "bytes", bytes_handler);
    set_function(validation, "numberRange", number_range_handler);
    set_function(validation, "integerRange", integer_range_handler);

    jerry_value_t errors = jerry_object();
    set_function(errors, "notSupported", not_supported_handler);
    set_function(errors, "busy", busy_handler);
    set_function(errors, "exhausted", exhausted_handler);
    set_function(errors, "noDevice", no_device_handler);
    set_function(errors, "io", io_handler);

    jerry_value_t global = jerry_current_realm();
    jerry_value_t result = jerry_object_set_sz(global, "validation", validation);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    result = jerry_object_set_sz(global, "errors", errors);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_t backend = jerry_string_sz("shared");
    const char *backend_name = "shared";
    result = jerry_object_set_sz(global, "__backend", backend);
    assert(!jerry_value_is_exception(result) && jerry_value_is_true(result));
    jerry_value_free(result);
    jerry_value_free(backend);
    jerry_value_free(global);
    jerry_value_free(errors);
    jerry_value_free(validation);

    assert(eval_source(s_test_source));
    jerry_cleanup();
    printf("shared validation/error contract passed for %s\n", backend_name);
    return 0;
}
