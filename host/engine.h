/*
 * mcujs - JavaScript Engine Interface
 * 
 * Abstraction layer over JerryScript for the mcujs runtime
 */

#ifndef MCUJS_ENGINE_H
#define MCUJS_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Engine result codes
 */
typedef enum {
    JS_OK = 0,
    JS_ERROR_INIT,
    JS_ERROR_PARSE,
    JS_ERROR_EXEC,
    JS_ERROR_MEMORY,
    JS_ERROR_FILE_NOT_FOUND,
    JS_ERROR_FILE_READ,
} js_result_t;

/*
 * Initialize the JavaScript engine
 * Must be called before any other engine functions
 * 
 * Returns: JS_OK on success, error code otherwise
 */
js_result_t js_engine_init(void);

/*
 * Cleanup the JavaScript engine
 * Frees all resources
 */
void js_engine_cleanup(void);

/*
 * Execute JavaScript code from a string
 * 
 * @param code: JavaScript source code
 * @param code_len: Length of code string
 * @param result_buf: Buffer to store string result (can be NULL)
 * @param result_buf_len: Size of result buffer
 * 
 * Returns: JS_OK on success, error code otherwise
 */
js_result_t js_engine_exec(const char *code, size_t code_len, 
                           char *result_buf, size_t result_buf_len);

/*
 * Execute JavaScript code from a file
 * 
 * @param filename: Path to JavaScript file
 * 
 * Returns: JS_OK on success, error code otherwise
 */
js_result_t js_engine_exec_file(const char *filename);

/*
 * Get the last error message
 * 
 * @param buf: Buffer to store error message
 * @param buf_len: Size of buffer
 * 
 * Returns: Length of error message, or 0 if no error
 */
size_t js_engine_get_error(char *buf, size_t buf_len);

/*
 * Run garbage collection
 */
void js_engine_gc(void);

/*
 * Get engine memory statistics
 */
typedef struct {
    size_t heap_size;       /* Total heap size */
    size_t heap_used;       /* Used heap memory */
    size_t heap_peak;       /* Peak heap usage */
} js_memory_stats_t;

void js_engine_get_memory_stats(js_memory_stats_t *stats);

/*
 * Register all native bindings
 * Called automatically by js_engine_init()
 */
void js_register_bindings(void);

/*
 * Process pending timers
 * Should be called from main loop
 * 
 * Returns: true if there are more pending timers
 */
bool js_engine_process_timers(void);

/*
 * Check if engine is initialized
 */
bool js_engine_is_initialized(void);

/*
 * Tab completion callback type
 * Called for each matching property name
 * Return true to continue iteration, false to stop
 */
typedef bool (*js_completion_callback_t)(const char *name, void *user_data);

/*
 * Get completions for a partial identifier
 * 
 * @param partial: Partial text to complete (e.g., "cons" or "console.l")
 * @param callback: Called for each matching property
 * @param user_data: Passed to callback
 * 
 * Returns: Number of matches found
 */
int js_engine_get_completions(const char *partial, js_completion_callback_t callback, void *user_data);

/*
 * Register REPL-defined globals for completion
 */
void js_engine_register_global_identifier(const char *name);

#endif /* MCUJS_ENGINE_H */
