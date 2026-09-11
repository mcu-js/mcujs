# Canvas 2D: first JavaScript slice

For the current per-display connection and lifetime API, see [display.canvas](display-canvas.md).
The native-contract and qualification sections below record the original HDMI slice.

This is a small, standards-aligned **subset**, not a complete implementation of
HTML Canvas, `HTMLCanvasElement`, or `OffscreenCanvas`. It exposes ordinary Canvas
2D drawing calls and delegates rendering to the native integration of the external
**ctx.graphics C rasterizer**. There is no JavaScript rasterizer.

## Application interface

Experimental Canvas firmware bundles `lib/canvas.js` into the UF2. No library
file installation or filesystem access is needed for:

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

Drawing code needs no bus configuration, pointer or framebuffer access.
The later [configured display handle slice](./device-display-handles.md) adds
explicit ownership and optional `display.present()` before release. Automatic
main-loop presentation remains available; neither `present()` nor `flush()` is
part of the Canvas 2D context. Installation and native module availability are
integration prerequisites, not application drawing steps.

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
on the PiZero. Standard firmware has neither the public Canvas module nor its
private native backend. The private module is not the application API.
The ctx.graphics copyright and permission notice is in `third_party/ctx/LICENSE`.

- Coordinates and rectangle dimensions are restricted to `[-512, 512]`, and
  native line width to `(0, 640]`, to bound fixed-point edge arithmetic.
- General connected-path miter joins are **not Canvas-conformant** in ctx 0.1.18.
  Single-segment strokes and the separate rectangle path are the qualified demo
  scope, not evidence of correct general joins or `miterLimit`.
- RGB565 quantization and antialiasing differ from Chromium. Browser comparisons
  are evidence about the named drawing only, not whole-API conformance.
- Native OOM behavior, repeated stop/start, long-running updates, and general
  Canvas conformance remain unqualified. This is not a release candidate.
- The experimental RP2040 linker layout reserves a separate 12 KiB Core 0
  stack in main RAM and explicitly retains a 2 KiB Core 1 stack. Do not infer
  stack capacity from `__StackTop - __StackLimit`: use `__StackBottom` and verify
  heap, both stacks, and TMDS scratch code are disjoint. Pico SDK otherwise
  defaults Core 1's stack size to `PICO_STACK_SIZE`.
- Scanline production runs in the native Core 1 DMA callback. Frame publication
  waits until the previous source frame has been copied, not for JavaScript to
  keep feeding scanlines.

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

Every native draw starts a fresh native path from the supplied list. Drawing
within one JavaScript task accumulates in the existing back buffer; the runtime
**automatically presents after the task/timer callbacks finish**. This prevents
intermediate clears from appearing as blank video frames. No application
`present()` or `flush()` call is required. The JavaScript module owns retained
path/state.
The external rasterizer/native adapter is responsible for nonzero fill, implicit
closure for fill without changing the JS path, default butt caps/miter joins,
antialiasing, alpha blending onto the opaque surface, bounds clipping, and actual
display presentation. In particular, native integration must verify signed and
degenerate rectangles and current-point behavior after close/rect.

## Explicit exclusions

The original slice had no text API; see the current [bounded bitmap text](display-canvas.md#bounded-bitmap-text) contract.
There is no DOM, canvas resizing, image API,
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

## On-device qualification

Diagnostic firmware **0.1.0+20b9f46** passed a bounded PiZero hardware run:
`getContext('2d')`, fill, single-segment stroke, and loading the complete example
while DVI was already running all returned successfully. ShadowCast captured
the cyan border, crossing white/green diagonals, and red/yellow squares. The
same drawing function was compared visually with the Chromium reference;
geometry agrees, while RGB565/capture colors and antialiasing differ.
The demo was stopped and `.info` remained responsive. No autorun was added.

The UF2 payload was checked against the compiled binary. The linker check passed
with non-overlapping heap, main stack, Core 1 stack, and TMDS scratch code.
This qualifies this drawing, not all methods, failure paths, or sustained use.

Check a compiled experimental image with:

```sh
python3 tests/check_canvas_stack_layout.py path/to/firmware.elf.map
```

## Bounded animation and packaged-module acceptance

Firmware **0.1.0+bd2e87c** completed the one-minute `canvas-animation.js` run:
988 drawing updates in 60,016 ms. Seven in-run post-collection samples each
reported 16,208 bytes of JS heap use and 11,708 bytes of native allocations.
This is sampled bounded-run evidence, not a general leak-freedom claim; the
post-restart sample includes an additional retained controller object.

In the 50-second interior of the actual ShadowCast recording, all 1,500 captured
frames contained both the moving square and following line. The preceding
per-operation presentation implementation exposed 500 blank intermediate frames
in the same-sized sample. A host regression requires no presentation during the
synchronous drawing sequence and one complete presentation when the task ends.

The animation controller stopped at its deadline, restarted, stopped manually,
and stayed stopped. DVI was stopped at the end and `.info` stayed responsive.
Restarting DVI itself after shutting down its hardware worker remains unqualified.
No autorun was installed.

`require('canvas')` succeeded while the device filesystem was still owned by the
USB host, proving that the library is firmware-packaged. Non-Canvas Pico and
PiZero builds compiled successfully with Canvas factory/renderer symbols and
module strings absent. Experimental heap instrumentation and recovery markers
are gated off in those standard builds.

These are local experimental builds, not release artifacts; filesystem placement
still follows firmware size, so upgrades must preserve files when that boundary
moves. No release or long-duration stability claim is made.
