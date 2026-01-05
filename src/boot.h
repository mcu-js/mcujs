/**
 * mcujs - JavaScript Runtime for Raspberry Pi Pico
 * Boot Loader Header
 */

#ifndef MCUJS_BOOT_H
#define MCUJS_BOOT_H

#include <stdbool.h>

/**
 * Default entry point filename
 */
#define BOOT_INDEX_FILE "/index.js"

/**
 * Attempt to boot from index.js
 * Looks for /index.js on the filesystem and executes it if found.
 * 
 * @return true if index.js was found and executed (may have had errors)
 *         false if index.js was not found
 */
bool boot_run_index_js(void);

/**
 * Run a JavaScript file from the filesystem
 * 
 * @param filename Path to the JavaScript file
 * @return true if file was executed successfully
 *         false if file could not be read or had execution errors
 */
bool boot_run_file(const char* filename);

/**
 * Check if a boot file exists
 * 
 * @return true if /index.js exists on the filesystem
 */
bool boot_file_exists(void);

#endif // MCUJS_BOOT_H
