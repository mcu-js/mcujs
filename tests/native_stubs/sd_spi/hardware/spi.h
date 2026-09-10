#ifndef SD_TEST_SPI_H
#define SD_TEST_SPI_H
#include "pico/stdlib.h"
typedef struct { int index; } spi_inst_t;
typedef struct { volatile uint32_t dr; } spi_hw_t;
extern spi_inst_t *spi1;
#define SPI_CPOL_0 0
#define SPI_CPHA_0 0
#define SPI_MSB_FIRST 0
uint spi_init(spi_inst_t *spi, uint baud);
uint spi_set_baudrate(spi_inst_t *spi, uint baud);
void spi_deinit(spi_inst_t *spi);
void spi_set_format(spi_inst_t *spi, uint bits, int cpol, int cpha, int order);
spi_hw_t *spi_get_hw(spi_inst_t *spi);
bool spi_is_writable(spi_inst_t *spi);
bool spi_is_readable(spi_inst_t *spi);
bool spi_is_busy(spi_inst_t *spi);
#endif
