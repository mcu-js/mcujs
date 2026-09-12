# Portable image examples

Use these files instead of the old board-wired `demo-image.js` and PiZero
`jpeg-slideshow.js`. They use the configured `devices.display` Canvas, not
`image`, `screen`, raw buffers, SPI pins or DVI globals.

Copy the directory layout to your device (for example under `/app/portable`):

- `images/info.js`
- `images/slideshow.js`
- `jpeg/show.js`
- `sd-bmp/show.js`

After ejecting the MCUJS drive from the host:

```js
var info = require('/app/portable/images/info');
console.log(info('/app/test_172x320.jpg'));
console.log(info('/app/test_172x320_24bit.bmp'));
var slides = require('/app/portable/images/slideshow')([
  '/app/test_172x320.jpg', '/app/test_172x320_24bit.bmp'
]);
// Optional early cancellation:
// slides.close();
```

`/sd` paths work through the same file API when that storage is available.
No directory scanning: supply one to eight explicit paths. The list is copied
and shown once, with at most one active renderer/display. Each successful
slide stays visible for two seconds **after** rendering completes. Bad headers,
unsupported decodes and per-slide timeouts are reported and stop the slideshow.
All errors fail closed: there is no retry or error-classification framework,
and no later renderer opens after a timer or cleanup failure.

## Metadata is not decode validation

`info(path)` reports JPEG `format`, `width`, `height`, or BMP `format`, `width`,
`height`, `bpp`. It reads bounded binary headers and closes the file before
returning: one 128-byte buffer, at most 128 read calls (including partial reads),
and JPEG header positions below 16,384 bytes. It never acquires a display or a
JPEG-decoder lease. Header metadata
can describe a progressive JPEG or a BMP depth/header that the rendering
examples intentionally do not decode. Pixel corruption and decoder support
are checked by the renderer, not certified by `info`.

## Completion and speed

Both image renderers return `{ close, status }`. `status()` returns `loading`,
`ready`, `error`, or `closed`. `close()` cancels pending work and is idempotent;
errors remain observable as `error` after cleanup. Existing console markers
remain for manual use. The slideshow observes status, never parses console
output, and closes the previous display before opening the next.

JPEG rendering still uses one MCU block per timer turn; BMP uses one row.
The standalone JPEG renderer has a 120-second rendering deadline, then a fresh
60-second display lifetime after completion (matching BMP), so a late success
cannot interrupt the slideshow hold.
A 172x320 JPEG currently takes about a minute on the 1.47-inch LCD. A fixed
five-second slide interval would interrupt it, so the slideshow waits for
completion and caps each rendering attempt at 122 seconds. This is not an
image-speed optimisation or an e-paper-refresh qualification.

## Placement, clipping and overlays

Copy `images/features.js` alongside the files above, then run:

```js
var demo = require('/app/portable/images/features')(
  '/app/test_172x320.jpg', '/app/icon_32x32.bmp'
);
// demo.close();
```

The diagnostic owns one display for four completed scenes: four corner icons on
blue, a centered icon on red, icons clipped at all four edges on bright green,
and a centered JPEG with five layered BMP icons plus white/black rectangles and
a yellow line. It needs a Canvas at least 128x128 and the supplied 32x32 BMP icon.
Each intermediate scene holds for two seconds after presentation. The entire
rendering sequence is bounded to three minutes, then the final composite has a
fresh one-minute lifetime. Any error stops the diagnostic and attempts all
cleanup. JPEG backgrounds are centered and clipped, **not rotated or scaled**;
this deliberately differs from the standalone full-image renderer.

For custom composition, both existing renderer functions also expose:

```js
var task = require('/app/portable/sd-bmp/show').draw(
  '/app/icon_32x32.bmp', display.canvas, -16, 40
);
```

`draw(path, canvas, x, y)` borrows the caller's Canvas. Positions are explicit
finite signed-32-bit integers; clipping uses the existing Canvas implementation.
The helper does not acquire, clear, rotate, present or close the caller's display.
It paints through `fillRect` and changes `fillStyle`. Wait for `task.status()` to
be `ready`, then call `task.close()` before starting the next image. The caller
presents the completed composition and closes its display. Borrowed completion
logs use `JPEG_DRAW_READY`/`BMP_DRAW_READY`, not the standalone presentation
markers. Cancellation/error
releases the helper's file/decoder/timers, not the borrowed Canvas. Buffers and
one-MCU/one-row scheduling stay the same; no second decoder or framebuffer is added.

## Retired image diagnostics and coverage

The seven old image-only scripts from the 1.28/1.47 board directories have been
removed rather than kept as compatibility shims:

- `test-image`, `test-minimal`, `test-step`: file decode and placement move to
  the portable renderers and this diagnostic.
- `test-all`, `test-blue`, `test-jpeg`: format/color/read failures are covered by
  the native JPEG suite, binary BMP tests, and portable show/slideshow examples.
- `test-features`: corners, centering, edge clipping and layered shapes/icons
  move to `features.js`; full-image BMP remains in `sd-bmp/show.js`.

Their private GPIO/SPI initialization and raw-buffer handles are intentionally
not reproduced. The image behavior is separated from display-controller setup.
This migration does not establish physical qualification of the 1.28, DVI,
SD cards or e-paper. Native legacy bindings/registrations remain for #19;
e-paper refresh and retained-image qualification remain #22.
