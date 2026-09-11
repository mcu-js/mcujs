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

## Remaining legacy diagnostics

The old `test-image`, `test-jpeg`, `test-blue`, `test-all`, `test-features`,
`test-step` and `test-minimal` board scripts are legacy diagnostics, not the
maintained image entrypoints. Their raw driver/buffer access and compound
positioning/clipping/overlay scenarios are still in the #19/#21 retirement
audit; this slice does not claim those whole scripts have been ported. Native
legacy bindings remain until that audit and e-paper qualification are finished.
