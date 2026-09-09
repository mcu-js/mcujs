/* Firmware-packaged JavaScript API. Hardware remains in canvas_native.c. */
#include "jerryscript.h"
#include "canvas_source.h"
#include "st7789_source.h"

static jerry_value_t load(const jerry_char_t *source,size_t length) {
    jerry_value_t parameters = jerry_string_sz("require,module");
    jerry_parse_options_t options = {
        .options = JERRY_PARSE_HAS_ARGUMENT_LIST,
        .argument_list = parameters,
    };
    jerry_value_t function = jerry_parse(source, length, &options);
    jerry_value_free(parameters);
    if (jerry_value_is_exception(function)) return function;
    jerry_value_t global = jerry_current_realm();
    jerry_value_t key = jerry_string_sz("require");
    jerry_value_t require = jerry_object_get(global, key);
    jerry_value_free(key);
    jerry_value_free(global);
    jerry_value_t module = jerry_object();
    jerry_value_t args[] = {require, module};
    jerry_value_t result = jerry_call(function, jerry_undefined(), args, 2);
    if (!jerry_value_is_exception(result)) {
        jerry_value_free(result);
        key = jerry_string_sz("exports");
        result = jerry_object_get(module, key);
        jerry_value_free(key);
    }
    jerry_value_free(module);
    jerry_value_free(require);
    jerry_value_free(function);
    return result;
}

jerry_value_t js_create_canvas_module(void) { return load(canvas_source,sizeof(canvas_source)); }
jerry_value_t js_create_st7789_module(void) { return load(st7789_source,sizeof(st7789_source)); }
