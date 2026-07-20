#ifndef MCUJS_TEST_RP2_SPI_H
#define MCUJS_TEST_RP2_SPI_H

#include "pico/stdlib.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int index;
} spi_inst_t;

typedef struct {
    volatile uint32_t dr;
} spi_hw_t;

extern spi_inst_t *spi0;
extern spi_inst_t *spi1;

#define SPI_CPOL_0 0
#define SPI_CPOL_1 1
#define SPI_CPHA_0 0
#define SPI_CPHA_1 1
#define SPI_MSB_FIRST 0

uint spi_init(spi_inst_t *instance, uint baudrate);
void spi_deinit(spi_inst_t *instance);
void spi_set_format(spi_inst_t *instance, uint data_bits, int cpol, int cpha,
                    int order);
int spi_write_read_blocking(spi_inst_t *instance, const uint8_t *tx,
                            uint8_t *rx, size_t length);
uint spi_get_dreq(spi_inst_t *instance, bool is_tx);
spi_hw_t *spi_get_hw(spi_inst_t *instance);
bool spi_is_busy(spi_inst_t *instance);

#endif
