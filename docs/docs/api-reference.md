---
sidebar_position: 7
---

# API Reference

## Runtime JavaScript

- Console, timers, and globals: see [Runtime JavaScript](./runtime-javascript.md)

## Modules

- Core modules: [Built-in Modules](./built-in-modules.md)
- Hardware modules: [Built-in Modules](./built-in-modules.md)

## Display and Image APIs

These display helpers are globals or built-in modules, depending on the board:

- `screen` (global): `screen.getBufferHandle()`, `screen.getByteOrder()`
- `graphics` (global): `graphics.getPointer(handle)`
- `image` (module): `byteOrder` option for `decodeJPEG`, `decodeBMP`, `drawJPEG`, `drawBMP`
- `DVI` (global on DVI boards): `DVI.getDrawBuffer()`, `DVI.swapAndShow()`

## MCU.js 0.2 portable contract

- API names, types, units, capabilities, and errors: [Portable API contract](./api-design/portable-api-contract.md)
- Updating 0.1 programs: [Migrating from MCU.js 0.1 to 0.2](./migration/0.2.md)
- Machine-readable contract and capability-manifest schema: [MCU.js 0.2 JSON Schema](/schemas/mcujs-portable-api-0.2.schema.json)
- Capability-gated nonportable compatibility methods, including RP `spi.writeBufferDMA()` and persistent `board.safeMode()`, are inventoried in the schema rather than silently treated as portable or removed.

The 0.2 pages define the implementation target. Check `board.apiVersion` and the running firmware's discovery surface before assuming a proposed API has reached a release.

## Notes

Refer to the [REPL](./glossary.md#repl) `.help` output or the runtime source for details while the API docs expand.
If something feels missing, check the source or open an issue so we can document it.

## Example map

- Want timers or console? Start at [Runtime JavaScript](./runtime-javascript.md)
- Want GPIO, files, or NeoPixels? Start at [Built-in Modules](./built-in-modules.md)

## Key terms

- [Runtime](./glossary.md#runtime)
- [CommonJS](./glossary.md#commonjs)
- [NeoPixel](./glossary.md#neopixel)
- [GPIO](./glossary.md#gpio)
- [ADC](./glossary.md#adc)
