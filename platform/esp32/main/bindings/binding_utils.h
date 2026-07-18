#ifndef MCUJS_ESP32_BINDING_UTILS_H
#define MCUJS_ESP32_BINDING_UTILS_H

#include "jerryscript.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MCUJS_ARG_OK = 0,
    MCUJS_ARG_TYPE_ERROR,
    MCUJS_ARG_RANGE_ERROR,
} mcujs_arg_status_t;

static inline mcujs_arg_status_t mcujs_get_finite_number(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index, double *out) {
    if (index >= argc || !jerry_value_is_number(args[index])) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    double value = jerry_value_as_number(args[index]);
    if (!isfinite(value)) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    *out = value;
    return MCUJS_ARG_OK;
}

static inline mcujs_arg_status_t mcujs_value_to_integer(jerry_value_t value, int *out) {
    if (!jerry_value_is_number(value)) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    double number = jerry_value_as_number(value);
    if (!isfinite(number)) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    if (trunc(number) != number || number < INT_MIN || number > INT_MAX) {
        return MCUJS_ARG_RANGE_ERROR;
    }
    *out = (int)number;
    return MCUJS_ARG_OK;
}

static inline mcujs_arg_status_t mcujs_get_integer(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index, int *out) {
    if (index >= argc) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    return mcujs_value_to_integer(args[index], out);
}

static inline mcujs_arg_status_t mcujs_value_to_byte(jerry_value_t value, uint8_t *out) {
    if (!jerry_value_is_number(value)) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    double number = jerry_value_as_number(value);
    if (!isfinite(number)) {
        return MCUJS_ARG_TYPE_ERROR;
    }
    if (trunc(number) != number || number < 0 || number > 255) {
        return MCUJS_ARG_RANGE_ERROR;
    }
    *out = (uint8_t)number;
    return MCUJS_ARG_OK;
}

static inline jerry_value_t mcujs_throw_arg(
    mcujs_arg_status_t status, const char *type_message, const char *range_message) {
    return jerry_throw_sz(status == MCUJS_ARG_TYPE_ERROR ? JERRY_ERROR_TYPE : JERRY_ERROR_RANGE,
                          status == MCUJS_ARG_TYPE_ERROR ? type_message : range_message);
}

#endif /* MCUJS_ESP32_BINDING_UTILS_H */
