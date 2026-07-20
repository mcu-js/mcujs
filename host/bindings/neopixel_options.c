#include "neopixel_options.h"

#include "runtime_features.h"
#include "validation.h"

#include <stddef.h>
#include <string.h>

static jerry_value_t get_property(jerry_value_t object, const char *name) {
    jerry_value_t key = jerry_string_sz(name);
    jerry_value_t value = jerry_object_get(object, key);
    jerry_value_free(key);
    return value;
}

static bool option_key_allowed(jerry_value_t key) {
    static const char *const names[] = {"pin", "length", "order"};
    if (!jerry_value_is_string(key)) return false;

    jerry_size_t length = jerry_string_size(key, JERRY_ENCODING_UTF8);
    if (length > 6) return false;
    char candidate[6];
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
                                  "Unknown NeoPixel init option");
        }
    }
    jerry_value_free(keys);
    return jerry_undefined();
}

static bool read_integer_property(jerry_value_t options, const char *name,
                                  const char *type_message,
                                  const char *range_message, int *out,
                                  jerry_value_t *error) {
    jerry_value_t value = get_property(options, name);
    if (jerry_value_is_exception(value)) {
        *error = value;
        return false;
    }
    if (jerry_value_is_undefined(value)) {
        jerry_value_free(value);
        *error = jerry_throw_sz(JERRY_ERROR_TYPE, type_message);
        return false;
    }

    mcujs_arg_status_t status = mcujs_value_to_integer(value, out);
    jerry_value_free(value);
    if (status == MCUJS_ARG_OK) return true;
    *error = mcujs_throw_arg(status, type_message, range_message);
    return false;
}

static bool pin_supported(int pin) {
    return pin >= 0 && pin < 64 &&
           (MCUJS_RUNTIME_NEOPIXEL_PIN_MASK &
            (1ULL << (unsigned)pin)) != 0;
}

static jerry_value_t read_order(jerry_value_t options, bool *grb) {
    jerry_value_t value = get_property(options, "order");
    if (jerry_value_is_exception(value)) return value;
    if (jerry_value_is_undefined(value)) {
        jerry_value_free(value);
#if MCUJS_RUNTIME_NEOPIXEL_ORDER_GRB
        *grb = true;
        return jerry_undefined();
#else
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "Default NeoPixel order is not supported");
#endif
    }
    if (!jerry_value_is_string(value)) {
        jerry_value_free(value);
        return jerry_throw_sz(JERRY_ERROR_TYPE,
                              "neopixel order must be RGB or GRB");
    }

    jerry_size_t size = jerry_string_size(value, JERRY_ENCODING_UTF8);
    char order[3];
    if (size != sizeof(order) ||
        jerry_string_to_buffer(value, JERRY_ENCODING_UTF8,
                               (jerry_char_t *)order, size) != size) {
        jerry_value_free(value);
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "neopixel order must be RGB or GRB");
    }
    jerry_value_free(value);

    if (memcmp(order, "RGB", sizeof(order)) == 0) {
#if MCUJS_RUNTIME_NEOPIXEL_ORDER_RGB
        *grb = false;
        return jerry_undefined();
#else
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "RGB NeoPixel order is not supported");
#endif
    }
    if (memcmp(order, "GRB", sizeof(order)) == 0) {
#if MCUJS_RUNTIME_NEOPIXEL_ORDER_GRB
        *grb = true;
        return jerry_undefined();
#else
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "GRB NeoPixel order is not supported");
#endif
    }
    return jerry_throw_sz(JERRY_ERROR_RANGE,
                          "neopixel order must be RGB or GRB");
}

jerry_value_t mcujs_parse_neopixel_init_args(
    const jerry_value_t args[], jerry_length_t argc,
    mcujs_neopixel_init_options_t *out) {
    if (argc < 1 || !jerry_value_is_object(args[0]) ||
        jerry_value_is_array(args[0]) || jerry_value_is_function(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE,
                              "neopixel.init requires an options object");
    }

    jerry_value_t result = validate_option_keys(args[0]);
    if (jerry_value_is_exception(result)) return result;
    jerry_value_free(result);

    jerry_value_t error;
    if (!read_integer_property(args[0], "pin",
                               "neopixel pin must be a finite number",
                               "neopixel pin must be an integer", &out->pin,
                               &error)) {
        return error;
    }
    if (!read_integer_property(args[0], "length",
                               "neopixel length must be a finite number",
                               "neopixel length must be an integer", &out->length,
                               &error)) {
        return error;
    }
    if (!pin_supported(out->pin)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "NeoPixel pin is not available on this board");
    }
    if (out->length < 1 ||
        out->length > MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH) {
        return jerry_throw_sz(JERRY_ERROR_RANGE,
                              "NeoPixel length is outside the advertised range");
    }
    return read_order(args[0], &out->grb);
}
