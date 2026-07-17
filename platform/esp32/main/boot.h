#ifndef MCUJS_ESP32_BOOT_H
#define MCUJS_ESP32_BOOT_H

#include <stdbool.h>

bool mcujs_boot_init(void);
void mcujs_boot_execute_index(void);
void mcujs_boot_task(void);

bool mcujs_boot_storage_ready(void);
bool mcujs_boot_safe_mode(void);
bool mcujs_boot_set_safe_mode(bool enabled);

#endif /* MCUJS_ESP32_BOOT_H */
