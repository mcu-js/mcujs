/*
 * mcujs - Bindings Common Implementation
 * 
 * Utility functions shared by all bindings
 */

#include "bindings.h"
#include "jerryscript.h"

#include <string.h>

/*
 * Helper: Create a property on an object
 */
void js_set_property(jerry_value_t object, const char *name, jerry_value_t value) {
    jerry_value_t prop_name = jerry_string_sz(name);
    jerry_object_set(object, prop_name, value);
    jerry_value_free(prop_name);
}

/*
 * Helper: Create a function property on an object
 */
void js_set_function(jerry_value_t object, const char *name, 
                     jerry_external_handler_t handler) {
    jerry_value_t func = jerry_function_external(handler);
    js_set_property(object, name, func);
    jerry_value_free(func);
}

/*
 * Helper: Create a number property on an object
 */
void js_set_number(jerry_value_t object, const char *name, double value) {
    jerry_value_t num = jerry_number(value);
    js_set_property(object, name, num);
    jerry_value_free(num);
}

/*
 * Helper: Create a string property on an object
 */
void js_set_string(jerry_value_t object, const char *name, const char *value) {
    jerry_value_t str = jerry_string_sz(value);
    js_set_property(object, name, str);
    jerry_value_free(str);
}

/*
 * Helper: Create a boolean property on an object
 */
void js_set_boolean(jerry_value_t object, const char *name, bool value) {
    jerry_value_t bool_val = jerry_boolean(value);
    js_set_property(object, name, bool_val);
    jerry_value_free(bool_val);
}

/*
 * Helper: Register an object on the global scope
 */
void js_register_global(const char *name, jerry_value_t object) {
    jerry_value_t global = jerry_current_realm();
    js_set_property(global, name, object);
    jerry_value_free(global);
}

/*
 * Helper: Get a number argument with default value
 */
double js_get_number_arg(const jerry_value_t args[], jerry_length_t argc, 
                         jerry_length_t index, double default_value) {
    if (index >= argc) {
        return default_value;
    }
    
    if (!jerry_value_is_number(args[index])) {
        return default_value;
    }
    
    return jerry_value_as_number(args[index]);
}

/*
 * Helper: Get a boolean argument with default value
 */
bool js_get_boolean_arg(const jerry_value_t args[], jerry_length_t argc,
                        jerry_length_t index, bool default_value) {
    if (index >= argc) {
        return default_value;
    }
    
    return jerry_value_to_boolean(args[index]);
}

/*
 * Helper: Convert JS string argument to C string
 * Returns length of string, or 0 on error
 * Caller must ensure buffer is large enough
 */
size_t js_get_string_arg(const jerry_value_t args[], jerry_length_t argc,
                         jerry_length_t index, char *buffer, size_t buffer_size) {
    if (index >= argc || buffer == NULL || buffer_size == 0) {
        return 0;
    }
    
    jerry_value_t str_val;
    bool need_free = false;
    
    if (jerry_value_is_string(args[index])) {
        str_val = args[index];
    } else {
        str_val = jerry_value_to_string(args[index]);
        need_free = true;
    }
    
    jerry_size_t str_size = jerry_string_size(str_val, JERRY_ENCODING_UTF8);
    if (str_size >= buffer_size) {
        str_size = buffer_size - 1;
    }
    
    jerry_string_to_buffer(str_val, JERRY_ENCODING_UTF8, 
                           (jerry_char_t *)buffer, str_size);
    buffer[str_size] = '\0';
    
    if (need_free) {
        jerry_value_free(str_val);
    }
    
    return str_size;
}
