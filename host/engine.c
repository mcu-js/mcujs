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

/* Suggestion state - captured when looking for "did you mean?" */
static char s_suggestion[64];
static bool s_has_suggestion = false;

/* REPL-defined global identifiers (for tab completion and suggestions) */
#define REPL_GLOBAL_MAX 64
#define REPL_GLOBAL_NAME_MAX 32
static char s_repl_globals[REPL_GLOBAL_MAX][REPL_GLOBAL_NAME_MAX];
static uint8_t s_repl_global_count = 0;

/*
 * Compute Levenshtein edit distance between two strings.
 * Uses O(min(m,n)) space by only keeping two rows.
 * Returns distance, or max_dist+1 if distance exceeds max_dist (early exit).
 */
static int levenshtein_distance(const char *s1, const char *s2, int max_dist) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    /* Quick length-based pruning */
    int len_diff = len1 > len2 ? len1 - len2 : len2 - len1;
    if (len_diff > max_dist) {
        return max_dist + 1;
    }
    
    /* Ensure s1 is the shorter string for space efficiency */
    if (len1 > len2) {
        const char *tmp = s1; s1 = s2; s2 = tmp;
        int t = len1; len1 = len2; len2 = t;
    }
    
    /* Use static buffer - limit string length to avoid overflow */
    if (len1 > 63) return max_dist + 1;
    
    int prev[64], curr[64];
    
    /* Initialize first row */
    for (int j = 0; j <= len1; j++) {
        prev[j] = j;
    }
    
    /* Fill in the rest */
    for (int i = 1; i <= len2; i++) {
        curr[0] = i;
        int min_in_row = i;
        
        for (int j = 1; j <= len1; j++) {
            int cost = (s1[j-1] == s2[i-1]) ? 0 : 1;
            
            /* Minimum of insert, delete, replace */
            int insert_cost = curr[j-1] + 1;
            int delete_cost = prev[j] + 1;
            int replace_cost = prev[j-1] + cost;
            
            curr[j] = insert_cost;
            if (delete_cost < curr[j]) curr[j] = delete_cost;
            if (replace_cost < curr[j]) curr[j] = replace_cost;
            
            if (curr[j] < min_in_row) min_in_row = curr[j];
        }
        
        /* Early exit if minimum in row exceeds max_dist */
        if (min_in_row > max_dist) {
            return max_dist + 1;
        }
        
        /* Swap rows */
        for (int j = 0; j <= len1; j++) {
            prev[j] = curr[j];
        }
    }
    
    return prev[len1];
}

/*
 * Context for finding best suggestion
 */
typedef struct {
    const char *target;     /* The misspelled name */
    char *best_match;       /* Buffer for best match */
    size_t best_match_len;  /* Size of buffer */
    int best_distance;      /* Distance of best match */
    int max_distance;       /* Maximum acceptable distance */
} suggestion_ctx_t;

/*
 * Callback to check each candidate against target
 */
static bool suggestion_callback(const char *name, void *user_data) {
    suggestion_ctx_t *ctx = (suggestion_ctx_t *)user_data;
    
    /* Skip if name is same as target */
    if (strcmp(name, ctx->target) == 0) {
        return true;
    }
    
    int dist = levenshtein_distance(ctx->target, name, ctx->max_distance);
    
    if (dist < ctx->best_distance) {
        ctx->best_distance = dist;
        strncpy(ctx->best_match, name, ctx->best_match_len - 1);
        ctx->best_match[ctx->best_match_len - 1] = '\0';
    }
    
    return true;  /* Continue iteration */
}

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
 * Find a suggestion for an undefined identifier by searching globals
 */
static void find_global_suggestion(const char *name) {
    s_has_suggestion = false;
    s_suggestion[0] = '\0';
    
    if (name == NULL || name[0] == '\0') {
        return;
    }
    
    /* Calculate max distance based on name length */
    int name_len = strlen(name);
    int max_dist = (name_len <= 3) ? 1 : (name_len <= 6) ? 2 : 3;
    
    suggestion_ctx_t ctx = {
        .target = name,
        .best_match = s_suggestion,
        .best_match_len = sizeof(s_suggestion),
        .best_distance = max_dist + 1,
        .max_distance = max_dist
    };
    
    /* Search global object properties */
    jerry_value_t global = jerry_current_realm();
    jerry_value_t keys = jerry_object_keys(global);
    
    if (!jerry_value_is_exception(keys)) {
        uint32_t length = jerry_array_length(keys);
        
        for (uint32_t i = 0; i < length; i++) {
            jerry_value_t key = jerry_object_get_index(keys, i);
            if (jerry_value_is_string(key)) {
                char key_buf[64];
                jerry_size_t key_len = jerry_string_size(key, JERRY_ENCODING_UTF8);
                if (key_len < sizeof(key_buf)) {
                    jerry_string_to_buffer(key, JERRY_ENCODING_UTF8,
                                          (jerry_char_t *)key_buf, sizeof(key_buf));
                    key_buf[key_len] = '\0';
                    suggestion_callback(key_buf, &ctx);
                }
            }
            jerry_value_free(key);
        }
        jerry_value_free(keys);
    }
    jerry_value_free(global);
    
    /* Also search REPL-defined globals */
    for (uint8_t i = 0; i < s_repl_global_count; i++) {
        suggestion_callback(s_repl_globals[i], &ctx);
    }
    
    if (ctx.best_distance <= max_dist) {
        s_has_suggestion = true;
    }
}

/*
 * Find a suggestion for an undefined property on an object
 */
static void find_property_suggestion(jerry_value_t obj, const char *name) {
    s_has_suggestion = false;
    s_suggestion[0] = '\0';
    
    if (name == NULL || name[0] == '\0') {
        return;
    }
    
    int name_len = strlen(name);
    int max_dist = (name_len <= 3) ? 1 : (name_len <= 6) ? 2 : 3;
    
    suggestion_ctx_t ctx = {
        .target = name,
        .best_match = s_suggestion,
        .best_match_len = sizeof(s_suggestion),
        .best_distance = max_dist + 1,
        .max_distance = max_dist
    };
    
    /* Use JERRY_PROPERTY_FILTER_ALL to include non-enumerable properties (like Math.floor) */
    jerry_value_t keys = jerry_object_property_names(obj, JERRY_PROPERTY_FILTER_ALL);
    
    if (!jerry_value_is_exception(keys)) {
        uint32_t length = jerry_array_length(keys);
        
        for (uint32_t i = 0; i < length; i++) {
            jerry_value_t key = jerry_object_get_index(keys, i);
            if (jerry_value_is_string(key)) {
                char key_buf[64];
                jerry_size_t key_len = jerry_string_size(key, JERRY_ENCODING_UTF8);
                if (key_len < sizeof(key_buf)) {
                    jerry_string_to_buffer(key, JERRY_ENCODING_UTF8,
                                          (jerry_char_t *)key_buf, sizeof(key_buf));
                    key_buf[key_len] = '\0';
                    suggestion_callback(key_buf, &ctx);
                }
            }
            jerry_value_free(key);
        }
        jerry_value_free(keys);
    }
    
    if (ctx.best_distance <= max_dist) {
        s_has_suggestion = true;
    }
}

/*
 * Extract identifier from error message patterns
 * Returns true if found, with name copied to buffer
 */
static bool extract_undefined_name(const char *msg, char *name, size_t name_len) {
    /* Pattern: "X is not defined" */
    const char *suffix = " is not defined";
    const char *pos = strstr(msg, suffix);
    if (pos != NULL && pos > msg) {
        size_t len = pos - msg;
        if (len < name_len) {
            memcpy(name, msg, len);
            name[len] = '\0';
            return true;
        }
    }
    return false;
}

static bool extract_not_function_name(const char *msg, char *obj_name, size_t obj_len,
                                       char *prop_name, size_t prop_len) {
    /* Pattern: "X.Y is not a function" or "X is not a function" */
    const char *suffix = " is not a function";
    const char *pos = strstr(msg, suffix);
    if (pos != NULL && pos > msg) {
        size_t len = pos - msg;
        char temp[128];
        if (len < sizeof(temp)) {
            memcpy(temp, msg, len);
            temp[len] = '\0';
            
            /* Look for dot */
            char *dot = strrchr(temp, '.');
            if (dot != NULL) {
                *dot = '\0';
                strncpy(obj_name, temp, obj_len - 1);
                obj_name[obj_len - 1] = '\0';
                strncpy(prop_name, dot + 1, prop_len - 1);
                prop_name[prop_len - 1] = '\0';
                return true;
            }
        }
    }
    return false;
}

/*
 * VM throw callback - captures backtrace and finds suggestions at throw time
 */
static void throw_callback(const jerry_value_t exception_value, void *user_p) {
    (void)user_p;
    
    /* Clear previous suggestion */
    s_has_suggestion = false;
    s_suggestion[0] = '\0';
    
    /* Capture backtrace into static buffer */
    backtrace_context_t ctx = {
        .buf = s_backtrace,
        .buf_len = sizeof(s_backtrace),
        .offset = 0,
        .frame_count = 0
    };
    
    jerry_backtrace_capture(backtrace_callback, &ctx);
    s_backtrace_len = ctx.offset;
    
    /* Try to get error message and find suggestions */
    jerry_value_t error_obj = jerry_exception_value(exception_value, false);
    jerry_value_t msg_prop = jerry_string_sz("message");
    jerry_value_t msg_value = jerry_object_get(error_obj, msg_prop);
    jerry_value_free(msg_prop);
    
    if (!jerry_value_is_exception(msg_value) && jerry_value_is_string(msg_value)) {
        char msg[256];
        jerry_size_t msg_len = jerry_string_size(msg_value, JERRY_ENCODING_UTF8);
        if (msg_len < sizeof(msg)) {
            jerry_string_to_buffer(msg_value, JERRY_ENCODING_UTF8,
                                  (jerry_char_t *)msg, sizeof(msg));
            msg[msg_len] = '\0';
            
            /* Check for "X is not defined" (ReferenceError) */
            char name[64];
            if (extract_undefined_name(msg, name, sizeof(name))) {
                find_global_suggestion(name);
            }
            /* Check for "X.Y is not a function" (TypeError) */
            else {
                char obj_name[64], prop_name[64];
                if (extract_not_function_name(msg, obj_name, sizeof(obj_name),
                                              prop_name, sizeof(prop_name))) {
                    /* Try to get the object and search its properties */
                    jerry_value_t parsed = jerry_parse((const jerry_char_t *)obj_name,
                                                       strlen(obj_name), NULL);
                    if (!jerry_value_is_exception(parsed)) {
                        jerry_value_t obj = jerry_run(parsed);
                        jerry_value_free(parsed);
                        if (!jerry_value_is_exception(obj)) {
                            find_property_suggestion(obj, prop_name);
                            jerry_value_free(obj);
                        }
                    } else {
                        jerry_value_free(parsed);
                    }
                }
            }
        }
    }
    
    jerry_value_free(msg_value);
    jerry_value_free(error_obj);
}

/*
 * Store error message from exception value, append suggestion and backtrace
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
    
    /* Append "Did you mean?" suggestion if available */
    if (s_has_suggestion && s_suggestion[0] != '\0') {
        int written = snprintf(s_error_message + msg_len, sizeof(s_error_message) - msg_len,
                              ". Did you mean '%s'?", s_suggestion);
        if (written > 0) {
            msg_len += written;
        }
        s_has_suggestion = false;
    }
    
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

/*
 * Try to find a suggestion for a method call TypeError
 * Parses source code to find "obj.method(" patterns and suggest similar methods
 */
bool js_engine_suggest_method(const char *source, char *suggestion, size_t suggestion_len) {
    if (!s_initialized || source == NULL || suggestion == NULL || suggestion_len == 0) {
        return false;
    }
    
    suggestion[0] = '\0';
    
    /* Find pattern: identifier.identifier( 
     * We look for the last occurrence of this pattern */
    const char *best_obj_start = NULL;
    const char *best_obj_end = NULL;
    const char *best_prop_start = NULL;
    const char *best_prop_end = NULL;
    
    const char *p = source;
    while (*p) {
        /* Skip whitespace */
        while (*p && isspace(*p)) p++;
        
        /* Look for identifier */
        if (isalpha(*p) || *p == '_' || *p == '$') {
            const char *id_start = p;
            while (*p && (isalnum(*p) || *p == '_' || *p == '$')) p++;
            const char *id_end = p;
            
            /* Check for dot */
            while (*p && isspace(*p)) p++;
            if (*p == '.') {
                p++;
                while (*p && isspace(*p)) p++;
                
                /* Look for property name */
                if (isalpha(*p) || *p == '_' || *p == '$') {
                    const char *prop_start = p;
                    while (*p && (isalnum(*p) || *p == '_' || *p == '$')) p++;
                    const char *prop_end = p;
                    
                    /* Check for opening paren (function call) */
                    while (*p && isspace(*p)) p++;
                    if (*p == '(') {
                        /* Found a method call pattern */
                        best_obj_start = id_start;
                        best_obj_end = id_end;
                        best_prop_start = prop_start;
                        best_prop_end = prop_end;
                    }
                }
            }
        } else {
            p++;
        }
    }
    
    if (best_obj_start == NULL || best_prop_start == NULL) {
        return false;
    }
    
    /* Extract object name and property name */
    size_t obj_len = best_obj_end - best_obj_start;
    size_t prop_len = best_prop_end - best_prop_start;
    
    if (obj_len >= 64 || prop_len >= 64) {
        return false;
    }
    
    char obj_name[64];
    char prop_name[64];
    memcpy(obj_name, best_obj_start, obj_len);
    obj_name[obj_len] = '\0';
    memcpy(prop_name, best_prop_start, prop_len);
    prop_name[prop_len] = '\0';
    
    /* Try to get the object */
    jerry_value_t parsed = jerry_parse((const jerry_char_t *)obj_name, obj_len, NULL);
    if (jerry_value_is_exception(parsed)) {
        jerry_value_free(parsed);
        return false;
    }
    
    jerry_value_t obj = jerry_run(parsed);
    jerry_value_free(parsed);
    
    if (jerry_value_is_exception(obj)) {
        jerry_value_free(obj);
        return false;
    }
    
    /* Find similar property name */
    find_property_suggestion(obj, prop_name);
    jerry_value_free(obj);
    
    if (s_has_suggestion && s_suggestion[0] != '\0') {
        /* Format as "obj.suggestion" */
        snprintf(suggestion, suggestion_len, "%s.%s", obj_name, s_suggestion);
        s_has_suggestion = false;
        return true;
    }
    
    return false;
}
