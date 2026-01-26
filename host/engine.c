/*
 * mcujs - JavaScript Engine Implementation
 * 
 * JerryScript integration for the mcujs runtime
 */

#include "engine.h"
#include "module_loader.h"
#include "bindings/bindings.h"

#include "jerryscript.h"

#include <stdio.h>
#include <string.h>

/* Engine state */
static bool s_initialized = false;
static char s_error_message[512];  /* Increased for stack traces */
static char s_backtrace[384];      /* Captured at throw time */
static size_t s_backtrace_len = 0;

/*
 * Convert JerryScript value to string
 */
static size_t value_to_string(jerry_value_t value, char *buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) {
        return 0;
    }
    
    jerry_value_t str_value = jerry_value_to_string(value);
    if (jerry_value_is_exception(str_value)) {
        jerry_value_free(str_value);
        return 0;
    }
    
    jerry_size_t str_size = jerry_string_size(str_value, JERRY_ENCODING_UTF8);
    if (str_size >= buf_len) {
        str_size = buf_len - 1;
    }
    
    jerry_string_to_buffer(str_value, JERRY_ENCODING_UTF8, (jerry_char_t *)buf, str_size);
    buf[str_size] = '\0';
    
    jerry_value_free(str_value);
    return str_size;
}

/*
 * Backtrace callback to build stack trace string
 */
typedef struct {
    char *buf;
    size_t buf_len;
    size_t offset;
    int frame_count;
} backtrace_context_t;

static bool backtrace_callback(jerry_frame_t *frame_p, void *user_p) {
    backtrace_context_t *ctx = (backtrace_context_t *)user_p;
    
    /* Limit stack depth */
    if (ctx->frame_count >= 5) {
        return false;  /* Stop iteration */
    }
    
    /* Get frame type */
    jerry_frame_type_t frame_type = jerry_frame_type(frame_p);
    if (frame_type != JERRY_BACKTRACE_FRAME_JS) {
        return true;  /* Skip non-JS frames */
    }
    
    /* Get location info */
    const jerry_frame_location_t *location = jerry_frame_location(frame_p);
    
    /* Get function name if available */
    const jerry_value_t *callee = jerry_frame_callee(frame_p);
    char func_name[32] = "<anonymous>";
    
    if (callee != NULL && jerry_value_is_function(*callee)) {
        jerry_value_t name_prop = jerry_string_sz("name");
        jerry_value_t name_val = jerry_object_get(*callee, name_prop);
        jerry_value_free(name_prop);
        
        if (jerry_value_is_string(name_val)) {
            jerry_size_t len = jerry_string_size(name_val, JERRY_ENCODING_UTF8);
            if (len > 0 && len < sizeof(func_name)) {
                jerry_string_to_buffer(name_val, JERRY_ENCODING_UTF8, 
                                       (jerry_char_t *)func_name, len);
                func_name[len] = '\0';
            }
        }
        jerry_value_free(name_val);
    }
    
    /* Format frame info */
    int written;
    if (location != NULL && jerry_value_is_string(location->source_name)) {
        /* Get source name */
        char source[64] = "<input>";
        jerry_size_t src_len = jerry_string_size(location->source_name, JERRY_ENCODING_UTF8);
        if (src_len > 0 && src_len < sizeof(source)) {
            jerry_string_to_buffer(location->source_name, JERRY_ENCODING_UTF8,
                                   (jerry_char_t *)source, src_len);
            source[src_len] = '\0';
        }
        
        written = snprintf(ctx->buf + ctx->offset, ctx->buf_len - ctx->offset,
                          "\r\n    at %s (%s:%u:%u)",
                          func_name, source,
                          (unsigned)location->line, (unsigned)location->column);
    } else if (location != NULL) {
        written = snprintf(ctx->buf + ctx->offset, ctx->buf_len - ctx->offset,
                          "\r\n    at %s (line %u)",
                          func_name, (unsigned)location->line);
    } else {
        written = snprintf(ctx->buf + ctx->offset, ctx->buf_len - ctx->offset,
                          "\r\n    at %s", func_name);
    }
    
    if (written > 0 && ctx->offset + written < ctx->buf_len) {
        ctx->offset += written;
    }
    
    ctx->frame_count++;
    return true;  /* Continue iteration */
}

/*
 * VM throw callback - captures backtrace at throw time when stack is still intact
 */
static void throw_callback(const jerry_value_t exception_value, void *user_p) {
    (void)exception_value;
    (void)user_p;
    
    /* Capture backtrace into static buffer */
    backtrace_context_t ctx = {
        .buf = s_backtrace,
        .buf_len = sizeof(s_backtrace),
        .offset = 0,
        .frame_count = 0
    };
    
    jerry_backtrace_capture(backtrace_callback, &ctx);
    s_backtrace_len = ctx.offset;
}

/*
 * Store error message from exception value, append captured backtrace
 */
static void store_error(jerry_value_t error_value) {
    jerry_value_t error_obj = jerry_exception_value(error_value, false);
    
    /* Try to get error message property */
    jerry_value_t msg_prop = jerry_string_sz("message");
    jerry_value_t msg_value = jerry_object_get(error_obj, msg_prop);
    jerry_value_free(msg_prop);
    
    size_t msg_len = 0;
    if (!jerry_value_is_exception(msg_value) && jerry_value_is_string(msg_value)) {
        msg_len = value_to_string(msg_value, s_error_message, sizeof(s_error_message));
    } else {
        /* Fall back to converting entire error object */
        msg_len = value_to_string(error_obj, s_error_message, sizeof(s_error_message));
    }
    
    jerry_value_free(msg_value);
    jerry_value_free(error_obj);
    
    /* Append captured backtrace if available */
    if (s_backtrace_len > 0 && msg_len + s_backtrace_len < sizeof(s_error_message)) {
        memcpy(s_error_message + msg_len, s_backtrace, s_backtrace_len);
        s_error_message[msg_len + s_backtrace_len] = '\0';
    }
    
    /* Clear captured backtrace for next error */
    s_backtrace_len = 0;
}

js_result_t js_engine_init(void) {
    if (s_initialized) {
        return JS_OK;
    }
    
    /* Clear error state */
    s_error_message[0] = '\0';
    s_backtrace[0] = '\0';
    s_backtrace_len = 0;
    
    /* Initialize JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    
    /* Register throw callback to capture backtraces at throw time */
    jerry_on_throw(throw_callback, NULL);
    
    /* Register native bindings */
    js_register_bindings();
    
    /* Initialize module loader */
    js_module_loader_init();
    
    s_initialized = true;
    return JS_OK;
}

void js_engine_cleanup(void) {
    if (!s_initialized) {
        return;
    }
    
    js_module_loader_cleanup();
    jerry_cleanup();
    s_initialized = false;
}

js_result_t js_engine_exec(const char *code, size_t code_len, 
                           char *result_buf, size_t result_buf_len) {
    return js_engine_exec_named(code, code_len, "<input>", result_buf, result_buf_len);
}

js_result_t js_engine_exec_named(const char *code, size_t code_len,
                                  const char *source_name,
                                  char *result_buf, size_t result_buf_len) {
    if (!s_initialized) {
        snprintf(s_error_message, sizeof(s_error_message), "Engine not initialized");
        return JS_ERROR_INIT;
    }
    
    if (code == NULL || code_len == 0) {
        return JS_OK;
    }
    
    /* Set up parse options with source name */
    jerry_parse_options_t parse_options;
    memset(&parse_options, 0, sizeof(parse_options));
    parse_options.options = JERRY_PARSE_HAS_SOURCE_NAME;
    parse_options.source_name = jerry_string_sz(source_name ? source_name : "<input>");
    
    /* Parse and execute */
    jerry_value_t parsed = jerry_parse((const jerry_char_t *)code, code_len, &parse_options);
    
    jerry_value_free(parse_options.source_name);
    
    if (jerry_value_is_exception(parsed)) {
        store_error(parsed);
        jerry_value_free(parsed);
        return JS_ERROR_PARSE;
    }
    
    jerry_value_t result = jerry_run(parsed);
    jerry_value_free(parsed);
    
    if (jerry_value_is_exception(result)) {
        store_error(result);
        jerry_value_free(result);
        return JS_ERROR_EXEC;
    }
    
    /* Convert result to string if buffer provided */
    if (result_buf != NULL && result_buf_len > 0) {
        if (jerry_value_is_undefined(result)) {
            snprintf(result_buf, result_buf_len, "undefined");
        } else if (jerry_value_is_null(result)) {
            snprintf(result_buf, result_buf_len, "null");
        } else {
            value_to_string(result, result_buf, result_buf_len);
        }
    }
    
    jerry_value_free(result);
    return JS_OK;
}

js_result_t js_engine_exec_file(const char *filename) {
    if (!s_initialized) {
        snprintf(s_error_message, sizeof(s_error_message), "Engine not initialized");
        return JS_ERROR_INIT;
    }
    
    /* Read file content */
    char *content = NULL;
    size_t content_len = 0;
    
    js_result_t read_result = js_module_read_file(filename, &content, &content_len);
    if (read_result != JS_OK) {
        return read_result;
    }
    
    /* Execute the code with filename for stack traces */
    js_result_t exec_result = js_engine_exec_named(content, content_len, filename, NULL, 0);
    
    /* Free file content */
    js_module_free_file(content);
    
    return exec_result;
}

size_t js_engine_get_error(char *buf, size_t buf_len) {
    if (buf == NULL || buf_len == 0) {
        return strlen(s_error_message);
    }
    
    size_t len = strlen(s_error_message);
    if (len >= buf_len) {
        len = buf_len - 1;
    }
    
    memcpy(buf, s_error_message, len);
    buf[len] = '\0';
    return len;
}

void js_engine_gc(void) {
    if (s_initialized) {
        jerry_heap_gc(JERRY_GC_PRESSURE_HIGH);
    }
}

void js_engine_get_memory_stats(js_memory_stats_t *stats) {
    if (stats == NULL) {
        return;
    }
    
    memset(stats, 0, sizeof(*stats));
    
    if (s_initialized) {
        jerry_heap_stats_t jerry_stats;
        if (jerry_heap_stats(&jerry_stats)) {
            stats->heap_size = jerry_stats.size;
            stats->heap_used = jerry_stats.allocated_bytes;
            stats->heap_peak = jerry_stats.peak_allocated_bytes;
        }
    }
}

bool js_engine_is_initialized(void) {
    return s_initialized;
}

void js_register_bindings(void) {
    /* Register all native JavaScript APIs */
    js_bind_console();
    js_bind_timers();
    js_bind_board();
    js_bind_gpio();
    js_bind_pwm();
    js_bind_i2c();
    js_bind_spi();
    js_bind_adc();
    js_bind_neopixel();
    js_bind_process();
    js_bind_require();
    js_bind_graphics();
}

bool js_engine_process_timers(void) {
    return js_timers_process();
}

#define REPL_GLOBAL_MAX 64
#define REPL_GLOBAL_NAME_MAX 32

static char s_repl_globals[REPL_GLOBAL_MAX][REPL_GLOBAL_NAME_MAX];
static uint8_t s_repl_global_count = 0;

void js_engine_register_global_identifier(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return;
    }

    size_t name_len = strlen(name);
    if (name_len >= REPL_GLOBAL_NAME_MAX) {
        return;
    }

    for (uint8_t i = 0; i < s_repl_global_count; i++) {
        if (strcmp(s_repl_globals[i], name) == 0) {
            return;
        }
    }

    if (s_repl_global_count >= REPL_GLOBAL_MAX) {
        return;
    }

    strncpy(s_repl_globals[s_repl_global_count], name, REPL_GLOBAL_NAME_MAX - 1);
    s_repl_globals[s_repl_global_count][REPL_GLOBAL_NAME_MAX - 1] = '\0';
    s_repl_global_count++;
}

/*
 * Get completions for tab completion
 */
int js_engine_get_completions(const char *partial, js_completion_callback_t callback, void *user_data) {
    if (!s_initialized || partial == NULL || callback == NULL) {
        return 0;
    }
    
    int match_count = 0;
    size_t partial_len = strlen(partial);
    
    /* Find the last dot to determine if this is a property access */
    const char *dot = strrchr(partial, '.');
    
    jerry_value_t target_obj;
    const char *prefix;
    size_t prefix_len;
    
    if (dot != NULL) {
        /* Property access: evaluate the object part */
        size_t obj_len = dot - partial;
        char obj_str[128];
        if (obj_len >= sizeof(obj_str)) {
            return 0;
        }
        memcpy(obj_str, partial, obj_len);
        obj_str[obj_len] = '\0';
        
        /* Parse and evaluate the object expression */
        jerry_value_t parsed = jerry_parse((const jerry_char_t *)obj_str, obj_len, NULL);
        if (jerry_value_is_exception(parsed)) {
            jerry_value_free(parsed);
            return 0;
        }
        
        target_obj = jerry_run(parsed);
        jerry_value_free(parsed);
        
        if (jerry_value_is_exception(target_obj)) {
            jerry_value_free(target_obj);
            return 0;
        }
        
        prefix = dot + 1;
        prefix_len = strlen(prefix);
    } else {
        /* Global completion */
        target_obj = jerry_current_realm();
        prefix = partial;
        prefix_len = partial_len;
    }
    
    /* Get object keys */
    jerry_value_t keys = jerry_object_keys(target_obj);
    if (jerry_value_is_exception(keys)) {
        if (dot != NULL) {
            jerry_value_free(target_obj);
        }
        jerry_value_free(keys);
        return 0;
    }
    
    uint32_t length = jerry_array_length(keys);
    
    for (uint32_t i = 0; i < length; i++) {
        jerry_value_t key = jerry_object_get_index(keys, i);
        if (jerry_value_is_string(key)) {
            char name_buf[64];
            jerry_size_t name_len = jerry_string_size(key, JERRY_ENCODING_UTF8);
            if (name_len < sizeof(name_buf)) {
                jerry_string_to_buffer(key, JERRY_ENCODING_UTF8, 
                                       (jerry_char_t *)name_buf, sizeof(name_buf));
                name_buf[name_len] = '\0';
                
                /* Check if name starts with prefix */
                if (prefix_len == 0 || strncmp(name_buf, prefix, prefix_len) == 0) {
                    match_count++;
                    if (!callback(name_buf, user_data)) {
                        jerry_value_free(key);
                        break;
                    }
                }
            }
        }
        jerry_value_free(key);
    }
    
    jerry_value_free(keys);

    if (dot == NULL && s_repl_global_count > 0) {
        for (uint8_t i = 0; i < s_repl_global_count; i++) {
            const char *name = s_repl_globals[i];
            if (prefix_len > 0 && strncmp(name, prefix, prefix_len) != 0) {
                continue;
            }

            match_count++;
            if (!callback(name, user_data)) {
                break;
            }
        }
    }

    if (dot != NULL) {
        jerry_value_free(target_obj);
    }
    
    return match_count;
}
