#ifndef MCUJS_NEOPIXEL_OPTIONS_H
#define MCUJS_NEOPIXEL_OPTIONS_H

#include "jerryscript.h"

#include <stdbool.h>

typedef struct {
    int pin;
    int length;
    bool grb;
} mcujs_neopixel_init_options_t;

/* Parses the portable closed init(options) shape. Returns undefined on success
 * or an owned exception on failure. */
jerry_value_t mcujs_parse_neopixel_init_args(
    const jerry_value_t args[], jerry_length_t argc,
    mcujs_neopixel_init_options_t *out);

#endif /* MCUJS_NEOPIXEL_OPTIONS_H */
