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

static jerry_value_t js_freeze_object(jerry_value_t object) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t object_constructor = jerry_object_get_sz(global, "Object");
    jerry_value_free(global);
    if (jerry_value_is_exception(object_constructor)) return object_constructor;

    jerry_value_t freeze = jerry_object_get_sz(object_constructor, "freeze");
    if (jerry_value_is_exception(freeze)) {
        jerry_value_free(object_constructor);
        return freeze;
    }

    jerry_value_t result = jerry_call(freeze, object_constructor, &object, 1);
    jerry_value_free(freeze);
    jerry_value_free(object_constructor);
    return result;
}

jerry_value_t js_deep_freeze(jerry_value_t value) {
    if (!jerry_value_is_object(value)) return jerry_undefined();

    jerry_value_t keys = jerry_object_keys(value);
    if (jerry_value_is_exception(keys)) return keys;

    jerry_length_t length = jerry_array_length(keys);
    for (jerry_length_t i = 0; i < length; i++) {
        jerry_value_t key = jerry_object_get_index(keys, i);
        if (jerry_value_is_exception(key)) {
            jerry_value_free(keys);
            return key;
        }

        jerry_value_t child = jerry_object_get(value, key);
        jerry_value_free(key);
        if (jerry_value_is_exception(child)) {
            jerry_value_free(keys);
            return child;
        }

        jerry_value_t frozen = js_deep_freeze(child);
        jerry_value_free(child);
        if (jerry_value_is_exception(frozen)) {
            jerry_value_free(keys);
            return frozen;
        }
        jerry_value_free(frozen);
    }
    jerry_value_free(keys);

    return js_freeze_object(value);
}

jerry_value_t js_define_immutable_property(jerry_value_t object,
                                           const char *name,
                                           jerry_value_t value) {
    jerry_value_t key = jerry_string_sz(name);
    jerry_property_descriptor_t descriptor = jerry_property_descriptor();
    descriptor.flags |= JERRY_PROP_IS_VALUE_DEFINED |
                        JERRY_PROP_IS_WRITABLE_DEFINED |
                        JERRY_PROP_IS_ENUMERABLE_DEFINED |
                        JERRY_PROP_IS_ENUMERABLE |
                        JERRY_PROP_IS_CONFIGURABLE_DEFINED |
                        JERRY_PROP_SHOULD_THROW;
    descriptor.value = jerry_value_copy(value);

    jerry_value_t result = jerry_object_define_own_prop(object, key, &descriptor);
    jerry_property_descriptor_free(&descriptor);
    jerry_value_free(key);
    return result;
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
