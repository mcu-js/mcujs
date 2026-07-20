#ifndef MCUJS_I2C_OPTIONS_H
#define MCUJS_I2C_OPTIONS_H

#include "jerryscript.h"

typedef struct {
    int bus;
    int sda;
    int scl;
    int frequency;
} mcujs_i2c_init_options_t;

/* Parses the preferred init(options) form and the 0.x positional compatibility
 * form. Returns undefined on success or an owned exception on failure. */
jerry_value_t mcujs_parse_i2c_init_args(
    const jerry_value_t args[], jerry_length_t argc,
    mcujs_i2c_init_options_t *out);

#endif /* MCUJS_I2C_OPTIONS_H */
