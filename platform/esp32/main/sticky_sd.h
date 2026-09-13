#ifndef MCUJS_STICKY_SD_H
#define MCUJS_STICKY_SD_H
#include "fs.h"

#define STICKY_SD_BASE_PATH "/mcujs-sd"
/* Board-private SPI2 owner. Call before adding the display device: initializes
 * an inserted card into SPI mode with both CS high, but does not mount it.
 * The bus/card live until reset; display close must remove only its device.
 * False means bus/pin setup failed, not merely absent/unsupported media.
 * All callers are serialized on the runtime task; no hot-swap support. */
bool sticky_sd_prepare(void);
fs_result_t sticky_sd_mount(void);
fs_result_t sticky_sd_status(void);
#endif
