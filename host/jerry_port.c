/*
 * mcujs - JerryScript Port Layer
 * 
 * Platform-specific implementations required by JerryScript
 */

#include "jerryscript.h"
#include "jerryscript-port.h"

#include "pico/stdlib.h"
#include "hardware/timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Port initialization
 */
void jerry_port_init(void) {
    /* Nothing to initialize for this platform */
}

/*
 * Fatal error handler
 */
void jerry_port_fatal(jerry_fatal_code_t code) {
    printf("mcujs: Fatal error %d\n", code);
    
    /* Blink LED rapidly to indicate error */
    while (1) {
        /* Halt - in production, could trigger watchdog reset */
        tight_loop_contents();
    }
}

/*
 * Console output
 */
void jerry_port_log(const char *message_p) {
    printf("%s", message_p);
}

/*
 * Get current time in milliseconds
 * Used for Date object
 */
double jerry_port_current_time(void) {
    return (double)to_ms_since_boot(get_absolute_time());
}

/*
 * Get local timezone adjustment
 * Returns offset from UTC in milliseconds
 * For embedded systems without RTC, we return 0 (UTC)
 */
int32_t jerry_port_local_tza(double unix_ms) {
    (void)unix_ms;
    return 0;  /* UTC - no timezone offset */
}

/*
 * Sleep for specified milliseconds
 */
void jerry_port_sleep(uint32_t sleep_time) {
    sleep_ms(sleep_time);
}

/*
 * Read a source file
 * This is used by JerryScript for module loading
 */
jerry_char_t *jerry_port_source_read(const char *file_name_p, jerry_size_t *out_size_p) {
    /* This is handled by our module loader, but provide a stub */
    (void)file_name_p;
    *out_size_p = 0;
    return NULL;
}

/*
 * Release source file buffer
 */
void jerry_port_source_free(uint8_t *buffer_p) {
    if (buffer_p != NULL) {
        free(buffer_p);
    }
}

/*
 * Normalize a file path
 */
jerry_char_t *jerry_port_path_normalize(const jerry_char_t *path_p, jerry_size_t path_size) {
    jerry_char_t *result = (jerry_char_t *)malloc(path_size + 1);
    if (result != NULL) {
        memcpy(result, path_p, path_size);
        result[path_size] = '\0';
    }
    return result;
}

/*
 * Free a normalized path
 */
void jerry_port_path_free(jerry_char_t *path_p) {
    if (path_p != NULL) {
        free(path_p);
    }
}

/*
 * Get the base name from a path
 */
jerry_size_t jerry_port_path_base(const jerry_char_t *path_p) {
    const jerry_char_t *basename_p = path_p;
    const jerry_char_t *p = path_p;
    
    while (*p != '\0') {
        if (*p == '/') {
            basename_p = p + 1;
        }
        p++;
    }
    
    return (jerry_size_t)(basename_p - path_p);
}

/*
 * Print a character (for console output)
 */
void jerry_port_print_char(char c) {
    putchar(c);
}

/*
 * Print a byte buffer
 */
void jerry_port_print_buffer(const jerry_char_t *buffer_p, jerry_size_t buffer_size) {
    for (jerry_size_t i = 0; i < buffer_size; i++) {
        putchar(buffer_p[i]);
    }
}
