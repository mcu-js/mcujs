# Canvas 2D: first JavaScript slice

This is a small, standards-aligned **subset**, not a complete implementation of
HTML Canvas, `HTMLCanvasElement`, or `OffscreenCanvas`. It exposes ordinary Canvas
2D drawing calls and delegates rendering to the native integration of the external
**ctx.graphics C rasterizer**. There is no JavaScript rasterizer.

## Application interface

Install `lib/canvas.js` on the board as **`/canvas.js`**, so the runtime module
loader can resolve:

```js
var canvas = require('canvas').canvas;
```

In a browser, obtain a canvas element instead (for equivalent opaque behavior,
create its first 2D context with `{ alpha: false }`). Once `canvas` is obtained,
the following drawing code is identical in both environments:

```js
var ctx = canvas.getContext('2d');
ctx.fillStyle = '#369';
ctx.fillRect(8, 8, 48, 24);
ctx.beginPath();
ctx.moveTo(8, 40);
ctx.lineTo(56, 64);
ctx.lineTo(8, 64);
ctx.closePath();
ctx.strokeStyle = 'white';
ctx.lineWidth = 2;
ctx.stroke();
```

No device constructor, bus configuration, pointer, framebuffer, or explicit
`present()`/`flush()` call is exposed to application code. Installation and native
module availability are integration prerequisites, not application drawing steps.

## Supported surface

- Module export: `{ canvas }` only.
- `canvas.width` and `canvas.height`: read-only native hardware dimensions,
  currently **160 × 120**. Unlike browser canvas dimensions, these cannot resize
  the surface or reset its state.
- `canvas.getContext('2d')`: always returns the same context; other context IDs
  return `null`. Context options do not change the fixed opaque surface.
- `ctx.canvas`: read-only identity of that canvas.
- `ctx.getContextAttributes()`: a fresh `{ alpha: false }` object.
- `fillStyle` and `strokeStyle`: initially `'#000000'`; unsupported or malformed
  color strings leave the existing value unchanged.
- `lineWidth`: initially `1`; positive finite numbers are accepted. Zero,
  negatives, `NaN`, and infinities leave the previous value unchanged.
- `beginPath()`, `moveTo(x, y)`, `lineTo(x, y)`, `closePath()`,
  `rect(x, y, width, height)`, `fill()`, `fill('nonzero')`, `stroke()`.
- `fillRect(x, y, width, height)`, `strokeRect(x, y, width, height)`,
  `clearRect(x, y, width, height)`.
- `save()` and `restore()` save/restore fill color, stroke color, and line width
  in LIFO order. They **do not save the current path**. Restoring an empty stack
  is a no-op. Balance saves/restores; the stack consumes ordinary JavaScript heap.

### Paths and numeric inputs

Coordinates use JavaScript ToNumber-like conversion: numeric strings, booleans,
`null`, and objects with numeric conversion work. Conversions happen left to
right for all required coordinates before the finite check. If any converted
coordinate is `NaN` or infinite, the entire operation is ignored. Missing required
coordinate arguments throw `TypeError`; Symbols and BigInts also throw during
numeric conversion. Finite fractions and signed rectangle dimensions are passed
through, without integer rounding or conversion to unsigned coordinates.

`lineTo` on an empty path starts a subpath at its point; it does not draw from the
origin. `moveTo` starts a subpath. Closing an empty or one-point subpath is a no-op.
After closure, the current point is the subpath start. `rect` adds a closed
rectangle and leaves the current point at its origin. Native replay must preserve
these current-point semantics, including subsequent `lineTo` calls.

`fill` and `stroke` preserve the path. `fillRect`, `strokeRect`, and `clearRect`
never mutate it. `beginPath` alone discards the retained path. An empty path does
not call native draw. Zero-area fill/clear rectangles do nothing. A stroke
rectangle with one zero dimension is sent to the renderer as a degenerate rect;
with both dimensions zero it does nothing.

**Memory limit:** a retained path may hold at most **128 numeric entries**,
including opcodes. Move/line commands use three entries, close uses one, and rect
uses five. This is an entry count, not a byte budget; interpreter representation
and temporary arrays add overhead. A command that would exceed the limit throws
`RangeError` before changing either the path or its subpath bookkeeping.
`beginPath()` recovers capacity. Draw calls pass a snapshot of the command array;
native must support this same maximum and must not retain a pointer to JS storage.

### Native prototype limits and known gaps

This native backend is enabled only with `-DMCUJS_EXPERIMENTAL_CANVAS=ON`
on the PiZero. Standard firmware remains unchanged. Install this JS module as
`/canvas.js`; the private native module is not the application API.

- Coordinates and rectangle dimensions are restricted to `[-512, 512]`, and
  native line width to `(0, 640]`, to bound fixed-point edge arithmetic.
- General connected-path miter joins are **not Canvas-conformant** in ctx 0.1.18.
  Single-segment strokes and the separate rectangle path are the qualified demo
  scope, not evidence of correct general joins or `miterLimit`.
- RGB565 quantization and antialiasing differ from Chromium. Browser comparisons
  are evidence about the named drawing only, not whole-API conformance.
- Native OOM behavior and sustained display scheduling still need device testing.
  This is a local experimental port, not a release candidate.

### Colors

Supported, case-insensitive, with surrounding whitespace ignored:

- `#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`.
- The basic sixteen CSS names: `black`, `silver`, `gray`, `white`, `maroon`, `red`,
  `purple`, `fuchsia`, `green`, `lime`, `olive`, `yellow`, `navy`, `blue`, `teal`,
  and `aqua`; plus `transparent`.
- A limited legacy comma syntax: `rgb(255, 0, 153)`, `rgba(51, 102, 153, .8)`,
  `rgb(100%, 0%, 60%)`, `rgba(100%, 0%, 60%, 25%)`. Channels must be either all
  decimal numbers or all percentages; alpha can be a decimal or percentage.
  Channels clamp to their valid range and RGB channels round to 8-bit values.

Opaque getters serialize as lowercase six-digit hex. Other getters serialize as
`rgba(r, g, b, a)` with alpha rounded to three decimal places. Internal/native
alpha retains the parsed precision, including exact hex-byte fractions, and
save/restore does not reparse the rounded string. Reassigning a serialized alpha
style can therefore change its precision slightly.

This is not a full CSS color parser: other named colors, `currentColor`, HSL,
modern space/slash syntax, scientific notation, color spaces, gradients, and
patterns are unsupported. Unsupported strings are ignored, not approximated.

### Opaque clear and rendering boundary

Because this surface has `alpha: false`, `clearRect` submits an **opaque black
rectangle** using native fill, independent of current styles and without changing
those styles. Its pixel coverage and fractional-edge behavior depend on native
rasterization and have not been verified by the JavaScript tests.

The fixed native module is `require('mcujs:canvas-native')`:

```text
width = 160
height = 120
draw(flatCommands, modeString, rgbaArray, lineWidth) -> undefined
```

`modeString` is `'fill'` or `'stroke'`; `rgbaArray` contains four numbers in
`[0, 1]`. The flat command protocol is:

| Opcode | Following values | Meaning |
| --- | --- | --- |
| 1 | x, y | moveTo |
| 2 | x, y | lineTo |
| 3 | none | closePath |
| 4 | x, y, width, height | rect |

Every native draw starts a fresh native path from the supplied list and
**automatically presents**. The JavaScript module owns all retained path/state.
The external rasterizer/native adapter is responsible for nonzero fill, implicit
closure for fill without changing the JS path, default butt caps/miter joins,
antialiasing, alpha blending onto the opaque surface, bounds clipping, and actual
display presentation. In particular, native integration must verify signed and
degenerate rectangles and current-point behavior after close/rect.

## Explicit exclusions

There is no DOM, event handling, canvas resizing, font/text API, image API,
`Path2D`, transforms, curves/arcs, clipping API, gradients/patterns, pixel access,
compositing controls, global alpha, or configurable caps/joins/dashes. Unsupported
methods are absent. Adding arbitrary properties to the plain JS context does not
implement those features. This is not a WebIDL-branded browser object, and full
browser method receiver/overload behavior is not implemented.

Only the nonzero fill rule is supported. `fill('evenodd')` explicitly throws
`RangeError` rather than silently rendering with the wrong rule; invalid fill-rule
strings throw `TypeError`. This evenodd rejection is a deliberate subset limitation,
not full Canvas conformance. Path2D overloads are not supported.

## Verification and evidence boundary

Run on the host, without hardware or a native renderer:

```sh
node --check lib/canvas.js
node --check tests/canvas-2d.test.js
node --test tests/canvas-2d.test.js
```

The Node tests execute the real module in a `vm` with only the fixed native module
mocked. They use independently specified literal opcodes, modes, colors, and
widths to check the public API and native handoff. Red runs observed missing
module/method failures and incorrect style/property behavior before the matching
implementation was added; the completed suite passes.

**These are JavaScript contract tests, not native rasterizer, JerryScript, browser
pixel-equivalence, firmware, or hardware passes.** Native rendering verification,
module installation, and board execution belong to the integration stage. No
hardware, Docker, deployment, or native code was exercised by this slice.
