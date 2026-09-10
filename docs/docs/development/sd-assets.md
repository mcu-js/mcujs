---
title: Removable SD assets
---

# Optional `/sd` assets — first hardware slice

On the configured **Waveshare RP2350 LCD 1.47 A**, `/sd` is a separate removable
FAT volume. `/app` remains internal flash; relative `fs` paths and automatic
startup still use `/app`. A card never replaces `/app` or supplies an automatic
`index.js`. Absolute `require('/sd/module')` and imports within that module are
explicit opt-ins, not a second startup search.

```js
var fs = require('fs');
var capability = board.capability('fs');
if (!capability.sd) throw new Error('No configured SD mount');
var asset = JSON.parse(fs.readFileSync('/sd/mcujs-proof.json', 'utf8'));
```

`board.capability('fs').sd` describes configured support: root `/sd`, FAT,
removable, writable, no USB host transfer, FAT16/FAT32 formats. It does **not**
claim a card is present, healthy or writable right now. The mount is attempted
lazily when accessing `/sd`; absence or unsupported formatting does not block
internal application startup. Other boards do not advertise this adapter.

## Preservation and ownership

- **No SD formatting API or automatic formatting.** exFAT and unsupported media
  fail rather than being converted. This first adapter supports block-addressed
  SD v2 cards (SDHC/SDXC) with FAT16/FAT32; SDSC is not supported.
- USB MSC continues to expose **only internal flash**. `/sd` remains runtime-owned
  even when the USB host owns `/app`. Never attach the same card to a second
  writer. Copy assets using the existing runtime `fs` API, or power off the board
  and use a computer card reader before reinserting it.
- No mount-crossing traversal: `/sd/../app` fails, even when the resulting path
  would return to a valid mount. Cross-mount renames fail with `EXDEV`. `/sd`
  itself cannot be removed, renamed or opened as a file.
- This board's verified SD pins are SPI1 SCK10/MOSI11/MISO12/CS15; the display
  uses SPI0. Those SD pins and SPI1 are not exposed as application peripheral
  routes. This is **not** shared-SPI display/card arbitration on other boards.
- Failed card I/O invalidates the mount. Close outstanding files before retrying;
  stale open handles are not silently redirected to a newly inserted card.
  The slot has no configured card-detect switch: idle removal is not an event.
  Power off before swapping cards; live hot-swap is not qualified.
- Errors distinguish `ENOMEDIUM` (unavailable/not ready), `ENOTSUP` (unsupported
  filesystem), `EROFS` (write-protected), `EIO` (failed transport/filesystem),
  `ENOSPC` (short/full write), and `EXDEV` (cross-mount rename).

The existing synchronous file size and memory limits still apply. The current
JS API is string-based; the demonstration uses a small ASCII JSON pixel asset,
not arbitrary binary data. Binary buffers, file-handle streaming, asynchronous
I/O, capacity/status APIs and other board adapters remain follow-up work under
[issue #7](https://github.com/mcu-js/mcujs/issues/7). Writes are not power-loss
atomic; keep backups and do not unplug during a write.

## Evidence required for the asset demonstration

1. Back up firmware/internal files before any update; recover them if firmware
   growth relocates the physical internal filesystem. Preserve the card as-is.
2. List the card before writing. Choose a unique test path that does not replace
   existing content. Keep the host source asset and its SHA-256.
3. Write the asset through `/sd`, close it, then reset the device so neither the
   JavaScript variable nor the FatFs sector cache can supply the next read.
4. Read the asset back from `/sd`, compare the exact returned bytes against the
   host source and hash, and render using only those loaded pixels/palette.
5. Capture the physical display with the bench webcam. A serial `READY` message
   alone does not prove rendering. Use a different second asset to distinguish
   a real load from a stale or hard-coded display.

The bounded hardware result and remaining reliability limits are recorded below.

## Sources

- [Manufacturer board wiki](https://www.waveshare.com/wiki/RP2350-LCD-1.47-A)
- [Manufacturer demos](https://files.waveshare.com/wiki/RP2350-LCD-1.47/RP2350-LCD-1.47.zip),
  `Python/02-SD/boot.py` and `C/02-FatFs/example/config/hw_config.c` verify wiring.

## Hardware acceptance

The 1.47 A hardware test established this bounded result:

- `/sd` listed the pre-existing card contents and read an existing small file.
- Two new external JSON pixel assets were written without formatting the card.
- Fresh runtimes after normal software resets read back both assets exactly:
  A: 398 bytes, SHA-256 `28ff501447b0e867f30edd29a3e23af656fad2dbcc431221bf37de345a027c4c`;
  B: 399 bytes, SHA-256 `13e1e76dbe2a05d539cc080f93671ed44b8168c9ff054e63a7789972fb8ea283`.
- The Logitech webcam showed distinct A/blue-border and B/red-border images,
  including different corner markers. The renderer read the pixel arrays from
  `/sd`; the firmware did not contain either picture.
- The original 12 internal files were backed up, restored and verified. Adding
  the renderer made 13 internal files. The later diagnostic flash retained the
  same filesystem offset and verified its protected tail unchanged.

**Limit:** the initial asset write returned an I/O error and left an empty test
file. A later retry succeeded; the first error is not root-caused. Physical
power-cycle/removal, long-duration writes and broad card compatibility are not
claimed. The on-device proof used diagnostic candidate `17c4bf3`; the preceding
`669b3e5` candidate passed all 13 firmware builds. This is hardware evidence for
the asset-loading path, not a declaration of production-ready SD reliability.
