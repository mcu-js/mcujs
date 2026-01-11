# mcujs Agent Instructions

## Project Principles

- mcujs is a JavaScript runtime for microcontrollers in the same spirit as Node.js for servers.
- JerryScript is the current JavaScript engine, analogous to V8 in Node.js.
- The engine is swappable: keep runtime boundaries clean to allow future interpreter changes.
- The firmware targets Raspberry Pi Pico boards today, but should remain extensible to other MCUs with UF2 boot support.
- Favor composable, modular code so core systems can be replaced or extended without large rewrites.

## Development Cycle (Flash + Test Loop)

- Default workflow: `build.sh <board>` then flash the UF2 and run REPL tests.
- The runtime can enter UF2 mode via `.uf2!` or `board.enterUf2()` over serial.
- When automating, confirm device state:
  - UF2 mode if the `RPI-RP2` volume is mounted.
  - Runtime mode if a CDC serial device (e.g. `/dev/ttyACM*`) is present and responds to the REPL prompt.
- Prefer an automated loop that:
  - Builds firmware.
  - Reboots into UF2 mode from the REPL.
  - Copies the UF2 to the mounted volume.
  - Waits for CDC to reappear and runs REPL tests.
- Keep tooling minimal and cross-platform. Node/Bun-based harnesses are preferred over Python.
