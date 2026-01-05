/*
 * mcujs - Console Bindings
 * 
 * Implements: console.log(), console.warn(), console.error()
 */

#include "bindings.h"
#include "jerryscript.h"
#include "usb_cdc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External helpers from bindings.c */
extern void js_set_function(jerry_value_t object, const char *name, 
                            jerry_external_handler_t handler);
extern void js_register_global(const char *name, jerry_value_t object);
extern size_t js_get_string_arg(const jerry_value_t args[], jerry_length_t argc,
                                jerry_length_t index, char *buffer, size_t buffer_size);

/*
 * Print arguments to console
 */
static void print_args(const jerry_value_t args[], jerry_length_t argc, 
                       const char *prefix) {
    char buffer[256];
    
    if (prefix != NULL) {
        usb_cdc_write(prefix, strlen(prefix));
    }
    
    for (jerry_length_t i = 0; i < argc; i++) {
        if (i > 0) {
            usb_cdc_write(" ", 1);
        }
        
        /* Convert value to string */
        jerry_value_t str_val = jerry_value_to_string(args[i]);
        
        if (!jerry_value_is_exception(str_val)) {
            jerry_size_t size = jerry_string_size(str_val, JERRY_ENCODING_UTF8);
            
            if (size < sizeof(buffer)) {
                jerry_string_to_buffer(str_val, JERRY_ENCODING_UTF8,
                                       (jerry_char_t *)buffer, size);
                buffer[size] = '\0';
                usb_cdc_write(buffer, size);
            } else {
                /* Large string - print in chunks */
                jerry_char_t *large_buf = malloc(size + 1);
                if (large_buf != NULL) {
                    jerry_string_to_buffer(str_val, JERRY_ENCODING_UTF8, large_buf, size);
                    large_buf[size] = '\0';
                    usb_cdc_write((const char *)large_buf, size);
                    free(large_buf);
                }
            }
        }
        
        jerry_value_free(str_val);
    }
    
    usb_cdc_write("\r\n", 2);
}

/*
 * console.log()
 */
static jerry_value_t console_log_handler(const jerry_call_info_t *call_info_p,
                                          const jerry_value_t args[],
                                          const jerry_length_t argc) {
    (void)call_info_p;
    print_args(args, argc, NULL);
    return jerry_undefined();
}

/*
 * console.warn()
 */
static jerry_value_t console_warn_handler(const jerry_call_info_t *call_info_p,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info_p;
    print_args(args, argc, "[WARN] ");
    return jerry_undefined();
}

/*
 * console.error()
 */
static jerry_value_t console_error_handler(const jerry_call_info_t *call_info_p,
                                            const jerry_value_t args[],
                                            const jerry_length_t argc) {
    (void)call_info_p;
    print_args(args, argc, "[ERROR] ");
    return jerry_undefined();
}

/*
 * Register console bindings
 */
void js_bind_console(void) {
    jerry_value_t console = jerry_object();
    
    js_set_function(console, "log", console_log_handler);
    js_set_function(console, "warn", console_warn_handler);
    js_set_function(console, "error", console_error_handler);
    
    js_register_global("console", console);
    jerry_value_free(console);
}
