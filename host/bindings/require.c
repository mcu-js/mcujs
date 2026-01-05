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
#include "jerryscript.h"
#include "fs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Maximum path length for modules */
#define MAX_MODULE_PATH 128

/* Maximum number of cached modules */
#define MAX_MODULES 16

/* Module cache entry */
typedef struct {
    char path[MAX_MODULE_PATH];
    jerry_value_t exports;
    bool loaded;
} cached_module_t;

/* Module cache */
static cached_module_t s_module_cache[MAX_MODULES];
static size_t s_cache_count = 0;

/* Current module path (for resolving relative paths) */
static char s_current_module_path[MAX_MODULE_PATH] = "";

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
        strncpy(resolved, specifier, resolved_len - 1);
        resolved[resolved_len - 1] = '\0';
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
        
        snprintf(resolved, resolved_len, "%s%s", base_dir, rel_path);
    }
    /* Bare specifier - search in /lib/ */
    else {
        snprintf(resolved, resolved_len, "/lib/%s", specifier);
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
    
    js_result_t result = js_module_read_file(resolved_path, &content, &content_len);
    if (result != JS_OK) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Cannot find module '%s'", resolved_path);
        return jerry_throw_sz(JERRY_ERROR_COMMON, error_msg);
    }
    
    /* Check if this is a JSON file */
    if (ends_with(resolved_path, ".json")) {
        /* Parse JSON directly using JSON.parse() */
        jerry_value_t json_str = jerry_string((const jerry_char_t *)content, content_len, JERRY_ENCODING_UTF8);
        js_module_free_file(content);
        
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
    
    /* Save current module path and set new one */
    char prev_path[MAX_MODULE_PATH];
    strncpy(prev_path, s_current_module_path, sizeof(prev_path));
    strncpy(s_current_module_path, resolved_path, sizeof(s_current_module_path) - 1);
    
    /* Create module wrapper:
     * (function(exports, require, module, __filename, __dirname) {
     *   <module code>
     * })
     */
    const char *wrapper_start = "(function(exports, require, module, __filename, __dirname) {\n";
    const char *wrapper_end = "\n})";
    
    size_t wrapper_len = strlen(wrapper_start) + content_len + strlen(wrapper_end) + 1;
    char *wrapped = (char *)malloc(wrapper_len);
    
    if (wrapped == NULL) {
        js_module_free_file(content);
        strncpy(s_current_module_path, prev_path, sizeof(s_current_module_path));
        return jerry_throw_sz(JERRY_ERROR_COMMON, "Out of memory loading module");
    }
    
    snprintf(wrapped, wrapper_len, "%s%s%s", wrapper_start, content, wrapper_end);
    js_module_free_file(content);
    
    /* Parse the wrapper function */
    jerry_value_t parsed = jerry_parse((const jerry_char_t *)wrapped, strlen(wrapped), NULL);
    free(wrapped);
    
    if (jerry_value_is_exception(parsed)) {
        strncpy(s_current_module_path, prev_path, sizeof(s_current_module_path));
        return parsed;  /* Return the parse error */
    }
    
    /* Execute to get the wrapper function */
    jerry_value_t wrapper_func = jerry_run(parsed);
    jerry_value_free(parsed);
    
    if (jerry_value_is_exception(wrapper_func)) {
        strncpy(s_current_module_path, prev_path, sizeof(s_current_module_path));
        return wrapper_func;
    }
    
    /* Create module object: { exports: {} } */
    jerry_value_t module_obj = jerry_object();
    jerry_value_t exports_obj = jerry_object();
    
    jerry_value_t exports_key = jerry_string_sz("exports");
    jerry_object_set(module_obj, exports_key, exports_obj);
    jerry_value_free(exports_key);
    
    /* Get require function from global */
    jerry_value_t global = jerry_current_realm();
    jerry_value_t require_key = jerry_string_sz("require");
    jerry_value_t require_func = jerry_object_get(global, require_key);
    jerry_value_free(require_key);
    jerry_value_free(global);
    
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
    
    /* Restore previous module path */
    strncpy(s_current_module_path, prev_path, sizeof(s_current_module_path));
    
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
 * Built-in module names that map to global objects
 * These modules are provided by native C bindings and should not be loaded from disk
 */
static const char *s_builtin_modules[] = {
    "fs",       /* Filesystem API */
    "process",  /* Process API */
    NULL        /* Sentinel */
};

/*
 * Check if a specifier is a built-in module name
 * Returns the global object if it is, or undefined if not
 */
static jerry_value_t get_builtin_module(const char *specifier) {
    for (int i = 0; s_builtin_modules[i] != NULL; i++) {
        if (strcmp(specifier, s_builtin_modules[i]) == 0) {
            /* Get the global object with this name */
            jerry_value_t global = jerry_current_realm();
            jerry_value_t key = jerry_string_sz(specifier);
            jerry_value_t module = jerry_object_get(global, key);
            jerry_value_free(key);
            jerry_value_free(global);
            return module;
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
    (void)call_info;
    
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
    char resolved[MAX_MODULE_PATH];
    if (!resolve_module_path(specifier, s_current_module_path, resolved, sizeof(resolved))) {
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
            jerry_object_set(module_obj, exports_key, s_module_cache[i].exports);
            jerry_value_free(exports_key);
            
            jerry_object_set(cache, key, module_obj);
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
    s_current_module_path[0] = '\0';
}

/*
 * Register require() and related globals
 */
void js_bind_require(void) {
    jerry_value_t global = jerry_current_realm();
    
    /* Create require function */
    jerry_value_t require_func = jerry_function_external(require_handler);
    
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
    jerry_object_set(global, require_key, require_func);
    jerry_value_free(require_key);
    jerry_value_free(require_func);
    
    jerry_value_free(global);
}
