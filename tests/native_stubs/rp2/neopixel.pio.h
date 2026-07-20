#ifndef MCUJS_TEST_RP2_NEOPIXEL_PIO_H
#define MCUJS_TEST_RP2_NEOPIXEL_PIO_H

#include "hardware/pio.h"

extern const pio_program_t mcujs_ws2812_program;
void mcujs_ws2812_program_init(PIO pio, uint state_machine, uint offset,
                               uint pin, float frequency, bool rgbw);

#endif
