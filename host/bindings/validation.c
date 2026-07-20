#include "validation.h"

#include <limits.h>
#include <math.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *code;
} mcujs_error_identity_t;

static const mcujs_error_identity_t s_error_identities[] = {
    [MCUJS_ERROR_NOT_SUPPORTED] = {"NotSupportedError", "ERR_NOT_SUPPORTED"},
    [MCUJS_ERROR_BUSY] = {"ResourceBusyError", "EBUSY"},
    [MCUJS_ERROR_RESOURCE_EXHAUSTED] = {
        "ResourceExhaustedError", "ERR_RESOURCE_EXHAUSTED"},
    [MCUJS_ERROR_NO_DEVICE] = {"Error", "ENXIO"},
    [MCUJS_ERROR_IO] = {"Error", "EIO"},
};

mcujs_arg_status_t mcujs_get_required(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    jerry_value_t *out) {
    if (index >= argc || jerry_value_is_undefined(args[index])) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    *out = args[index];
    return MCUJS_ARG_OK;
}

mcujs_arg_status_t mcujs_get_finite_number(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    double *out) {
    if (index >= argc || !jerry_value_is_number(args[index])) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    double value = jerry_value_as_number(args[index]);
    if (!isfinite(value)) return MCUJS_ARG_TYPE_ERROR;
    *out = value;
    return MCUJS_ARG_OK;
}

mcujs_arg_status_t mcujs_value_to_integer(jerry_value_t value, int *out) {
    if (!jerry_value_is_number(value)) return MCUJS_ARG_TYPE_ERROR;
    double number = jerry_value_as_number(value);
    if (!isfinite(number)) return MCUJS_ARG_TYPE_ERROR;
    if (trunc(number) != number || number < INT_MIN || number > INT_MAX) {
        return MCUJS_ARG_RANGE_ERROR;
    }
    *out = (int)number;
    return MCUJS_ARG_OK;
}

mcujs_arg_status_t mcujs_get_integer(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    int *out) {
    if (index >= argc) return MCUJS_ARG_TYPE_ERROR;
    return mcujs_value_to_integer(args[index], out);
}

mcujs_arg_status_t mcujs_get_boolean(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    bool *out) {
    if (index >= argc || !jerry_value_is_boolean(args[index])) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    *out = jerry_value_is_true(args[index]);
    return MCUJS_ARG_OK;
}

mcujs_arg_status_t mcujs_get_enum(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    const char *const values[], size_t value_count, size_t *out_index) {
    if (index >= argc || !jerry_value_is_string(args[index])) {
        return MCUJS_ARG_TYPE_ERROR;
    }

    size_t maximum_length = 0;
    for (size_t i = 0; i < value_count; i++) {
        size_t length = strlen(values[i]);
        if (length > maximum_length) maximum_length = length;
    }

    jerry_size_t length =
        jerry_string_size(args[index], JERRY_ENCODING_UTF8);
    if ((size_t)length > maximum_length) return MCUJS_ARG_RANGE_ERROR;

    char candidate[maximum_length + 1];
    jerry_size_t copied = jerry_string_to_buffer(
        args[index], JERRY_ENCODING_UTF8, (jerry_char_t *)candidate, length);
    if (copied != length) return MCUJS_ARG_RANGE_ERROR;
    candidate[length] = '\0';

    for (size_t i = 0; i < value_count; i++) {
        size_t value_length = strlen(values[i]);
        if ((size_t)length == value_length &&
            memcmp(candidate, values[i], value_length) == 0) {
            *out_index = i;
            return MCUJS_ARG_OK;
        }
    }
    return MCUJS_ARG_RANGE_ERROR;
}

mcujs_arg_status_t mcujs_value_to_byte(jerry_value_t value, uint8_t *out) {
    int integer;
    mcujs_arg_status_t status = mcujs_value_to_integer(value, &integer);
    if (status != MCUJS_ARG_OK) return status;
    if (integer < 0 || integer > UINT8_MAX) return MCUJS_ARG_RANGE_ERROR;
    *out = (uint8_t)integer;
    return MCUJS_ARG_OK;
}

mcujs_arg_status_t mcujs_get_byte_array(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    uint8_t *out, size_t capacity, size_t min_length, size_t max_length,
    size_t *out_length, jerry_value_t *out_exception) {
    if (index >= argc || !jerry_value_is_array(args[index])) {
        return MCUJS_ARG_TYPE_ERROR;
    }

    size_t length = (size_t)jerry_array_length(args[index]);
    if (length < min_length || length > max_length || length > capacity) {
        return MCUJS_ARG_RANGE_ERROR;
    }

    for (size_t i = 0; i < length; i++) {
        jerry_value_t element =
            jerry_object_get_index(args[index], (uint32_t)i);
        if (jerry_value_is_exception(element)) {
            *out_exception = element;
            return MCUJS_ARG_EXCEPTION;
        }
        mcujs_arg_status_t status = mcujs_value_to_byte(element, &out[i]);
        jerry_value_free(element);
        if (status != MCUJS_ARG_OK) return status;
    }

    *out_length = length;
    return MCUJS_ARG_OK;
}

mcujs_arg_status_t mcujs_get_number_range(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    double minimum, double maximum, double *out) {
    double value;
    mcujs_arg_status_t status =
        mcujs_get_finite_number(args, argc, index, &value);
    if (status != MCUJS_ARG_OK) return status;
    if (value < minimum || value > maximum) return MCUJS_ARG_RANGE_ERROR;
    *out = value;
    return MCUJS_ARG_OK;
}

mcujs_arg_status_t mcujs_get_integer_range(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    int minimum, int maximum, int *out) {
    int value;
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, index, &value);
    if (status != MCUJS_ARG_OK) return status;
    if (value < minimum || value > maximum) return MCUJS_ARG_RANGE_ERROR;
    *out = value;
    return MCUJS_ARG_OK;
}

jerry_value_t mcujs_throw_argument_error(
    mcujs_arg_status_t status, const char *type_message,
    const char *range_message) {
    if (status != MCUJS_ARG_TYPE_ERROR && status != MCUJS_ARG_RANGE_ERROR) {
        return jerry_throw_sz(JERRY_ERROR_COMMON,
                              "invalid argument-validation status");
    }
    return jerry_throw_sz(
        status == MCUJS_ARG_TYPE_ERROR ? JERRY_ERROR_TYPE : JERRY_ERROR_RANGE,
        status == MCUJS_ARG_TYPE_ERROR ? type_message : range_message);
}

jerry_value_t mcujs_throw_arg(
    mcujs_arg_status_t status, const char *type_message,
    const char *range_message) {
    return mcujs_throw_argument_error(status, type_message, range_message);
}

static jerry_value_t set_string_property(jerry_value_t object,
                                         const char *name,
                                         const char *value) {
    jerry_value_t string = jerry_string_sz(value);
    jerry_value_t result = jerry_object_set_sz(object, name, string);
    jerry_value_free(string);
    return result;
}

static jerry_value_t set_number_property(jerry_value_t object,
                                         const char *name,
                                         double value) {
    jerry_value_t number = jerry_number(value);
    jerry_value_t result = jerry_object_set_sz(object, name, number);
    jerry_value_free(number);
    return result;
}

static bool property_set_succeeded(jerry_value_t result) {
    return !jerry_value_is_exception(result) && jerry_value_is_true(result);
}

static jerry_value_t property_failure(jerry_value_t error,
                                      jerry_value_t result) {
    jerry_value_free(error);
    if (jerry_value_is_exception(result)) return result;
    jerry_value_free(result);
    return jerry_throw_sz(JERRY_ERROR_COMMON,
                          "unable to create structured operational error");
}

#define SET_ERROR_PROPERTY(expression)                   \
    do {                                                 \
        jerry_value_t result = (expression);             \
        if (!property_set_succeeded(result)) {           \
            return property_failure(error_object, result); \
        }                                                \
        jerry_value_free(result);                        \
    } while (0)

jerry_value_t mcujs_throw_operational_error(
    mcujs_operational_error_t error, const char *message,
    const mcujs_error_details_t *details) {
    if ((size_t)error >=
        sizeof(s_error_identities) / sizeof(s_error_identities[0])) {
        return jerry_throw_sz(JERRY_ERROR_COMMON,
                              "unknown MCU.js operational error");
    }

    const mcujs_error_identity_t *identity = &s_error_identities[error];
    jerry_value_t error_object = jerry_error_sz(JERRY_ERROR_COMMON, message);
    SET_ERROR_PROPERTY(
        set_string_property(error_object, "name", identity->name));
    SET_ERROR_PROPERTY(
        set_string_property(error_object, "code", identity->code));

    if (details != NULL) {
        if (details->resource != NULL) {
            SET_ERROR_PROPERTY(set_string_property(
                error_object, "resource", details->resource));
        }
        if (details->owner != NULL) {
            SET_ERROR_PROPERTY(set_string_property(
                error_object, "owner", details->owner));
        }
        if (details->has_pin) {
            SET_ERROR_PROPERTY(set_number_property(
                error_object, "pin", (double)details->pin));
        }
        if (details->has_bus) {
            SET_ERROR_PROPERTY(set_number_property(
                error_object, "bus", (double)details->bus));
        }
        if (details->has_limit) {
            SET_ERROR_PROPERTY(set_number_property(
                error_object, "limit", details->limit));
        }
        if (details->has_native_code) {
            SET_ERROR_PROPERTY(set_number_property(
                error_object, "nativeCode", (double)details->native_code));
        }
    }

    return jerry_throw_value(error_object, true);
}

#undef SET_ERROR_PROPERTY
