# MCU.js platform backends

MCU.js selects one platform backend at CMake configure time:

```sh
cmake -DMCUJS_PLATFORM=rp2 -DBOARD=pico ..
```

`rp2` is the default while all release boards target RP2040 or RP2350.
Unsupported platform names fail during configuration rather than silently
falling back to RP2.

## Backend contract

A backend provides `platform/<name>/platform.cmake` with:

- `MCUJS_PLATFORM_MAIN_SOURCE`
- `MCUJS_PLATFORM_JERRY_PORT_SOURCE`
- `MCUJS_PLATFORM_USB_SOURCES`
- `MCUJS_PLATFORM_STORAGE_SOURCES`
- `MCUJS_PLATFORM_BOARD_SOURCE`
- `MCUJS_PLATFORM_BOOT_SOURCE`
- `MCUJS_PLATFORM_HARDWARE_BINDING_SOURCES`
- `MCUJS_PLATFORM_USB_BINDING_SOURCES`
- `MCUJS_PLATFORM_OPTIONAL_BINDING_SOURCES`
- `MCUJS_PLATFORM_INCLUDE_DIRS`

It also implements these CMake hooks:

- `mcujs_platform_sdk_init()`
- `mcujs_platform_configure_jerry(target, visibility)`
- `mcujs_platform_configure_host(target)`
- `mcujs_platform_configure_core(target)`
- `mcujs_platform_configure_bindings(target)`
- `mcujs_platform_configure_executable(target)`

The source groups are inserted into the same positions as the original RP2
sources. Their ordering is intentional: it preserves deterministic static
library member order and therefore reproducible firmware output.

## Shared contracts

Headers remain in their existing shared locations:

- `board/board.h` — board identity, timing, LED, filesystem geometry, loader entry
- `src/filesystem/storage.h` and `flash_ops.h` — storage contract
- `src/usb/*.h` — CDC, MSC, and HID contract
- `host/bindings/*.h` — JavaScript binding registration and shared helper APIs

A backend supplies the matching implementations. Platform-specific SDK
headers must not leak into shared C/H sources under `src/`, `host/`, or
`board/`; `scripts/verify-platform-boundaries.sh` enforces this.

## RP2 backend

`platform/rp2/` contains the existing RP2040/RP2350 implementations without
functional changes:

- Pico SDK import, link libraries, PIO generation, and UF2 output
- firmware entrypoint and JerryScript port
- board, boot flow, and BOOTSEL handling
- XIP flash/FatFs disk operations
- TinyUSB CDC, MSC, HID, and descriptors
- GPIO, timers, PWM, I2C, SPI, ADC, NeoPixel, board, keyboard, mouse, and DVI bindings

## ESP32-S3 backend

`platform/esp32/` is an ESP-IDF 5.3.2 project for the headless XIAO ESP32-S3
bring-up. ESP-IDF evaluates components through its own project flow rather than
the RP2 top-level source-list hooks, while shared engine capability flags keep
unimplemented subsystems out of the link without ESP32 branches in RP2 code.

Milestone 4 supports TinyUSB USB-OTG composite CDC+MSC, JerryScript, the REPL,
console, timers, board/process identity, reset, constrained GPIO, an
exclusive-owner wear-levelled FFAT filesystem, CommonJS modules, `/index.js`,
persistent boot-failure safe mode, and button-independent TinyUF2 crash-loop recovery. See
[`platform/esp32/README.md`](esp32/README.md) for pinned dependencies, build,
flash ownership, storage behavior, recovery, and hardware verification.
