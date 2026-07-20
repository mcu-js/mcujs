#ifndef MCUJS_NEOPIXEL_H
#define MCUJS_NEOPIXEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NEOPIXEL_INIT_FAILURE_NONE = 0,
    NEOPIXEL_INIT_FAILURE_BUSY,
    NEOPIXEL_INIT_FAILURE_RESOURCE_EXHAUSTED,
    NEOPIXEL_INIT_FAILURE_IO,
} neopixel_init_failure_t;

bool neopixel_init(uint32_t pin, uint32_t length);
neopixel_init_failure_t neopixel_last_init_failure(void);
void neopixel_set_order(bool grb);
bool neopixel_is_grb(void);
void neopixel_set_pixel_ordered(uint32_t index, uint8_t r, uint8_t g, uint8_t b, bool grb);
void neopixel_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
void neopixel_show(void);
void neopixel_clear(void);
bool neopixel_is_ready(void);
uint32_t neopixel_length(void);
int neopixel_pin(void);

#endif /* MCUJS_NEOPIXEL_H */
