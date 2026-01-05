/*
 * mcujs - Module Loader Implementation
 * 
 * Handles JavaScript module loading (import/require)
 * Note: JerryScript v3.0.0 doesn't have external module callbacks,
 * so we implement a simpler require()-style loader.
 */

#include "module_loader.h"
#include "fs.h"

#include "jerryscript.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum number of cached modules */
#define MAX_CACHED_MODULES 16

/* Maximum path length */
#define MAX_PATH_LENGTH 128

/* Module cache entry */
typedef struct {
    char path[MAX_PATH_LENGTH];
    jerry_value_t module_value;
    bool loaded;
} module_cache_entry_t;

/* Module cache */
static module_cache_entry_t s_module_cache[MAX_CACHED_MODULES];
static size_t s_module_count = 0;

void js_module_loader_init(void) {
    /* Clear cache */
    memset(s_module_cache, 0, sizeof(s_module_cache));
    s_module_count = 0;
    
    /* JerryScript v3.0.0 doesn't have external module resolution callbacks.
     * Module loading is handled via js_module_load() called from our
     * custom require() implementation in bindings. */
}

void js_module_loader_cleanup(void) {
    /* Free cached modules */
    for (size_t i = 0; i < s_module_count; i++) {
        if (s_module_cache[i].loaded) {
            jerry_value_free(s_module_cache[i].module_value);
        }
    }
    
    s_module_count = 0;
    memset(s_module_cache, 0, sizeof(s_module_cache));
}

js_result_t js_module_read_file(const char *filename, char **content, size_t *content_len) {
    if (filename == NULL || content == NULL || content_len == NULL) {
        return JS_ERROR_FILE_READ;
    }
    
    /* Open file */
    fs_file_t file;
    fs_result_t result = fs_open(&file, filename, FS_MODE_READ);
    
    if (result != FS_OK) {
        return JS_ERROR_FILE_NOT_FOUND;
    }
    
    /* Get file size */
    size_t file_size;
    result = fs_size(&file, &file_size);
    
    if (result != FS_OK) {
        fs_close(&file);
        return JS_ERROR_FILE_READ;
    }
    
    /* Allocate buffer */
    *content = (char *)malloc(file_size + 1);
    if (*content == NULL) {
        fs_close(&file);
        return JS_ERROR_MEMORY;
    }
    
    /* Read content */
    size_t bytes_read;
    result = fs_read(&file, *content, file_size, &bytes_read);
    
    fs_close(&file);
    
    if (result != FS_OK || bytes_read != file_size) {
        free(*content);
        *content = NULL;
        return JS_ERROR_FILE_READ;
    }
    
    (*content)[file_size] = '\0';
    *content_len = file_size;
    
    return JS_OK;
}

void js_module_free_file(char *content) {
    if (content != NULL) {
        free(content);
    }
}

js_result_t js_module_resolve(const char *base_path, const char *specifier,
                               char *resolved, size_t resolved_len) {
    if (specifier == NULL || resolved == NULL || resolved_len == 0) {
        return JS_ERROR_FILE_NOT_FOUND;
    }
    
    /* Handle absolute paths */
    if (specifier[0] == '/') {
        strncpy(resolved, specifier, resolved_len - 1);
        resolved[resolved_len - 1] = '\0';
    }
    /* Handle relative paths */
    else if (specifier[0] == '.') {
        if (base_path != NULL && base_path[0] != '\0') {
            /* Find directory of base path */
            char base_dir[MAX_PATH_LENGTH];
            strncpy(base_dir, base_path, sizeof(base_dir) - 1);
            base_dir[sizeof(base_dir) - 1] = '\0';
            
            char *last_slash = strrchr(base_dir, '/');
            if (last_slash != NULL) {
                *(last_slash + 1) = '\0';
            } else {
                base_dir[0] = '\0';
            }
            
            /* Combine with specifier */
            snprintf(resolved, resolved_len, "%s%s", base_dir, specifier);
        } else {
            /* Relative to root */
            if (specifier[0] == '.' && specifier[1] == '/') {
                snprintf(resolved, resolved_len, "/%s", specifier + 2);
            } else {
                snprintf(resolved, resolved_len, "/%s", specifier);
            }
        }
    }
    /* Handle bare specifiers (future: node_modules style) */
    else {
        snprintf(resolved, resolved_len, "/%s", specifier);
    }
    
    /* Add .js extension if missing */
    size_t len = strlen(resolved);
    if (len < 3 || strcmp(resolved + len - 3, ".js") != 0) {
        if (len + 3 < resolved_len) {
            strcat(resolved, ".js");
        }
    }
    
    return JS_OK;
}

js_result_t js_module_load(const char *module_path) {
    if (module_path == NULL) {
        return JS_ERROR_FILE_NOT_FOUND;
    }
    
    /* Check cache first */
    for (size_t i = 0; i < s_module_count; i++) {
        if (strcmp(s_module_cache[i].path, module_path) == 0 && 
            s_module_cache[i].loaded) {
            /* Module already loaded */
            return JS_OK;
        }
    }
    
    /* Read file */
    char *content = NULL;
    size_t content_len = 0;
    
    js_result_t read_result = js_module_read_file(module_path, &content, &content_len);
    if (read_result != JS_OK) {
        return read_result;
    }
    
    /* Parse and execute as script (not ES module) */
    jerry_value_t parsed = jerry_parse((const jerry_char_t *)content, content_len, NULL);
    
    js_module_free_file(content);
    
    if (jerry_value_is_exception(parsed)) {
        jerry_value_free(parsed);
        return JS_ERROR_PARSE;
    }
    
    /* Execute */
    jerry_value_t result = jerry_run(parsed);
    jerry_value_free(parsed);
    
    if (jerry_value_is_exception(result)) {
        jerry_value_free(result);
        return JS_ERROR_EXEC;
    }
    
    /* Cache the result if space available */
    if (s_module_count < MAX_CACHED_MODULES) {
        strncpy(s_module_cache[s_module_count].path, module_path, MAX_PATH_LENGTH - 1);
        s_module_cache[s_module_count].module_value = result;
        s_module_cache[s_module_count].loaded = true;
        s_module_count++;
    } else {
        jerry_value_free(result);
    }
    
    return JS_OK;
}

void js_module_clear_cache(void) {
    for (size_t i = 0; i < s_module_count; i++) {
        if (s_module_cache[i].loaded) {
            jerry_value_free(s_module_cache[i].module_value);
            s_module_cache[i].loaded = false;
        }
    }
    s_module_count = 0;
}
