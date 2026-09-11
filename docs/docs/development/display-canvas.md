# Per-display Canvas (experimental)

## Start with the configured display

Use `devices` to open the board's configured display and `display.canvas` to
draw. Do not mix this handle with `screen`, `graphics`, direct display pins or
controller calls. Check the installed firmware: an onboard panel does not mean
that its Canvas adapter is enabled.

```js
var devices = require('devices');
if (!devices.display) throw new Error('No configured display');
var display = devices.display.open();
var canvas = display.canvas;
var ctx = canvas.getContext('2d');
ctx.fillStyle = '#00eeee';
ctx.fillRect(10, 10, 40, 40);
// The runtime presents when this JavaScript task returns.
setTimeout(function () { display.close(); }, 20000);
```

Start with `examples/waveshare-lcd-1.47/display-canvas-demo.js`. Despite its
historical folder name, the example uses the configured canvas dimensions, not
LCD pins or a board ID. It draws color bars, a cyan border and center square,
and two crossing lines. After 20 seconds it closes the display and prints
`Demo complete!`. Run the file again after completion; it opens a fresh handle.
It is a runnable example, no longer an exported drawing-helper function.

To try it without changing startup files, enter `.multiline` in the REPL, paste
the file, then enter `.end` on a new line. Do not add a filename to `.multiline`
for this RAM-only test. Alternatively, copy it to a new file on the internal
USB volume, eject that volume, and use `.run /app/<filename>.js`. Do not replace
`/app/index.js` just to try a drawing. A busy display must be closed by its
current owner first; the example does not seize another program's handle.

Closing an LCD turns its backlight off. If you need to present and close in one
synchronous task, call `display.present()` **before** `display.close()`; close
otherwise discards the pending frame. `present()` belongs to the display handle,
not to the context. E-paper can retain an image after close and can take much
longer to refresh. See [configured handle lifetime](device-display-handles.md).

## Explicit external LCD setup

For external wiring, setup may use the existing connector below. This is not the
default application path. Keep controller profiles and pins out of drawing code.
The public drawing object is still the stable, read-only `display.canvas`:

```js
var ST7789 = require('displays/st7789');
var display = ST7789.connect(); // built-in LCD wiring on a supported RP2350 LCD board
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

Waveshare panel profiles (host-tested; physical qualification is separate):

- `waveshare-1.47`: LCD 1.47-A / ST7789V3, landscape 320x172 or portrait 172x320.
- `waveshare-1.69`: Touch LCD 1.69 / ST7789V2, **portrait default 240x280**, or landscape 280x240 with `horizontal: true`.
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

Older code may use `require('canvas').display`, which lazily opens and caches the board's default display;
`require('canvas').canvas` remains an alias for its canvas. On PiZero the default
is DVI; on a supported RP2350 LCD board it is the onboard LCD. Requiring either software
module alone does not open the hardware.

Prefer `devices.display.open()` for new applications. Do not combine it with
the explicit connector or cached convenience: a second conflicting connection fails. Every successfully
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

The RP adapters cover PiZero and RP2350 LCD 1.47-A, Touch LCD 1.69 and 2.8.
Separate ESP adapters cover Sticky and ePaper 1.54 V2; they are not LCD profiles.
Availability depends on the compiled experimental options and configured adapter.
The RP2350 uses its own SDK-derived linker layout with a separately reserved
Core 0 stack; it must not inherit the RP2040 boot/image layout.
The Canvas drawing subset limits in `canvas-first-slice.md` still apply; that
page's original HDMI prototype and acceptance sections are historical evidence.

## Touch LCD 1.69 vendor reference

Sources: [manufacturer wiki](https://www.waveshare.com/wiki/RP2350-Touch-LCD-1.69)
and [official demo ZIP](https://files.waveshare.com/wiki/RP2350-Touch-LCD-1.69/RP2350-Touch-LCD-1.69-Code.zip).
`C/lib/Config/DEV_Config.h` specifies SPI1, SCK10, MOSI11, CS9, DC8,
reset13, BL25. Python demo explicitly selects mode 0; C uses SDK defaults.
Canvas retains its conservative 37.5MHz limit (vendor C requests 40MHz).
`C/lib/LCD/LCD_1in69.h` specifies 240x280; `LCD_1in69.c` adds Y20 in
portrait, X20 in landscape. RGB565 is high-byte first: its 16-bit write helper
and `C/lib/GUI/GUI_Paint.c` explicitly serialize that order.

V2 has its own B2/B7/C3/C6/gamma values and E4 25 00 00; it does not use
T3's B0 00 E8 little-endian RAMCTRL or SPI mode 3. Reset is high/low/high,
100ms each, followed by register setup, inversion, sleep-out, 120ms wait and
screen-on. Canvas adds 20ms after screen-on and defers backlight to presentation.
The vendor horizontal MADCTL 78 is overwritten by InitReg's 00: Canvas applies
rotation once, using 70 (axis/scan bits retained, BGR bit cleared to preserve
RGB colors) and 00 in portrait. Landscape therefore needs physical visual
qualification rather than claiming the vendor's broken sequence works unchanged.
One native RGB565 buffer is retained; no runtime GPIO/bus capabilities are added.
Normal builds still require `MCUJS_EXPERIMENTAL_CANVAS=ON` to include Canvas.

## Verification boundaries

JS tests exercise per-instance state, default laziness, connector forwarding and
close. Host Jerry tests execute the packaged module and real rasterizer into two
independent buffers. SPI fakes test initialization, wire bytes, offsets,
conflicts, bus restoration, allocation/transfer failures and release/reopen.
These host tests do not establish physical LCD output. That requires an exact
firmware build, on-board execution and visual inspection of the real panel.

### Bounded example proof — September 11, 2026

The exact runnable example was sent through RAM-only `.multiline` on the 1.47-A
running `0.1.0+7b4f401` and the 1.69 running `0.1.0+24b1f5b`. It ran twice on
each without a reset. Each run printed its asynchronous completion message and
returned `devices.display.state` to `idle`; final `.info` replies kept the same
build IDs and host-owned filesystem state. No firmware or board files changed.

Silent C920 stills showed the crossing lines, center marker and cyan border on
both panels, and dark LCD backlights after close. The 1.69's bottom edge was
partly outside the frame and the bright bars were overexposed. This proves
visible static drawing and repeat/close behavior, **not** pixel-perfect color,
every edge, touch, sustained animation, DVI or e-paper refresh qualification.

## Legacy behavior audit and removal gate

`screen`, `graphics` and public `image` are deprecated for new application code.
They still exist on firmware that enables them. The removal point is **before
v1.0**, after each useful supported behavior below has a tested replacement or
an explicit retirement decision. This documentation/example slice does not
remove bindings, decoders, registrations, capability/schema entries or tests.
Do not add compatibility shims or start new examples on the deprecated APIs.
Track the remaining migration in [issue #19](https://github.com/mcu-js/mcujs/issues/19).

This is a source audit, not a claim of cross-board parity:

| Existing behavior and source | Canonical status | Remaining work |
| --- | --- | --- |
| `graphics.fill`, `fillRect`, `setPixel`; `screen.fill`, `fillRect`, `drawLine` (`host/bindings/graphics.c`, `screen.c`) | Solid fills, rectangles and single-segment lines are covered by Canvas. A 1-by-1 `fillRect` is the drawing equivalent of an integer pixel write. | Keep public Canvas/native tests; current camera proof covers only the named static drawing, not every raster edge. |
| `screen.drawCircle` / `fillCircle`, plus example-local circle rasterizers | No Canvas arc/curve API exists. | Needs a bounded, tested replacement or a documented retirement decision. Do not silently drop circles from maintained demos. |
| `screen.drawText`: built-in 5-by-7 bitmap glyphs, integer scale and newline handling | No Canvas text/font API exists. | Preserve useful readable text behavior before removing this binding. Font metrics, clipping, character coverage and LCD/e-paper readability need explicit tests. |
| `image.info`, `decodeJPEG`, `drawJPEG` (`host/bindings/image.c`, picojpeg baseline decoder) | No general Canvas image decoder or `drawImage` exists. | Preserve the decoder implementation; separate bounded decoding from display ownership. JPEG is not replaced by the BMP example. |
| Legacy BMP decode/draw: 16-bit RGB565, 24-bit BGR, 32-bit BGRA (alpha ignored) | `examples/portable/sd-bmp/show.js` reads raw `fs` bytes and draws rows with Canvas; only 24-bit uncompressed BI_RGB with a 40-byte DIB is covered. | 16/32-bit BMP and image-info behavior remain unported. The [binary-assets proof](binary-file-assets.md) is limited to its documented format and board. |
| Raw buffer handles/pointers, byte-order options, `screen.show`, direct SPI/DVI transfers | Applications use `display.canvas`; the adapter owns buffers and wire order. Automatic task-end presentation or `display.present()` handles pending frames. | Retire raw buffer/pin/byte-swap plumbing from application examples after their useful drawing is migrated; do not expose it as a new Canvas API. |
| LCD retained RGB565 frame; DVI frame-boundary publication (`platform/rp2/bindings/canvas_display_*.c`) | Already behind the display adapter. LCD close parks the backlight off; DVI close can retain the last video frame. | Keep lifecycle distinctions documented. This LCD run does not requalify DVI or long-running refresh. |
| Sticky full-frame refresh; ePaper V2 full refresh and gated partial-refresh policy (`platform/esp32/main/canvas_display_*.c`) | Private adapter policy, including BUSY waits and panel sleep/power sequencing. No portable partial-refresh selector is promised. | Physical ghosting, repeat refresh and sleep/reopen qualification are separate from LCD proof. Sticky is not REPL-ready on this bench; the V2 board is absent. Do not change bootloaders to complete this audit. |

### Caller migration order

- **Start here:** the runnable `display-canvas-demo.js` above, plus the existing
  `examples/portable/device-display/` and `examples/portable/sd-bmp/` consumers.
  The latter two retain their own documented setup and qualification limits.
- **Still to migrate:** legacy scripts in `examples/waveshare-lcd-1.28/`,
  `waveshare-lcd-1.47/`, `waveshare-lcd-1.69/` and
  `waveshare_rp2040_pizero/` use globals as well as `require()` imports. Searching
  only `require('screen')` misses these callers. Text, circles, images and
  high-rate animation must not be marked migrated from this static example.
- **Remove last:** legacy setup helpers `drivers/dvi.js`, `drivers/st7789.js`,
  `lib/waveshare_rp2040_pizero/display.js`, and board-folder `screen.js` helpers,
  then native public bindings, registrations, schema/capability entries and
  legacy-only tests. Preserve useful internal decoder/rasterizer code and move
  behavior tests to the canonical interface first.
