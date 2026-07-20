#ifndef MCUJS_TEST_RP2_CLOCKS_H
#define MCUJS_TEST_RP2_CLOCKS_H

#include <stdint.h>

#define clk_sys 0
#define clk_peri 1

uint32_t clock_get_hz(int clock);

#endif