#ifndef CANVAS_ST7789_FAKE_SDK_H
#define CANVAS_ST7789_FAKE_SDK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef unsigned int uint;
#define NUM_BANK0_GPIOS 30
#define SPI_SSPCR1_SSE_BITS 2u
#define RESETS_RESET_SPI0_BITS (1u << 16)
#define RESETS_RESET_SPI1_BITS (1u << 17)
typedef enum { GPIO_FUNC_SPI = 1, GPIO_FUNC_SIO = 5, GPIO_FUNC_NULL = 31 } gpio_function_t;
#define GPIO_OUT true
typedef enum { SPI_CPOL_0, SPI_CPOL_1 } spi_cpol_t;
typedef enum { SPI_CPHA_0, SPI_CPHA_1 } spi_cpha_t;
typedef enum { SPI_MSB_FIRST, SPI_LSB_FIRST } spi_order_t;
typedef struct { volatile uint32_t cr0, cr1, dr, sr, cpsr, imsc, ris, mis, icr, dmacr; } spi_hw_t;
typedef struct { spi_hw_t hw; unsigned id; } spi_inst_t;
extern spi_inst_t fake_spi[2];
#define spi0 (&fake_spi[0])
#define spi1 (&fake_spi[1])
typedef struct { volatile uint32_t reset; } resets_hw_t;
extern resets_hw_t fake_resets;
#define resets_hw (&fake_resets)
static inline spi_hw_t *spi_get_hw(spi_inst_t *spi) { return &spi->hw; }
bool spi_is_busy(spi_inst_t *spi);
bool spi_is_readable(spi_inst_t *spi);
uint spi_init(spi_inst_t *spi, uint baud);
void spi_deinit(spi_inst_t *spi);
uint spi_set_baudrate(spi_inst_t *spi, uint baud);
void spi_set_format(spi_inst_t *spi, uint bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t order);
int spi_write_blocking(spi_inst_t *spi, const uint8_t *data, size_t len);
void gpio_init(uint pin);
void gpio_deinit(uint pin);
void gpio_put(uint pin, bool value);
void gpio_set_dir(uint pin, bool output);
void gpio_set_function(uint pin, gpio_function_t fn);
gpio_function_t gpio_get_function(uint pin);
void sleep_ms(uint ms);
typedef enum { clk_peri } clock_index;
uint32_t clock_get_hz(clock_index clk);
#endif
