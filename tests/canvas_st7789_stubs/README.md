# Focused native ST7789V3 backend tests

Run from the repository root:

```sh
sh tests/run-canvas-st7789-tests.sh
CANVAS_TEST_CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' sh tests/run-canvas-st7789-tests.sh
```

The script compiles `canvas_display_st7789.c` and `canvas_spi.c` as separate
translation units against small SDK stand-ins, with strict compiler warnings.
It requires a host C compiler with GNU-compatible linker `--wrap` support.
Binaries are temporary and removed on exit. No SDK installation or device
access is involved.

Tests invoke the real private display interface. GPIO/SPI stand-ins observe
command/data bytes, controller configuration during traffic, selected CS and
SPI destination, and count allocations for injected OOM cases. Coverage includes
Waveshare initialization and timing, horizontal/portrait offsets/MADCTL,
retained native-endian RGB565, bounded conversion including a partial chunk,
shared/independent buses, live-connection conflicts, release/reopen,
register/pin-function/reset restoration, busy/unread bus refusal, and cleanup
for initialization/frame write errors.

These are host behavior tests, **not** Pico SDK compilation or physical LCD
qualification. SDK fakes do not model timing, electrical effects, DMA, IRQ/core
concurrency, or every register side effect. The transport contract is serialized,
synchronous JS-task use with other bus consumers also completing their transfers
before returning. It restores CR0/CR1/CPSR/DMACR/IMSC and SCK/MOSI functions after
each transaction; an initially reset controller is returned to reset. Dedicated
CS/backlight GPIO outputs are parked high/low on release to avoid selecting a
closed panel; shared SPI pins/controller are not deinitialized by release.

Firmware integration must compile **both** new C files and link Pico SDK
`hardware_spi` plus the usual GPIO/clock/reset/stdlib dependencies. The caller
supplies a zero-initialized display struct and retains ownership of that struct.
There is one panel profile (Waveshare RP2350 LCD 1.47-A ST7789V3), not a claim
of compatibility with every ST7789 module or any I2C transport.
