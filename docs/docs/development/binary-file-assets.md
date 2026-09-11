---
title: Bounded binary file assets
---

# Bounded binary file assets

The same synchronous handle API serves `/app` and configured `/sd` mounts:

```js
var fs = require('fs');
var fd = fs.openSync('/sd/picture.bmp', 'r');
var bytes = new Uint8Array(512);
try {
  var count = fs.readSync(fd, bytes, 0, bytes.length, null);
  // Only bytes[0..count) were read. Zero means EOF.
} finally {
  fs.closeSync(fd);
}
```

- `openSync(path, 'r')` opens for reading; `'w'` creates/truncates for writing.
  Other flags are unsupported. Check destination ownership before using `'w'`.
- `readSync(fd, Uint8Array, offset, length[, position])` and `writeSync(...)`
  operate on raw bytes, including NUL and bytes above 127. Views/subarrays retain
  their proper byte offset. ArrayBuffer, other typed arrays and strings are not
  accepted as buffers in this slice.
- Omitted/null position advances the handle's sequential cursor. An explicit
  position leaves that cursor unchanged. Reads beyond EOF return zero. Sparse
  writes beyond the current file size are rejected rather than exposing FatFs
  uninitialized gaps.
- Maximum four live handles and 4096 bytes per call; common maximum file position
  is 2147483647. Offset, length and position must be finite nonnegative integers.
- Callers supply buffers. The binding additionally uses one fixed 4096-byte
  native scratch buffer; it never allocates storage proportional to file size.
- Closed/stale handles fail with `EBADF`; exhausting handles returns `EMFILE`.
  Access modes are enforced. Media/USB ownership errors remain authoritative.
  Transfer failures include `bytesRead`/`bytesWritten`; a short write is `ENOSPC`.
  Explicit close reports flush errors and retires the handle even on failure.
- Engine cleanup closes remaining native files before destroying the VM. It does
  not make an interrupted write transactional or promise success on removal.
- `board.capability('fs').binary` advertises the configured limits. This is not
  a media-health reading. Existing string-based `readFileSync`/`writeFileSync`
  semantics remain unchanged; no Node Buffer shim, streams or async I/O added.

## Image example

Install `examples/portable/sd-bmp/copy.js` as `/app/bmp-copy.js` and `show.js` as
`/app/bmp-show.js`, through the internal USB volume. Eject that volume first.
Copy is format-agnostic. The renderer accepts a bounded 40-byte DIB BMP:

- **16-bit RGB565:** `BI_BITFIELDS` (`compression=3`), with explicit masks
  `0xf800`, `0x07e0`, `0x001f` immediately after the DIB and pixels at offset 66
  or later. RGB555, other masks and ambiguous 16-bit `BI_RGB` are rejected;
  the old native decoder's implicit RGB565 assumption is not copied.
- **24-bit BGR:** uncompressed `BI_RGB`, as before.
- **32-bit BGRX:** uncompressed `BI_RGB`. The fourth byte is ignored, including
  zero; it is not alpha blending. Equal colors can share a run even when that
  unused byte differs. Extended DIB headers and 32-bit bitfields are not covered.

Real fixtures are in `examples/images/test_172x320_16bit.bmp` and
`test_172x320_32bit.bmp`. The former was mislabeled 24-bit/V5 data and is now
actual RGB565; the latter retains full BGR color with varying unused bytes.
JPEG has a separate bounded reader below. General image metadata and legacy
caller migration remain separate work; no native decoder was removed.

```js
require('/app/bmp-copy')('/app/picture.bmp', '/sd/picture.bmp');
// Wait for COPY_READY, then reset and eject internal MSC again.
require('/app/bmp-show')('/sd/picture.bmp');
```

Copy refuses an existing destination, uses a 1024-byte Uint8Array and yields
between chunks. The image renderer reads a 54-byte header, 12 mask bytes only for
RGB565, and one padded row per timer turn. At the 640-pixel width limit its input
buffers total at most 2614 bytes; no read exceeds 2560 bytes. It supports
bottom-up/top-down rows, honors padding, rejects
invalid dimensions/compression/truncation, and closes the file on completion or
failure. Images must fit the configured display directly or after clockwise rotation
(and be no larger than 640x640). Rotation uses canvas dimensions only.
The display uses its existing framebuffer; row-wise file decoding does not
eliminate that framebuffer or normal JS/native renderer memory. `BMP_READY`
means drawing calls completed, not independent proof of physical output. The
display handle releases after 60 seconds; the returned `.close()` can cancel.

## Baseline JPEG example

`examples/portable/jpeg/show.js` uses the format-specific `jpeg` reader and the
existing Canvas operations, not deprecated `image`, `graphics` or `screen` APIs.
It requires new firmware with the `jpeg` module; copying this JS onto older
firmware does not add the native decoder interface.

Install the example as `/app/jpeg-show.js` and the existing
`examples/images/test_172x320.jpg` as `/app/picture.jpg`. Eject the internal USB
volume before running it:

```js
var picture = require('/app/jpeg-show')('/app/picture.jpg');
// The same caller accepts a file on an already-configured /sd mount.
// Stop early with picture.close().
```

The example reads at most 256 bytes per timer turn, accepts at most 16384
compressed bytes, checks for a changed file length, and closes the file before
opening a decoder or display. It centers the image or rotates it clockwise to
fit the configured canvas, without scaling. One JPEG block is decoded and drawn
per subsequent timer turn. Adjacent equal RGB values share a Canvas rectangle.
The existing display framebuffer is reused; no second full-frame pixel buffer
is created. The entire load/display operation has a 60-second deadline.

The concrete decoder interface is independent of paths and display ownership:

```js
var jpeg = require('jpeg');
var reader = jpeg.open(bytes); // bytes is a Uint8Array containing one baseline JPEG
var pixels = new Uint8Array(768);
try {
  // Dimensions are available without opening a display.
  console.log(reader.width, reader.height);
  var block = reader.read(pixels); // one block per call; schedule between calls
  // block is {x, y, width, height}, or null at completion.
  // pixels contains tightly packed RGB888 for that block; only
  // block.width * block.height * 3 bytes are valid.
} finally {
  reader.close();
}
```

The reader snapshots at most 16384 bytes and bounds image dimensions to 320x320.
It reuses picojpeg and emits at most a 16x16 block into a caller-owned
Uint8Array of at least 768 bytes. There is one active decoder lease; a second
open fails with `EBUSY`, and legacy JPEG drawing cannot replace an active
reader's decoder state. Explicit close is idempotent. Completion, decoder error
and garbage collection release the native snapshot and lease. Reads after
explicit close fail with `ENXIO`; reads after normal completion return null.
Only complete, single-scan baseline files with a final EOI marker and no trailing
bytes are accepted. Progressive JPEG is not supported. The pinned picojpeg is
prepared in the build directory, leaving the SDK untouched: green accumulation
stays signed until both chroma contributions are applied, and entropy accounting
rejects premature end markers instead of decoding synthetic padding. The color
correction adds 512 static bytes; entropy bookkeeping adds a few more. Both the
reader and remaining legacy JPEG callers use the corrected decoder. Marker parser
errors are propagated; restart boundaries and final entropy padding/EOI are
checked before success. The reader requires every frame component in the single
scan, in frame order, using picojpeg's supported Huffman table selectors (0/1).

At the input ceiling, the example can temporarily hold a 16 KiB JS input and a
16 KiB native snapshot, plus its 768-byte RGB output block. The existing fs
scratch buffer, picojpeg state, JavaScript objects and display framebuffer are
additional memory, not included in those byte-buffer limits. Input is bounded,
not a claim of constant-memory streaming of arbitrarily large JPEG files.

`JPEG_READY` is emitted only after decoding completes and the display accepts
presentation. It is not camera proof. Native decoder tests and example
scheduling/ownership tests are separate evidence; LCD acceptance requires the
new firmware and is not implied by passing either suite. This slice does not
qualify JPEG on ESP builds without the decoder, slideshow callers, arbitrary
image metadata, or SD-card hardware compatibility.

## Write-fault investigation and acceptance

On diagnostic firmware `17c4bf3`, three new-file writes and three rewrites passed
exact readback after normal software resets. The three scoped probes were
removed, and the original root listing was restored. The earlier first-write
error was not reproduced and remains unexplained; this is not a reliability fix.

Hardware acceptance on the 1.47 with firmware `7b4f401` and the orientation-aware
BMP example: copied the pre-existing 172x320 24-bit BMP to the new scoped path
`/sd/mcujs-binary-7b4f401.bmp` with a 1024-byte copy buffer. All 165174 bytes
read back after a normal software reset (no ROM assistance) matched SHA-256
`959ae20e9a887318e4b879455356467f065ed9b178556ee1fbd348667d1d9928`.
The renderer completed with a 54-byte header plus one 516-byte row buffer;
webcam inspection showed the source red-purple-blue gradient and white `mcujs`
text rotated clockwise into the 320x172 canvas. The SD root listing changed only
by that BMP. All 13 original internal files were hash-verified after helper
installation. This proves software-reset persistence, not physical power-cycle
or power-loss behavior. The original example rejected the portrait BMP; the
orientation change is example-only and required no firmware reflash.
No formatting, existing-card-content replacement, physical removal qualification,
power-loss atomicity or broad card-compatibility claim is implied.
