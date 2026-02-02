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
