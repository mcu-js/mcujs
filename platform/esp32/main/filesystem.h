#ifndef MCUJS_ESP32_FILESYSTEM_H
#define MCUJS_ESP32_FILESYSTEM_H

#include <stdbool.h>

/* True only when the validated FFAT partition is entirely erased (0xff). */
bool mcujs_filesystem_partition_is_erased(void);

#endif /* MCUJS_ESP32_FILESYSTEM_H */
