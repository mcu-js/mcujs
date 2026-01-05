/*
 * mcujs - Module Loader Interface
 * 
 * Handles JavaScript module loading (import/require)
 */

#ifndef MCUJS_MODULE_LOADER_H
#define MCUJS_MODULE_LOADER_H

#include "engine.h"
#include <stddef.h>

/*
 * Initialize module loader
 */
void js_module_loader_init(void);

/*
 * Cleanup module loader
 */
void js_module_loader_cleanup(void);

/*
 * Read a file from the filesystem
 * Caller must free the returned buffer using js_module_free_file()
 * 
 * @param filename: Path to file
 * @param content: Output pointer to file content
 * @param content_len: Output length of content
 * 
 * Returns: JS_OK on success, error code otherwise
 */
js_result_t js_module_read_file(const char *filename, char **content, size_t *content_len);

/*
 * Free file content allocated by js_module_read_file
 */
void js_module_free_file(char *content);

/*
 * Resolve a module path relative to the current module
 * 
 * @param base_path: Path of the importing module (can be NULL for top-level)
 * @param specifier: Module specifier (e.g., "./foo", "../bar", "module")
 * @param resolved: Output buffer for resolved path
 * @param resolved_len: Size of output buffer
 * 
 * Returns: JS_OK on success, error code otherwise
 */
js_result_t js_module_resolve(const char *base_path, const char *specifier,
                               char *resolved, size_t resolved_len);

/*
 * Load and execute a module
 * Modules are cached after first load
 * 
 * @param module_path: Resolved path to module
 * 
 * Returns: JS_OK on success, error code otherwise
 */
js_result_t js_module_load(const char *module_path);

/*
 * Clear the module cache
 */
void js_module_clear_cache(void);

#endif /* MCUJS_MODULE_LOADER_H */
