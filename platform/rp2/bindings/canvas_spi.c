#include "canvas_spi.h"
#include "hardware/clocks.h"
#include "hardware/resets.h"
#include "hardware/regs/resets.h"
#include "hardware/regs/spi.h"
#include <limits.h>

bool canvas_spi_configure(canvas_spi_t *bus, int spi, int sck, int mosi, int baudrate) {
    if (!bus || spi < 0 || spi > 1 || sck < 0 || mosi < 0 ||
        sck >= NUM_BANK0_GPIOS || mosi >= NUM_BANK0_GPIOS ||
        (sck & 3) != 2 || (mosi & 3) != 3 ||
        ((sck >> 3) & 1) != spi || ((mosi >> 3) & 1) != spi || baudrate <= 0)
        return false;
    /* Avoid the SDK's assertion on an unrepresentably low baud rate. */
    if ((uint64_t)(unsigned)baudrate * 254u * 256u < clock_get_hz(clk_peri))
        return false;
    *bus = (canvas_spi_t){.instance = spi ? spi1 : spi0,
        .sck = sck, .mosi = mosi, .baudrate = (uint32_t)baudrate};
    return true;
}

bool canvas_spi_begin(canvas_spi_t *bus) {
    if (!bus || bus->active || bus->mode > 3) return false;
    spi_inst_t *spi = bus->instance;
    uint32_t reset_bit = spi == spi0 ? RESETS_RESET_SPI0_BITS : RESETS_RESET_SPI1_BITS;
    bus->was_reset = (resets_hw->reset & reset_bit) != 0;
    if (!bus->was_reset && (spi_is_busy(spi) || spi_is_readable(spi))) return false;
    /* spi_init() resets the controller: never call it on an existing bus. */
    if (bus->was_reset) spi_init(spi, bus->baudrate);
    spi_hw_t *hw = spi_get_hw(spi);
    bus->cr0 = hw->cr0;
    bus->cr1 = hw->cr1;
    bus->cpsr = hw->cpsr;
    bus->dmacr = hw->dmacr;
    bus->imsc = hw->imsc;
    bus->sck_function = gpio_get_function((uint)bus->sck);
    bus->mosi_function = gpio_get_function((uint)bus->mosi);
    hw->cr1 = 0; /* Disabled master, no slave/loopback mode. */
    hw->dmacr = 0;
    hw->imsc = 0;
    hw->cr0 = 0; /* Motorola SPI, including clearing any previous frame format. */
    spi_set_baudrate(spi, bus->baudrate);
    spi_set_format(spi, 8, (spi_cpol_t)((bus->mode >> 1) & 1),
                   (spi_cpha_t)(bus->mode & 1), SPI_MSB_FIRST);
    gpio_set_function((uint)bus->sck, GPIO_FUNC_SPI);
    gpio_set_function((uint)bus->mosi, GPIO_FUNC_SPI);
    hw->cr1 = SPI_SSPCR1_SSE_BITS;
    bus->active = true;
    return true;
}

bool canvas_spi_write(canvas_spi_t *bus, const uint8_t *bytes, size_t count) {
    return bus && bus->active && count <= INT_MAX &&
        spi_write_blocking(bus->instance, bytes, count) == (int)count;
}

void canvas_spi_end(canvas_spi_t *bus) {
    if (!bus || !bus->active) return;
    /* spi_write_blocking completes clocks and drains its received bytes. */
    spi_hw_t *hw = spi_get_hw(bus->instance);
    hw->cr1 = 0;
    hw->cr0 = bus->cr0;
    hw->cpsr = bus->cpsr;
    hw->dmacr = bus->dmacr;
    hw->imsc = bus->imsc;
    gpio_set_function((uint)bus->sck, bus->sck_function);
    gpio_set_function((uint)bus->mosi, bus->mosi_function);
    hw->cr1 = bus->cr1;
    if (bus->was_reset) spi_deinit(bus->instance);
    bus->active = false;
}
