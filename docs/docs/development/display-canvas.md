# Per-display Canvas (experimental)

The public object is a display with a stable, read-only `display.canvas`:

```js
var ST7789 = require('displays/st7789');
var display = ST7789.connect(); // built-in LCD wiring on either supported RP2350 LCD board
var ctx = display.canvas.getContext('2d');
ctx.fillStyle = '#00eeee';
ctx.fillRect(10, 10, 40, 40);
```

`connect()` uses the qualified panel profile and board defaults. For an explicit
connection, `spi` is the numeric bus index used by MCU.js, not a new bus object:

```js
var display = ST7789.connect({
  profile: 'waveshare-1.47',
  spi: 0, sck: 18, mosi: 19, cs: 17, dc: 16,
  reset: 20, backlight: 21, horizontal: true, baudrate: 37500000
});
```

Two Waveshare panel profiles are qualified:

- `waveshare-1.47`: LCD 1.47-A / ST7789V3, landscape 320x172 or portrait 172x320.
- `waveshare-2.8`: Touch LCD 2.8 / ST7789T3, landscape 320x240 or portrait 240x320.

Use `horizontal: false` for portrait. Optional width and height must match the
selected profile and orientation. The board selects its onboard profile by
default; explicit external connections can select `profile` separately from pins.
The 2.8-inch board defaults to SPI1, SCK 10, MOSI 11, CS 13, DC 14, reset 15,
backlight 16. SPI mode, panel initialization, offsets and wire byte order stay
inside the backend, not in application drawing code.

Other panel profiles, arbitrary dimensions, I2C drivers and bus objects are
**not implemented**. Unknown options (including `i2c`) fail rather than silently
using the onboard SPI connection. The initial 2.8-inch port deliberately withholds
unqualified public GPIO/bus modules; its private display transport is independent
of those public API capabilities.

## Defaults and lifetime

`require('canvas').display` lazily opens and caches the board's default display;
`require('canvas').canvas` remains an alias for its canvas. On PiZero the default
is DVI; on either supported RP2350 LCD board it is the onboard LCD. Requiring either software
module alone does not open the hardware.

Use either the explicit connector or the default convenience for a physical
screen, not both: a second conflicting connection fails. Every successfully
opened display owns independent styles, paths, save/restore state and pixels.
A display's `close()` is idempotent, releases its Canvas resources and rejects
later drawing through that canvas. Close is not a universal power-off API:
LCD release parks its backlight off; DVI releases Canvas ownership while leaving
its video worker displaying the last frame. Closing a default display does not
automatically replace its cached object. Use a new explicit connection to reopen.

## Internal boundaries

- `lib/canvas.js`: per-display Canvas state and standard-name drawing methods.
- `canvas_native.c`: validated native instances with GC-managed lifetimes.
- `canvas_renderer.c`: one shared ctx.graphics rasterizer into native-endian RGB565.
- `canvas_display.h`: acquire / present / release per-surface interface.
- `canvas_display_dvi.c`: DVI buffer preservation and frame-boundary publication.
- `canvas_display_st7789.c`: panel commands, offsets and retained RGB565 pixels.
- `canvas_spi.c`: synchronous borrowing/restoration of SPI registers and data-pin
  multiplexing. Shared-bus LCD connections need the same SCK/MOSI and independent
  control pins. Busy or unread buses are refused. This is serialized JS-task
  access, not an interrupt-safe global pin/peripheral ownership manager.

Presentation runs after JavaScript callbacks finish. No application `show()`,
`flush()`, framebuffer handles or byte swapping is required. The LCD converts to
wire byte order in bounded chunks; it never swaps or duplicates the whole frame.
Backlight enables only after a successful initial presentation.

The experimental build supports PiZero, RP2350 LCD 1.47-A and RP2350 Touch LCD 2.8 only. The RP2350 uses
its own SDK-derived linker layout with a separately reserved Core 0 stack; it
must not inherit the RP2040 boot/image layout. Normal builds remain gated off.
The Canvas subset limits documented in `canvas-first-slice.md` still apply.

## Verification boundaries

JS tests exercise per-instance state, default laziness, connector forwarding and
close. Host Jerry tests execute the packaged module and real rasterizer into two
independent buffers. SPI fakes test initialization, wire bytes, offsets,
conflicts, bus restoration, allocation/transfer failures and release/reopen.
These host tests do not establish physical LCD output. That requires an exact
firmware build, on-board execution and visual inspection of the real panel.
