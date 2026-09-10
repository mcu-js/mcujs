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
The helpers require an uncompressed 24-bit BI_RGB BMP (40-byte DIB header):

```js
require('/app/bmp-copy')('/app/picture.bmp', '/sd/picture.bmp');
// Wait for COPY_READY, then reset and eject internal MSC again.
require('/app/bmp-show')('/sd/picture.bmp');
```

Copy refuses an existing destination, uses a 1024-byte Uint8Array and yields
between chunks. The image renderer reads a 54-byte header and one padded BGR row
per timer turn. It supports bottom-up/top-down rows, honors padding, rejects
invalid dimensions/compression/truncation, and closes the file on completion or
failure. Images must fit the configured display directly or after clockwise rotation
(and be no larger than 640x640). Rotation uses canvas dimensions only.
The display uses its existing framebuffer; row-wise file decoding does not
eliminate that framebuffer or normal JS/native renderer memory. `BMP_READY`
means drawing calls completed, not independent proof of physical output. The
display handle releases after 60 seconds; the returned `.close()` can cancel.

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
