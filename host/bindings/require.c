/*
 * mcujs - CommonJS Module System (require/exports)
 * 
 * Implements Node.js-style require() for loading JavaScript modules.
 * 
 * Usage:
 *   // math.js
 *   exports.add = function(a, b) { return a + b; };
 *   exports.PI = 3.14159;
 *   
 *   // or using module.exports
 *   module.exports = { add: (a, b) => a + b };
 *   
 *   // index.js
 *   const math = require('./math');
 *   console.log(math.add(1, 2));
 * 
 *   // JSON files are also supported
 *   const config = require('./config.json');
 *   console.log(config.setting);
 */

#include "bindings.h"
#include "../module_loader.h"
#include "../runtime_features.h"
#include "../runtime_registry.h"
#include "jerryscript.h"
#include "fs.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Compute Levenshtein edit distance (simplified, for short strings)
 */
static int levenshtein(const char *s1, const char *s2, int max_dist) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    int len_diff = len1 > len2 ? len1 - len2 : len2 - len1;
    if (len_diff > max_dist) return max_dist + 1;
    
    if (len1 > len2) {
        const char *tmp = s1; s1 = s2; s2 = tmp;
        int t = len1; len1 = len2; len2 = t;
    }
    
    if (len1 > 31) return max_dist + 1;
    
    int prev[32], curr[32];
    for (int j = 0; j <= len1; j++) prev[j] = j;
    
    for (int i = 1; i <= len2; i++) {
        curr[0] = i;
        for (int j = 1; j <= len1; j++) {
            int cost = (s1[j-1] == s2[i-1]) ? 0 : 1;
            int ins = curr[j-1] + 1;
            int del = prev[j] + 1;
            int rep = prev[j-1] + cost;
            curr[j] = ins < del ? ins : del;
            if (rep < curr[j]) curr[j] = rep;
        }
        for (int j = 0; j <= len1; j++) prev[j] = curr[j];
    }
    return prev[len1];
}

/* Maximum path length for modules */
#define MAX_MODULE_PATH 128

/* Maximum number of cached modules */
#define MAX_MODULES 16

/* Maximum module size for static wrapper buffer (8KB source + wrapper overhead)
 * Using a static buffer avoids malloc() during require(), which is critical
 * when memory is constrained (e.g., DVI running with large framebuffers).
 * Modules larger than this will fall back to malloc(). */
#define MAX_STATIC_MODULE_SIZE 8192
#define WRAPPER_OVERHEAD 128  /* Space for wrapper prefix/suffix */
static char s_module_wrapper_buf[MAX_STATIC_MODULE_SIZE + WRAPPER_OVERHEAD];

static void set_property_value(jerry_value_t object, jerry_value_t property,
                               jerry_value_t value) {
    jerry_value_t result = jerry_object_set(object, property, value);
    jerry_value_free(result);
}


/* Module cache entry */
typedef struct {
    char path[MAX_MODULE_PATH];
    jerry_value_t exports;
    bool loaded;
} cached_module_t;

/* Module cache */
static cached_module_t s_module_cache[MAX_MODULES];
static size_t s_cache_count = 0;

typedef struct {
    char from_path[MAX_MODULE_PATH];
} require_context_t;

static jerry_value_t require_handler(const jerry_call_info_t *call_info,
                                     const jerry_value_t args[],
                                     const jerry_length_t argc);

static void require_context_free(void *native_p, jerry_object_native_info_t *info_p) {
    (void)info_p;
    free(native_p);
}

static const jerry_object_native_info_t s_require_context_info = {
    .free_cb = require_context_free,
};

static jerry_value_t create_require_function(const char *from_path) {
    jerry_value_t function = jerry_function_external(require_handler);
    if (from_path == NULL || from_path[0] == '\0') {
        return function;
    }

    require_context_t *context = malloc(sizeof(*context));
    if (context == NULL) {
        jerry_value_free(function);
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Out of memory creating module require");
    }
    strncpy(context->from_path, from_path, sizeof(context->from_path) - 1);
    context->from_path[sizeof(context->from_path) - 1] = '\0';
    jerry_object_set_native_ptr(function, &s_require_context_info, context);
    return function;
}

/*
 * Read a module file, using a static buffer for small files to avoid malloc().
 */
static js_result_t read_module_file(const char *filename,
                                    char **content,
                                    size_t *content_len,
                                    bool *used_static) {
    if (filename == NULL || content == NULL || content_len == NULL || used_static == NULL) {
        return JS_ERROR_FILE_READ;
    }
    
    fs_file_t file;
    fs_result_t result = fs_open(&file, filename, FS_MODE_READ);
    if (result != FS_OK) {
        return JS_ERROR_FILE_NOT_FOUND;
    }
    
    size_t file_size = 0;
    result = fs_size(&file, &file_size);
    if (result != FS_OK) {
        fs_close(&file);
        return JS_ERROR_FILE_READ;
    }
    
    if (file_size <= MAX_STATIC_MODULE_SIZE) {
        size_t bytes_read = 0;
        result = fs_read(&file, s_module_wrapper_buf, file_size, &bytes_read);
        fs_close(&file);
        if (result != FS_OK || bytes_read != file_size) {
            return JS_ERROR_FILE_READ;
        }
        s_module_wrapper_buf[file_size] = '\0';
        *content = s_module_wrapper_buf;
        *content_len = file_size;
        *used_static = true;
        return JS_OK;
    }
    
    *content = (char *)malloc(file_size + 1);
    if (*content == NULL) {
        fs_close(&file);
        return JS_ERROR_MEMORY;
    }
    
    size_t bytes_read = 0;
    result = fs_read(&file, *content, file_size, &bytes_read);
    fs_close(&file);
    
    if (result != FS_OK || bytes_read != file_size) {
        free(*content);
        *content = NULL;
        return JS_ERROR_FILE_READ;
    }
    
    (*content)[file_size] = '\0';
    *content_len = file_size;
    *used_static = false;
    return JS_OK;
}

/*
 * Find a module in the cache
 */
static cached_module_t* find_cached_module(const char *path) {
    for (size_t i = 0; i < s_cache_count; i++) {
        if (strcmp(s_module_cache[i].path, path) == 0 && s_module_cache[i].loaded) {
            return &s_module_cache[i];
        }
    }
    return NULL;
}

/*
 * Add a module to the cache
 */
static cached_module_t* cache_module(const char *path, jerry_value_t exports) {
    if (s_cache_count >= MAX_MODULES) {
        return NULL;
    }
    
    cached_module_t *entry = &s_module_cache[s_cache_count++];
    strncpy(entry->path, path, MAX_MODULE_PATH - 1);
    entry->path[MAX_MODULE_PATH - 1] = '\0';
    entry->exports = jerry_value_copy(exports);
    entry->loaded = true;
    
    return entry;
}

/*
 * Check if a string ends with a given suffix
 */
static bool ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static bool join_path(char *output, size_t output_len,
                      const char *prefix, const char *suffix) {
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    if (prefix_len + suffix_len >= output_len) {
        return false;
    }
    memcpy(output, prefix, prefix_len);
    memcpy(output + prefix_len, suffix, suffix_len + 1);
    return true;
}

/*
 * Resolve a module path
 * Handles: ./relative, ../parent, /absolute, bare (searches /lib/)
 * Tries .js extension first, then .json if not found
 */
static bool resolve_module_path(const char *specifier, const char *from_path, 
                                 char *resolved, size_t resolved_len) {
    if (specifier == NULL || resolved == NULL || resolved_len == 0) {
        return false;
    }
    
    /* Absolute path */
    if (specifier[0] == '/') {
        if (!join_path(resolved, resolved_len, "", specifier)) {
            return false;
        }
    }
    /* Relative path: ./ or ../ */
    else if (specifier[0] == '.') {
        /* Get directory of current module */
        char base_dir[MAX_MODULE_PATH] = "/";
        
        if (from_path != NULL && from_path[0] != '\0') {
            strncpy(base_dir, from_path, sizeof(base_dir) - 1);
            base_dir[sizeof(base_dir) - 1] = '\0';
            
            /* Find last slash and truncate */
            char *last_slash = strrchr(base_dir, '/');
            if (last_slash != NULL) {
                *(last_slash + 1) = '\0';
            }
        }
        
        /* Handle ./ prefix */
        const char *rel_path = specifier;
        if (specifier[0] == '.' && specifier[1] == '/') {
            rel_path = specifier + 2;
        }
        /* Handle ../ prefix (simplified - just go up one level) */
        else if (specifier[0] == '.' && specifier[1] == '.' && specifier[2] == '/') {
            /* Remove trailing slash, then remove one path component */
            size_t len = strlen(base_dir);
            if (len > 1 && base_dir[len-1] == '/') {
                base_dir[len-1] = '\0';
            }
            char *slash = strrchr(base_dir, '/');
            if (slash != NULL) {
                *(slash + 1) = '\0';
            }
            rel_path = specifier + 3;
        }
        
        if (!join_path(resolved, resolved_len, base_dir, rel_path)) {
            return false;
        }
    }
    /* Bare specifier - search in /lib/ */
    else {
        if (!join_path(resolved, resolved_len, "/lib/", specifier)) {
            return false;
        }
    }
    
    /* Add extension if missing - try .js first, then .json */
    size_t len = strlen(resolved);
    bool has_js_ext = ends_with(resolved, ".js");
    bool has_json_ext = ends_with(resolved, ".json");
    
    if (!has_js_ext && !has_json_ext) {
        /* No extension - try .js first */
        if (len + 3 < resolved_len) {
            strcat(resolved, ".js");
            /* Check if .js file exists, if not try .json */
            if (fs_exists(resolved) != FS_OK) {
                /* Remove .js and try .json */
                resolved[len] = '\0';
                if (len + 5 < resolved_len) {
                    strcat(resolved, ".json");
                    /* If .json also doesn't exist, revert to .js for error message */
                    if (fs_exists(resolved) != FS_OK) {
                        resolved[len] = '\0';
                        strcat(resolved, ".js");
                    }
                }
            }
        } else {
            return false;
        }
    }
    
    return true;
}

/*
 * Load and execute a module, returning its exports
 */
static jerry_value_t load_module(const char *resolved_path) {
    /* Check cache first */
    cached_module_t *cached = find_cached_module(resolved_path);
    if (cached != NULL) {
        return jerry_value_copy(cached->exports);
    }
    
    /* Read file content */
    char *content = NULL;
    size_t content_len = 0;
    bool content_is_static = false;
    
    js_result_t result = read_module_file(resolved_path, &content, &content_len, &content_is_static);
    if (result != JS_OK) {
        /* Try to find a similar module name to suggest */
        char error_msg[192];
        const char *basename = strrchr(resolved_path, '/');
        basename = basename ? basename + 1 : resolved_path;
        
        /* Remove .js extension for comparison */
        char name_without_ext[64];
        strncpy(name_without_ext, basename, sizeof(name_without_ext) - 1);
        name_without_ext[sizeof(name_without_ext) - 1] = '\0';
        char *dot = strrchr(name_without_ext, '.');
        if (dot && strcmp(dot, ".js") == 0) *dot = '\0';
        
        /* Search builtin modules for similar names */
        int best_dist = 3;  /* Max distance to suggest */
        const char *suggestion = NULL;
        const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();

        for (size_t i = 0; i < registry->builtin_module_count; i++) {
            const char *module_name = registry->builtin_modules[i];
            /* Skip special module names with colons */
            if (strchr(module_name, ':') != NULL) continue;
            
            int dist = levenshtein(name_without_ext, module_name, best_dist);
            if (dist < best_dist) {
                best_dist = dist;
                suggestion = module_name;
            }
        }
        
        if (suggestion != NULL) {
            snprintf(error_msg, sizeof(error_msg), 
                    "Cannot find module '%s'. Did you mean '%s'?", resolved_path, suggestion);
        } else {
            snprintf(error_msg, sizeof(error_msg), "Cannot find module '%s'", resolved_path);
        }
        return jerry_throw_sz(JERRY_ERROR_COMMON, error_msg);
    }
    
    /* Check if this is a JSON file */
    if (ends_with(resolved_path, ".json")) {
        /* Parse JSON directly using JSON.parse() */
        jerry_value_t json_str = jerry_string((const jerry_char_t *)content, content_len, JERRY_ENCODING_UTF8);
        if (!content_is_static) {
            js_module_free_file(content);
        }
        
        if (jerry_value_is_exception(json_str)) {
            return json_str;
        }
        
        /* Get JSON.parse from global */
        jerry_value_t global = jerry_current_realm();
        jerry_value_t json_key = jerry_string_sz("JSON");
        jerry_value_t json_obj = jerry_object_get(global, json_key);
        jerry_value_free(json_key);
        jerry_value_free(global);
        
        jerry_value_t parse_key = jerry_string_sz("parse");
        jerry_value_t parse_func = jerry_object_get(json_obj, parse_key);
        jerry_value_free(parse_key);
        jerry_value_free(json_obj);
        
        /* Call JSON.parse(content) */
        jerry_value_t args[1] = { json_str };
        jerry_value_t parsed = jerry_call(parse_func, jerry_undefined(), args, 1);
        
        jerry_value_free(parse_func);
        jerry_value_free(json_str);
        
        if (jerry_value_is_exception(parsed)) {
            return parsed;
        }
        
        /* Cache and return the parsed JSON */
        cache_module(resolved_path, parsed);
        return parsed;
    }
    
    /* JavaScript module - wrap and execute */
    
    /* Create module wrapper:
     * (function(exports, require, module, __filename, __dirname) {
     *   <module code>
     * })
     * 
     * Use static buffer when possible to avoid malloc() during require().
     * This is critical for memory-constrained situations (e.g., DVI running).
     */
    const char *wrapper_start = "(function(exports, require, module, __filename, __dirname) {\n";
    const char *wrapper_end = "\n})";
    
    size_t wrapper_start_len = strlen(wrapper_start);
    size_t wrapper_end_len = strlen(wrapper_end);
    size_t wrapper_len = wrapper_start_len + content_len + wrapper_end_len + 1;
    char *wrapped = NULL;
    
    if (content_is_static) {
        if (wrapper_len > sizeof(s_module_wrapper_buf)) {
            return jerry_throw_sz(JERRY_ERROR_COMMON, "Module too large for static buffer");
        }
        /* Shift content forward and wrap in-place */
        memmove(s_module_wrapper_buf + wrapper_start_len, s_module_wrapper_buf, content_len + 1);
        memcpy(s_module_wrapper_buf, wrapper_start, wrapper_start_len);
        memcpy(s_module_wrapper_buf + wrapper_start_len + content_len, wrapper_end, wrapper_end_len);
        s_module_wrapper_buf[wrapper_start_len + content_len + wrapper_end_len] = '\0';
        wrapped = s_module_wrapper_buf;
    } else {
        wrapped = (char *)realloc(content, wrapper_len);
        if (wrapped == NULL) {
            js_module_free_file(content);
            return jerry_throw_sz(JERRY_ERROR_COMMON, "Out of memory loading module");
        }
        memmove(wrapped + wrapper_start_len, wrapped, content_len + 1);
        memcpy(wrapped, wrapper_start, wrapper_start_len);
        memcpy(wrapped + wrapper_start_len + content_len, wrapper_end, wrapper_end_len);
        wrapped[wrapper_start_len + content_len + wrapper_end_len] = '\0';
    }
    
    /* Parse the wrapper function */
    jerry_value_t parsed = jerry_parse((const jerry_char_t *)wrapped, strlen(wrapped), NULL);
    if (!content_is_static) {
        free(wrapped);
    }
    
    if (jerry_value_is_exception(parsed)) {
        return parsed;  /* Return the parse error */
    }
    
    /* Execute to get the wrapper function */
    jerry_value_t wrapper_func = jerry_run(parsed);
    jerry_value_free(parsed);
    
    if (jerry_value_is_exception(wrapper_func)) {
        return wrapper_func;
    }
    
    /* Create module object: { exports: {} } */
    jerry_value_t module_obj = jerry_object();
    jerry_value_t exports_obj = jerry_object();
    
    jerry_value_t exports_key = jerry_string_sz("exports");
    set_property_value(module_obj, exports_key, exports_obj);
    jerry_value_free(exports_key);
    
    /* Each module gets a require function bound to its defining path. */
    jerry_value_t require_func = create_require_function(resolved_path);
    if (jerry_value_is_exception(require_func)) {
        jerry_value_free(wrapper_func);
        jerry_value_free(module_obj);
        jerry_value_free(exports_obj);
        return require_func;
    }
    
    /* Create __filename and __dirname strings */
    jerry_value_t filename = jerry_string_sz(resolved_path);
    
    /* Extract directory from path */
    char dirname[MAX_MODULE_PATH];
    strncpy(dirname, resolved_path, sizeof(dirname));
    char *last_slash = strrchr(dirname, '/');
    if (last_slash != NULL) {
        if (last_slash == dirname) {
            /* Root directory case: /file.js -> dirname is "/" */
            dirname[1] = '\0';
        } else {
            *last_slash = '\0';
        }
    } else {
        dirname[0] = '/';
        dirname[1] = '\0';
    }
    jerry_value_t dirname_val = jerry_string_sz(dirname);
    
    /* Call wrapper function: wrapper(exports, require, module, __filename, __dirname) */
    jerry_value_t args[5] = { exports_obj, require_func, module_obj, filename, dirname_val };
    jerry_value_t call_result = jerry_call(wrapper_func, jerry_undefined(), args, 5);
    
    jerry_value_free(wrapper_func);
    jerry_value_free(require_func);
    jerry_value_free(filename);
    jerry_value_free(dirname_val);
    
    if (jerry_value_is_exception(call_result)) {
        jerry_value_free(exports_obj);
        jerry_value_free(module_obj);
        return call_result;
    }
    jerry_value_free(call_result);
    
    /* Get final exports (might have been replaced via module.exports = ...) */
    jerry_value_t final_exports_key = jerry_string_sz("exports");
    jerry_value_t final_exports = jerry_object_get(module_obj, final_exports_key);
    jerry_value_free(final_exports_key);
    jerry_value_free(module_obj);
    jerry_value_free(exports_obj);
    
    /* Cache the module */
    cache_module(resolved_path, final_exports);
    
    return final_exports;
}

/*
 * Built-in module descriptors
 * These modules are provided by native C bindings and should not be loaded from disk
 */
typedef jerry_value_t (*builtin_factory_t)(void);

typedef struct {
    const char *specifier;
    builtin_factory_t factory;
} builtin_module_t;

static const builtin_module_t s_builtin_modules[];

static jerry_value_t create_process_module(void) {
    jerry_value_t global = jerry_current_realm();
    jerry_value_t key = jerry_string_sz("process");
    jerry_value_t module = jerry_object_get(global, key);
    jerry_value_free(key);
    jerry_value_free(global);
    return module;
}

static jerry_value_t create_builtin_modules_list(void) {
    jerry_value_t array = jerry_array(0);
    const mcujs_runtime_registry_t *registry = mcujs_runtime_registry();

    for (uint32_t i = 0; i < registry->builtin_module_count; i++) {
        char index_str[11];
        snprintf(index_str, sizeof(index_str), "%" PRIu32, i);
        jerry_value_t entry = jerry_string_sz(registry->builtin_modules[i]);
        jerry_value_t idx = jerry_string_sz(index_str);
        set_property_value(array, idx, entry);
        jerry_value_free(entry);
        jerry_value_free(idx);
    }

    return array;
}

static jerry_value_t module_has_handler(const jerry_call_info_t *call_info,
                                        const jerry_value_t args[],
                                        jerry_length_t argc) {
    (void)call_info;
    if (argc < 1 || !jerry_value_is_string(args[0])) return jerry_boolean(false);
    char name[MAX_MODULE_PATH];
    jerry_size_t length = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    if (length == 0 || length >= sizeof(name)) return jerry_boolean(false);
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8,
                           (jerry_char_t *)name, length);
    name[length] = '\0';
    return jerry_boolean(mcujs_runtime_has_module(name));
}

static jerry_value_t create_module_module(void) {
    jerry_value_t module = jerry_object();
    jerry_value_t list = create_builtin_modules_list();
    jerry_value_t key = jerry_string_sz("builtinModules");
    set_property_value(module, key, list);
    jerry_value_free(key);
    jerry_value_free(list);
    js_set_function(module, "has", module_has_handler);
    return module;
}

static const builtin_module_t s_builtin_modules[] = {
#if MCUJS_FEATURE_BOARD
    {"board", js_create_board_module},
#endif
#if MCUJS_FEATURE_FS
    {"fs", js_create_fs_module},
#endif
#if MCUJS_FEATURE_PROCESS
    {"process", create_process_module},
#endif
#if MCUJS_FEATURE_GPIO
    {"gpio", js_create_gpio_module},
#endif
#if MCUJS_FEATURE_PWM
    {"pwm", js_create_pwm_module},
#endif
#if MCUJS_FEATURE_I2C
    {"i2c", js_create_i2c_module},
#endif
#if MCUJS_FEATURE_SPI
    {"spi", js_create_spi_module},
#endif
#if MCUJS_FEATURE_ADC
    {"adc", js_create_adc_module},
#endif
#if MCUJS_FEATURE_NEOPIXEL
    {"neopixel", js_create_neopixel_module},
#endif
#if MCUJS_FEATURE_IMAGE
    {"image", js_create_image_module},
#endif
#if MCUJS_FEATURE_KEYBOARD
    {"keyboard", js_create_keyboard_module},
#endif
#if MCUJS_FEATURE_MOUSE
    {"mouse", js_create_mouse_module},
#endif
    {"mcujs:module", create_module_module},
    {NULL, NULL}
};

static jerry_value_t s_builtin_cache[16];
static bool s_builtin_cached[16];

/*
 * Check if a specifier is a built-in module name
 * Returns the module object if it is, or undefined if not
 */
static jerry_value_t get_builtin_module(const char *specifier) {
    if (!mcujs_runtime_has_module(specifier)) return jerry_undefined();
    const char *lookup = specifier;
    if (strcmp(specifier, "node:module") == 0) {
        lookup = "mcujs:module";
    }

    for (int i = 0; s_builtin_modules[i].specifier != NULL; i++) {
        if (strcmp(lookup, s_builtin_modules[i].specifier) == 0) {
            if (!s_builtin_cached[i]) {
                s_builtin_cache[i] = s_builtin_modules[i].factory();
                s_builtin_cached[i] = true;
            }
            return jerry_value_copy(s_builtin_cache[i]);
        }
    }
    return jerry_undefined();
}

/*
 * require(specifier) - Load a CommonJS module
 */
static jerry_value_t require_handler(const jerry_call_info_t *call_info,
                                      const jerry_value_t args[],
                                      const jerry_length_t argc) {
    if (argc < 1 || !jerry_value_is_string(args[0])) {
        return jerry_throw_sz(JERRY_ERROR_TYPE, "require() argument must be a string");
    }
    
    /* Get module specifier */
    char specifier[MAX_MODULE_PATH];
    jerry_size_t len = jerry_string_size(args[0], JERRY_ENCODING_UTF8);
    if (len >= sizeof(specifier)) {
        return jerry_throw_sz(JERRY_ERROR_RANGE, "Module path too long");
    }
    jerry_string_to_buffer(args[0], JERRY_ENCODING_UTF8, 
                           (jerry_char_t *)specifier, len);
    specifier[len] = '\0';
    
    /* Check for built-in modules first (bare specifiers like 'fs', 'process') */
    if (specifier[0] != '.' && specifier[0] != '/') {
        jerry_value_t builtin = get_builtin_module(specifier);
        if (!jerry_value_is_undefined(builtin)) {
            return builtin;
        }
        jerry_value_free(builtin);
    }
    
    /* Resolve the path */
    require_context_t *context = jerry_object_get_native_ptr(
        call_info->function, &s_require_context_info);
    const char *from_path = context == NULL ? "" : context->from_path;
    char resolved[MAX_MODULE_PATH];
    if (!resolve_module_path(specifier, from_path, resolved, sizeof(resolved))) {
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Failed to resolve module path");
    }
    
    /* Load the module */
    return load_module(resolved);
}

/*
 * require.cache - Object containing cached modules
 */
static jerry_value_t require_cache_getter(const jerry_call_info_t *call_info,
                                           const jerry_value_t args[],
                                           const jerry_length_t argc) {
    (void)call_info;
    (void)args;
    (void)argc;
    
    jerry_value_t cache = jerry_object();
    
    for (size_t i = 0; i < s_cache_count; i++) {
        if (s_module_cache[i].loaded) {
            jerry_value_t key = jerry_string_sz(s_module_cache[i].path);
            jerry_value_t module_obj = jerry_object();
            
            jerry_value_t exports_key = jerry_string_sz("exports");
            set_property_value(module_obj, exports_key, s_module_cache[i].exports);
            jerry_value_free(exports_key);
            
            set_property_value(cache, key, module_obj);
            jerry_value_free(key);
            jerry_value_free(module_obj);
        }
    }
    
    return cache;
}

/*
 * Clear module cache (useful for hot reloading)
 */
void js_require_clear_cache(void) {
    for (size_t i = 0; i < s_cache_count; i++) {
        if (s_module_cache[i].loaded) {
            jerry_value_free(s_module_cache[i].exports);
            s_module_cache[i].loaded = false;
        }
    }
    s_cache_count = 0;
}

/*
 * Register require() and related globals
 */
void js_bind_require(void) {
    jerry_value_t global = jerry_current_realm();
    
    /* Create require function */
    jerry_value_t require_func = create_require_function(NULL);
    
    /* Add require.cache as a getter property */
    jerry_value_t cache_key = jerry_string_sz("cache");
    jerry_value_t cache_getter = jerry_function_external(require_cache_getter);
    
    jerry_property_descriptor_t prop_desc = jerry_property_descriptor();
    prop_desc.flags |= JERRY_PROP_IS_GET_DEFINED;
    prop_desc.getter = cache_getter;
    
    jerry_object_define_own_prop(require_func, cache_key, &prop_desc);
    
    jerry_property_descriptor_free(&prop_desc);
    jerry_value_free(cache_key);
    jerry_value_free(cache_getter);
    
    /* Register global require */
    jerry_value_t require_key = jerry_string_sz("require");
    set_property_value(global, require_key, require_func);
    jerry_value_free(require_key);
    jerry_value_free(require_func);
    
    jerry_value_free(global);
}
