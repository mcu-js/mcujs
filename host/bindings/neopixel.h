#ifndef MCUJS_NEOPIXEL_H
#define MCUJS_NEOPIXEL_H

#include <stdbool.h>
#include <stdint.h>

void neopixel_init(uint32_t pin, uint32_t length);
void neopixel_set_order(bool grb);
bool neopixel_is_grb(void);
void neopixel_set_pixel_ordered(uint32_t index, uint8_t r, uint8_t g, uint8_t b, bool grb);
void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
void neopixel_show(void);
void neopixel_clear(void);
bool neopixel_is_ready(void);
uint32_t neopixel_length(void);

#endif /* MCUJS_NEOPIXEL_H */
