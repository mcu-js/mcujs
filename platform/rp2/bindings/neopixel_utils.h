#ifndef MCUJS_NEOPIXEL_UTILS_H
#define MCUJS_NEOPIXEL_UTILS_H

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

static inline void neopixel_write_reset(void) {
    sleep_us(80);
}

#endif /* MCUJS_NEOPIXEL_UTILS_H */
