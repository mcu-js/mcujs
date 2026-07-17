/* MCU.js process binding for ESP-IDF. */

#include "bindings.h"
#include "board_config.h"
#include "jerryscript.h"

#include "esp_idf_version.h"

#include <stdio.h>

void js_bind_process(void) {
    jerry_value_t process = jerry_object();
    char version[20];
    snprintf(version, sizeof(version), "v%s", MCUJS_VERSION);
    js_set_string(process, "version", version);
    js_set_string(process, "arch", MCUJS_BOARD_CHIP);
    js_set_string(process, "platform", "mcujs");

    jerry_value_t versions = jerry_object();
    js_set_string(versions, "mcujs", MCUJS_VERSION);
    js_set_string(versions, "jerryscript", JERRYSCRIPT_VERSION);
    js_set_string(versions, "esp-idf", IDF_VER);
    js_set_property(process, "versions", versions);
    jerry_value_free(versions);

    js_register_global("process", process);
    jerry_value_free(process);
}
