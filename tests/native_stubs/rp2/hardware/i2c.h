#ifndef MCUJS_TEST_RP2_I2C_H
#define MCUJS_TEST_RP2_I2C_H

#include "pico/stdlib.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int index;
} i2c_inst_t;

extern i2c_inst_t *i2c0;
extern i2c_inst_t *i2c1;

uint i2c_init(i2c_inst_t *instance, uint baudrate);
void i2c_deinit(i2c_inst_t *instance);
int i2c_write_blocking(i2c_inst_t *instance, uint8_t address,
                       const uint8_t *data, size_t length, bool nostop);
int i2c_read_blocking(i2c_inst_t *instance, uint8_t address,
                      uint8_t *data, size_t length, bool nostop);

#endif
