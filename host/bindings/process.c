/*
 * mcujs - Process Bindings
 * 
 * Implements: process object (Node.js-compatible)
 * - process.version - mcujs version
 * - process.versions - dependency versions
 * - process.arch - CPU architecture
 * - process.platform - platform name
 */

#include "bindings.h"
#include "jerryscript.h"
#include "board_config.h"

#include <stdio.h>

/* External helpers from bindings.c */
extern void js_set_property(jerry_value_t object, const char *name, jerry_value_t value);
extern void js_set_string(jerry_value_t object, const char *name, const char *value);
extern void js_register_global(const char *name, jerry_value_t object);

/*
 * Register process bindings
 */
void js_bind_process(void) {
    jerry_value_t process = jerry_object();
    
    /* process.version - mcujs version (with 'v' prefix like Node.js) */
    #ifdef MCUJS_VERSION
    char version_str[16];
    snprintf(version_str, sizeof(version_str), "v%s", MCUJS_VERSION);
    js_set_string(process, "version", version_str);
    #else
    js_set_string(process, "version", "v0.0.0");
    #endif
    
    /* process.arch - CPU architecture */
    #if defined(MCUJS_BOARD_CHIP)
    js_set_string(process, "arch", MCUJS_BOARD_CHIP);
    #else
    js_set_string(process, "arch", "arm");
    #endif
    
    /* process.platform - always "mcujs" to distinguish from Node.js */
    js_set_string(process, "platform", "mcujs");
    
    /* process.versions - object with all dependency versions */
    jerry_value_t versions = jerry_object();
    
    /* mcujs version */
    #ifdef MCUJS_VERSION
    js_set_string(versions, "mcujs", MCUJS_VERSION);
    #else
    js_set_string(versions, "mcujs", "0.0.0");
    #endif
    
    /* JerryScript version */
    #ifdef JERRYSCRIPT_VERSION
    js_set_string(versions, "jerryscript", JERRYSCRIPT_VERSION);
    #else
    js_set_string(versions, "jerryscript", "3.0.0");
    #endif
    
    /* Pico SDK version */
    #ifdef PICO_SDK_VERSION
    js_set_string(versions, "pico-sdk", PICO_SDK_VERSION);
    #else
    js_set_string(versions, "pico-sdk", "2.2.0");
    #endif
    
    /* TinyUSB version */
    #ifdef TINYUSB_VERSION
    js_set_string(versions, "tinyusb", TINYUSB_VERSION);
    #else
    js_set_string(versions, "tinyusb", "0.16.0");
    #endif
    
    /* FatFs version (R0.16) */
    js_set_string(versions, "fatfs", "0.16");
    
    js_set_property(process, "versions", versions);
    jerry_value_free(versions);
    
    js_register_global("process", process);
    jerry_value_free(process);
}
