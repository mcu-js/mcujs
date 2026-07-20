#include "i2c_options.h"

#include "runtime_features.h"
#include "validation.h"

#include <stdbool.h>
#include <string.h>

static jerry_value_t get_property(jerry_value_t object, const char *name) {
    jerry_value_t key = jerry_string_sz(name);
    jerry_value_t value = jerry_object_get(object, key);
    jerry_value_free(key);
    return value;
}

static bool option_key_allowed(jerry_value_t key) {
    static const char *const names[] = {"bus", "frequency", "sda", "scl"};
    if (!jerry_value_is_string(key)) return false;

    jerry_size_t length = jerry_string_size(key, JERRY_ENCODING_UTF8);
    if (length > 9) return false;
    char candidate[10];
    jerry_size_t copied = jerry_string_to_buffer(
        key, JERRY_ENCODING_UTF8, (jerry_char_t *)candidate, length);
    if (copied != length) return false;

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        size_t expected = strlen(names[i]);
        if ((size_t)length == expected &&
            memcmp(candidate, names[i], expected) == 0) {
            return true;
        }
    }
    return false;
}

static jerry_value_t validate_option_keys(jerry_value_t options) {
    jerry_value_t keys = jerry_object_keys(options);
    if (jerry_value_is_exception(keys)) return keys;

    uint32_t length = jerry_array_length(keys);
    for (uint32_t i = 0; i < length; i++) {
        jerry_value_t key = jerry_object_get_index(keys, i);
        if (jerry_value_is_exception(key)) {
            jerry_value_free(keys);
            return key;
        }
        bool allowed = option_key_allowed(key);
        jerry_value_free(key);
        if (!allowed) {
            jerry_value_free(keys);
            return jerry_throw_sz(JERRY_ERROR_RANGE,
                                  "Unknown I2C init option");
        }
    }
    jerry_value_free(keys);
    return jerry_undefined();
}

static bool read_integer_property(jerry_value_t options, const char *name,
                                  bool required, const char *type_message,
                                  const char *range_message, bool *present,
                                  int *out, jerry_value_t *error) {
    jerry_value_t value = get_property(options, name);
    if (jerry_value_is_exception(value)) {
        *error = value;
        return false;
    }
    if (jerry_value_is_undefined(value)) {
        jerry_value_free(value);
        *present = false;
        if (!required) return true;
        *error = jerry_throw_sz(JERRY_ERROR_TYPE, type_message);
        return false;
    }

    *present = true;
    mcujs_arg_status_t status = mcujs_value_to_integer(value, out);
    jerry_value_free(value);
    if (status == MCUJS_ARG_OK) return true;
    *error = mcujs_throw_arg(status, type_message, range_message);
    return false;
}

static bool route_supported(int bus, int sda, int scl) {
#define MCUJS_MATCH_I2C_ROUTE(route_bus, route_sda, route_scl) \
    if (bus == (route_bus) && sda == (route_sda) && scl == (route_scl)) return true;
    MCUJS_RUNTIME_I2C_ROUTES(MCUJS_MATCH_I2C_ROUTE)
#undef MCUJS_MATCH_I2C_ROUTE
    return false;
}

static jerry_value_t validate_init_options(mcujs_i2c_init_options_t *options) {
    if (options->frequency < MCUJS_RUNTIME_I2C_MIN_HZ ||
        options->frequency > MCUJS_RUNTIME_I2C_MAX_HZ) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "I2C frequency is outside the advertised range");
    }
    if (!route_supported(options->bus, options->sda, options->scl)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "I2C route is not supported by this board");
    }
    return jerry_undefined();
}

static jerry_value_t parse_options(jerry_value_t options,
                                   mcujs_i2c_init_options_t *out) {
    if (jerry_value_is_array(options) || jerry_value_is_function(options)) {
        return jerry_throw_sz(JERRY_ERROR_TYPE,
                              "I2C.init requires an options object");
    }

    jerry_value_t keys_result = validate_option_keys(options);
    if (jerry_value_is_exception(keys_result)) return keys_result;
    jerry_value_free(keys_result);

    bool has_bus;
    bool has_frequency;
    bool has_sda;
    bool has_scl;
    jerry_value_t error;
    if (!read_integer_property(options, "bus", false,
                               "I2C bus must be a finite number",
                               "I2C bus must be an integer", &has_bus,
                               &out->bus, &error)) {
        return error;
    }
    if (!has_bus) out->bus = MCUJS_RUNTIME_I2C_DEFAULT_BUS;

    if (!read_integer_property(options, "frequency", true,
                               "I2C frequency must be a finite number",
                               "I2C frequency must be an integer", &has_frequency,
                               &out->frequency, &error)) {
        return error;
    }
    (void)has_frequency;

    if (!read_integer_property(options, "sda", false,
                               "I2C SDA pin must be a finite number",
                               "I2C SDA pin must be an integer", &has_sda,
                               &out->sda, &error)) {
        return error;
    }
    if (!read_integer_property(options, "scl", false,
                               "I2C SCL pin must be a finite number",
                               "I2C SCL pin must be an integer", &has_scl,
                               &out->scl, &error)) {
        return error;
    }

    if (has_sda != has_scl) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "I2C SDA and SCL must be supplied together");
    }
    if (!has_sda) {
        if (out->bus != MCUJS_RUNTIME_I2C_DEFAULT_BUS) {
            return jerry_throw_sz(
                JERRY_ERROR_RANGE,
                "A non-default I2C bus requires an explicit listed route");
        }
        out->sda = MCUJS_RUNTIME_I2C_DEFAULT_SDA;
        out->scl = MCUJS_RUNTIME_I2C_DEFAULT_SCL;
    }
    return validate_init_options(out);
}

static jerry_value_t parse_positional(const jerry_value_t args[],
                                      jerry_length_t argc,
                                      mcujs_i2c_init_options_t *out) {
    mcujs_arg_status_t status = mcujs_get_integer(args, argc, 0, &out->bus);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C bus must be a finite number",
                              "I2C bus must be an integer");
    }
    status = mcujs_get_integer(args, argc, 1, &out->sda);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C SDA pin must be a finite number",
                              "I2C SDA pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 2, &out->scl);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C SCL pin must be a finite number",
                              "I2C SCL pin must be an integer");
    }
    status = mcujs_get_integer(args, argc, 3, &out->frequency);
    if (status != MCUJS_ARG_OK) {
        return mcujs_throw_arg(status, "I2C frequency must be a finite number",
                              "I2C frequency must be an integer");
    }
    return validate_init_options(out);
}

jerry_value_t mcujs_parse_i2c_init_args(
    const jerry_value_t args[], jerry_length_t argc,
    mcujs_i2c_init_options_t *out) {
    if (argc > 0 && jerry_value_is_object(args[0])) {
        return parse_options(args[0], out);
    }
    return parse_positional(args, argc, out);
}
