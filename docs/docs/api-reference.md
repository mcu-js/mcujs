---
sidebar_position: 7
---

# API Reference

## Runtime JavaScript

- Console, timers, and globals: see [Runtime JavaScript](./runtime-javascript.md)

## Modules

- Core modules: [Built-in Modules](./built-in-modules.md)
- Hardware modules: [Built-in Modules](./built-in-modules.md)

## Display and Canvas

New drawing code uses the configured device handle:

```js
var display = require('devices').display.open();
var canvas = display.canvas;
var ctx = canvas.getContext('2d');
```

First check that `require('devices').display` exists in the installed firmware.
Keep one owner, draw through its canvas, and close the handle when finished.
See [Canvas setup, runnable example and legacy behavior audit](./development/display-canvas.md)
and [display lifetime](./development/device-display-handles.md). Use `fs` to read
asset bytes independently of `/app` or optional `/sd`; the bounded
[24-bit BMP example](./development/binary-file-assets.md) is not a general image API.

## Deprecated display and image APIs

`screen`, `graphics` and public `image` are deprecated for new code and will be
removed before v1.0 once useful behaviors are replaced or explicitly retired.
They have **not** been removed by the documentation/example migration. Do not
mix them with Canvas or add shims; the audit above records remaining text,
image, circle and refresh gaps.

These display helpers are globals or built-in modules, depending on the board:

- `screen` (global): `screen.getBufferHandle()`, `screen.getByteOrder()`
- `graphics` (global): `graphics.getPointer(handle)`
- `image` (module): `byteOrder` option for `decodeJPEG`, `decodeBMP`, `drawJPEG`, `drawBMP`
- `DVI` (global on DVI boards): `DVI.getDrawBuffer()`, `DVI.swapAndShow()`

Feature-detect `image` with `require('mcujs:module').has('image')` and inspect
`board.capability('image')` for the exact current firmware methods, decoders, and
input ceiling. Image decoding is independent from graphics-buffer allocation,
screen/display output, DVI, and onboard-device inventory. The current RP module
is an explicitly nonportable compatibility surface; portable image API
normalization remains deferred to a later 0.x release.

Likewise, feature-detect `graphics`, `screen`, and `dvi` through
`require('mcujs:module').has(name)` and inspect their separate capability
descriptors for the exact current-image methods and limits. An onboard LCD entry
in `board.devices.display` is physical inventory, not proof that any one of
those modules exists. These RP compatibility surfaces are documented for honest
discovery of older code only; new applications use `devices` + `display.canvas`.

## MCU.js 0.2 portable contract

- API names, types, units, capabilities, and errors: [Portable API contract](./api-design/portable-api-contract.md)
- Error classes, stable codes, and diagnostics: [Error reference](./api-design/errors.md)
- Updating 0.1 programs: [Migrating from MCU.js 0.1 to 0.2](./migration/0.2.md)
- Machine-readable contract and capability-manifest schema: [MCU.js 0.2 JSON Schema](/schemas/mcujs-portable-api-0.2.schema.json)
- Capability-gated nonportable compatibility methods, including RP `spi.writeBufferDMA()` and persistent `board.safeMode()`, are inventoried in the schema rather than silently treated as portable or removed.

The 0.2 pages define the implementation target. Check `board.apiVersion` and the running firmware's discovery surface before assuming a proposed API has reached a release.

## Notes

Refer to the [REPL](./glossary.md#repl) `.help` output or the runtime source for details while the API docs expand.
If something feels missing, check the source or open an issue so we can document it.

## Example map

- Want timers or console? Start at [Runtime JavaScript](./runtime-javascript.md)
- Want GPIO, PWM, files, or NeoPixels? Start at [Built-in Modules](./built-in-modules.md), including the [PWM contract](./built-in-modules.md#pwm-contract)
- Measuring PWM on supported boards? Follow the [non-destructive PWM waveform protocol](./development/pwm-waveform-protocol.md)

## Key terms

- [Runtime](./glossary.md#runtime)
- [CommonJS](./glossary.md#commonjs)
- [NeoPixel](./glossary.md#neopixel)
- [GPIO](./glossary.md#gpio)
- [ADC](./glossary.md#adc)
