#ifndef MCUJS_ESP32_FILESYSTEM_H
#define MCUJS_ESP32_FILESYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"

/* True only when the validated FFAT partition is entirely erased (0xff). */
bool mcujs_filesystem_partition_is_erased(void);

/* Transfer exclusive storage ownership between FatFs and USB MSC. */
fs_result_t mcujs_filesystem_begin_host_access(void);
fs_result_t mcujs_filesystem_end_host_access(void);
bool mcujs_filesystem_host_owned(void);
uint32_t mcujs_filesystem_sector_size(void);

#endif /* MCUJS_ESP32_FILESYSTEM_H */
