#ifndef MCUJS_VALIDATION_H
#define MCUJS_VALIDATION_H

#include "jerryscript.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MCUJS_ARG_OK = 0,
    MCUJS_ARG_TYPE_ERROR,
    MCUJS_ARG_RANGE_ERROR,
    MCUJS_ARG_EXCEPTION,
} mcujs_arg_status_t;

/* Required values are borrowed from args and remain owned by the caller. */
mcujs_arg_status_t mcujs_get_required(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    jerry_value_t *out);

mcujs_arg_status_t mcujs_get_finite_number(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    double *out);

mcujs_arg_status_t mcujs_value_to_integer(jerry_value_t value, int *out);
mcujs_arg_status_t mcujs_get_integer(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    int *out);

mcujs_arg_status_t mcujs_get_boolean(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    bool *out);

/* Enum values are exact and case-sensitive; callers may normalize explicitly. */
mcujs_arg_status_t mcujs_get_enum(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    const char *const values[], size_t value_count, size_t *out_index);

mcujs_arg_status_t mcujs_value_to_byte(jerry_value_t value, uint8_t *out);
mcujs_arg_status_t mcujs_get_byte_array(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    uint8_t *out, size_t capacity, size_t min_length, size_t max_length,
    size_t *out_length, jerry_value_t *out_exception);

/* MCUJS_ARG_EXCEPTION transfers the original owned exception to the caller
 * through out_exception. The caller must return or free that value exactly
 * once; other statuses leave out_exception untouched. */

mcujs_arg_status_t mcujs_get_number_range(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    double minimum, double maximum, double *out);

mcujs_arg_status_t mcujs_get_integer_range(
    const jerry_value_t args[], jerry_length_t argc, jerry_length_t index,
    int minimum, int maximum, int *out);

jerry_value_t mcujs_throw_argument_error(
    mcujs_arg_status_t status, const char *type_message,
    const char *range_message);

/* Compatibility spelling used by existing ESP32 bindings. */
jerry_value_t mcujs_throw_arg(
    mcujs_arg_status_t status, const char *type_message,
    const char *range_message);

typedef enum {
    MCUJS_ERROR_NOT_SUPPORTED = 0,
    MCUJS_ERROR_BUSY,
    MCUJS_ERROR_RESOURCE_EXHAUSTED,
    MCUJS_ERROR_NO_DEVICE,
    MCUJS_ERROR_IO,
} mcujs_operational_error_t;

typedef struct {
    const char *resource;
    const char *owner;
    bool has_pin;
    int pin;
    bool has_bus;
    int bus;
    bool has_limit;
    double limit;
    bool has_native_code;
    int native_code;
} mcujs_error_details_t;

jerry_value_t mcujs_throw_operational_error(
    mcujs_operational_error_t error, const char *message,
    const mcujs_error_details_t *details);

#endif /* MCUJS_VALIDATION_H */
