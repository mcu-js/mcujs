---
sidebar_position: 11
---

# Architecture

mcujs is organized around replaceable boundaries: the JavaScript engine, host runtime, native bindings, firmware services, board support, and examples. Keep changes inside the narrowest layer that owns the behavior.

## Layers

| Layer | Paths | Owns |
| --- | --- | --- |
| JavaScript engine adapter | `javascript/`, `host/engine.*`, `host/jerry_port.c` | JerryScript setup, evaluation, engine-facing helpers |
| Module system | `host/module_loader.*`, `host/bindings/require.c` | CommonJS resolution, JSON loading, module cache |
| Native bindings | `host/bindings/` | JavaScript APIs for GPIO, PWM, I2C, SPI, ADC, filesystem, HID, display, process, board helpers |
| Firmware services | `src/` | boot flow, REPL, USB CDC/MSC, filesystem integration, startup |
| Board support | `board/<board-id>/` | pin maps, flash size, feature flags, memory maps, Pico SDK board selection |
| Userland examples | `examples/` | reusable JavaScript demos and board-local drivers |
| Tooling | `build.sh`, `docker-entrypoint.sh`, `scripts/` | deterministic builds, release packaging, source verification |

## Engine boundary

JerryScript is the current engine, but it should stay replaceable. New runtime behavior should not require board code to know JerryScript details. Prefer these boundaries:

- Engine setup and raw `jerry_value_t` handling stay in `javascript/`, `host/engine.*`, and `host/bindings/`.
- Firmware services expose C APIs that bindings can call.
- JavaScript-facing APIs are documented as runtime contracts, not JerryScript contracts.
- Dependency versions exposed through `process.versions` must match the Dockerfile and CMake compile definitions.

## Board boundary

Board configuration belongs in `board/<board-id>/`. Build and release tooling discovers supported board IDs from `scripts/lib/boards.sh`, then CMake includes `board/<board-id>/board_config.cmake`.

Keep board changes explicit:

- Put pin and feature constants in `board_config.h`.
- Put CMake board selection and compile definitions in `board_config.cmake`.
- Add a memory map only when flash layout needs it.
- Document examples and pin notes in [Hardware and Boards](./hardware-boards.md).

## Serial boundary

USB CDC serial output must use `\r\n`. That applies to REPL output, errors, command responses, and any new native code that writes text to serial.

## Example boundary

Examples should preserve REPL recovery:

- Use `setInterval()` and `setTimeout()` for demos and animation loops.
- Avoid indefinite loops in top-level example code.
- Print progress with `console.log()`.
- Stop long-running demos after a fixed duration when possible.

## Release boundary

Release artifacts are produced by scripts, not by hand:

- `scripts/verify-release.sh` checks source metadata and docs coverage.
- `./build.sh all --clean` builds every supported board.
- `scripts/package-release.sh` writes checksums and a manifest.
- `scripts/release.sh` runs the full release flow.

This keeps the public release process reproducible and easy to review.
