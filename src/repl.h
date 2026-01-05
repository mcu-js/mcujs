/**
 * mcujs - JavaScript Runtime for Raspberry Pi Pico
 * REPL (Read-Eval-Print Loop) Header
 */

#ifndef MCUJS_REPL_H
#define MCUJS_REPL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize the REPL subsystem
 */
void repl_init(void);

/**
 * Process REPL input (non-blocking)
 * Should be called regularly from the main loop
 */
void repl_task(void);

/**
 * Check if REPL is currently processing multi-line input
 */
bool repl_is_multiline(void);

/**
 * Reset the REPL state (clear input buffer)
 */
void repl_reset(void);

#endif // MCUJS_REPL_H
