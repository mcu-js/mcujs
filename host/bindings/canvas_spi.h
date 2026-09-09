/* Private synchronous SPI transaction seam; no LCD protocol lives here.
 * Called only from the serialized JS task, never IRQ/core/DMA callbacks.
 * Other bus consumers must likewise finish transfers before returning.
 */
#ifndef MCUJS_CANVAS_SPI_H
#define MCUJS_CANVAS_SPI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hardware/spi.h"
#include "hardware/gpio.h"

typedef struct {
    spi_inst_t *instance;
    int sck, mosi;
    uint32_t baudrate;
    unsigned mode;
    uint32_t cr0, cr1, cpsr, dmacr, imsc;
    gpio_function_t sck_function, mosi_function;
    bool was_reset, active;
} canvas_spi_t;

/* Validation/configuration has no hardware side effects. */
bool canvas_spi_configure(canvas_spi_t *bus, int spi, int sck, int mosi, int baudrate);
/* Save and borrow an idle controller. The caller owns CS and must deselect
 * before end. Busy/unread data is rejected rather than discarded. */
bool canvas_spi_begin(canvas_spi_t *bus);
bool canvas_spi_write(canvas_spi_t *bus, const uint8_t *bytes, size_t count);
void canvas_spi_end(canvas_spi_t *bus);
#endif
