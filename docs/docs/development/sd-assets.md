---
title: Removable SD assets
---

# Optional `/sd` assets — board-defined transports

On boards that advertise `fs.sd`, `/sd` is a separate removable FAT volume.
`/app` remains internal flash; relative `fs` paths and automatic startup still
use `/app`. A card never replaces `/app` or supplies an automatic `index.js`.
Absolute `require('/sd/module')` and imports within that module are explicit
opt-ins, not a second startup search.

Configured boards:

- **Waveshare RP2350 LCD 1.47 A**: writable `/sd` on a dedicated SPI bus,
  with a separate USB mass-storage volume in the enabled development build.
- **Waveshare RP2040 PiZero**: writable `/sd`, USB MSC, dedicated SPI0
  SCK18/MOSI19/MISO20/CS21 at 5 MHz.
- **Waveshare RP2350 Touch LCD 2.8**: writable `/sd`, USB MSC, GPIO SPI
  SCK19/MOSI20/MISO21/CS24. The 10 MHz runtime ceiling is policy, not a
  measured clock guarantee; hardware timing/throughput qualification is pending.
- **Waveshare ESP32-S3 ePaper 1.54 V2**: writable `/sd`, USB MSC, 1-bit
  SDMMC CLK39/CMD41/D0=40 at 4 MHz. There is no MCU-connected CS.
- **Seeed reTerminal Sticky**: read-only `/sd` on the shared display SPI bus.
  Writes, format, erase and USB host transfer are denied. Power/select of the
  inserted card is required for display traffic even when no file is opened.

```js
var fs = require('fs');
var capability = board.capability('fs');
if (!capability.sd) throw new Error('No configured SD mount');
var asset = JSON.parse(fs.readFileSync('/sd/mcujs-proof.json', 'utf8'));
```

`board.capability('fs').sd` describes configured support: root `/sd`, FAT,
removable, FAT16/FAT32 formats, and `hostTransfer` support. `writable` and
`hostTransfer` are board policies, not live media state. It does **not** claim a card is present, healthy
or writable right now. The mount is attempted lazily when accessing `/sd`;
absence or unsupported formatting does not block internal application startup.
Other boards do not advertise this adapter.

Wiring and firmware policy in `runtime/board-registry.js` generate the SD C
header, build source-selection flag, capabilities and manifests together. These
declarations require the matching platform adapters; they are not hardware
qualification or evidence for an unintegrated build. A disabled physical slot
requires a nonempty `policy.sd.disabledReason`; disabled policies do not compile
the SD filesystem backend. Sticky experimental display builds still require its
read-only SD adapter to own/power the shared SPI bus, and reject disabling it.
Enabled transport clocks are validated against backend limits (at least 100 kHz;
GPIO SPI at most 10 MHz).

## Preservation and ownership

- **No SD formatting API or automatic formatting.** exFAT and unsupported media
  fail rather than being converted. This first adapter supports block-addressed
  SD v2 cards (SDHC/SDXC) with FAT16/FAT32; SDSC is not supported.
- On profiles with `fs.sd.hostTransfer`, USB MSC exposes app flash and SD as independent
  volumes. A host-owned volume rejects device access with `EBUSY`; the other
  volume can remain device-owned. Never attach the card to a second writer.
  Boards without `fs.sd.hostTransfer` still use the runtime `fs` API or a card
  reader (power off before removing or reinserting a card).
- No mount-crossing traversal: `/sd/../app` fails, even when the resulting path
  would return to a valid mount. Cross-mount renames fail with `EXDEV`. `/sd`
  itself cannot be removed, renamed or opened as a file.
- The 1.47-A board's verified SD pins are SPI1 SCK10/MOSI11/MISO12/CS15; the display
  uses SPI0. Those SD pins and SPI1 are not exposed as application peripheral
  routes. This is **not** shared-SPI display/card arbitration on other boards.
- Failed card I/O invalidates the mount. Close outstanding files before retrying;
  stale open handles are not silently redirected to a newly inserted card.
  RP slots and ePaper V2 have no card-detect GPIO. Sticky has a hardware detect
  signal, but that does not establish a public hot-swap lifecycle.
  Power off before swapping cards; live hot-swap is not qualified.
- Errors distinguish `ENOMEDIUM` (unavailable/not ready), `ENOTSUP` (unsupported
  filesystem), `EROFS` (write-protected), `EIO` (failed transport/filesystem),
  `ENOSPC` (short/full write), and `EXDEV` (cross-mount rename).
  `ENOENT` covers a missing file/path or a build without a configured `/sd`.
  `existsSync` is not a media-health probe: false does not diagnose absence.
  On the RP SPI adapter, initialization failures currently share a not-ready
  status, so `ENOMEDIUM` alone cannot distinguish an empty socket from failed
  card initialization. Preserve the operation and diagnostic trace when
  investigating; do not infer a physical cause from the public code alone.

The existing synchronous file size and memory limits still apply. The current
JS API is string-based; the demonstration uses a small ASCII JSON pixel asset,
not arbitrary binary data. [Bounded binary file handles](./binary-file-assets.md)
now provide explicit Uint8Array transfers. Streams, asynchronous I/O,
capacity/status APIs and other board adapters remain follow-up work under
[issue #7](https://github.com/mcu-js/mcujs/issues/7). Writes are not power-loss
atomic; keep backups and do not unplug during a write.

## Copying files over USB

The runtime namespace is `/app` plus optional `/sd`. The computer sees separate
volumes, not a synthetic disk containing `app/` and `sd/` folders:

```text
Computer                         MCU.js
MCUJS/index.js               ->  /app/index.js
<existing SD label>/photo.bmp ->  /sd/photo.bmp
```

`MCUJS_SD` would be a useful user-chosen card label, **not** a label firmware
writes automatically. Actual mount locations and displayed names depend on the
host OS. SD slot presence alone does not imply USB export: check
`board.capability('fs').sd.hostTransfer === true`.

1. Connect the enabled board with a supported FAT16/FAT32 card already inserted.
   After startup files are read, firmware requests host ownership of each
   volume. An open runtime file/directory delays that volume's handoff until it
   closes; an absent or unsupported SD card does not stop app-volume export.
2. Copy a uniquely named file into the appropriate drive's root with the normal
   file manager. Do not overwrite existing files unintentionally.
3. Use the host's **safe eject** action and wait for completion before reading
   that volume from JavaScript. An unmount command that sends no SCSI eject is
   not enough. Eject may affect one volume or the entire USB device depending
   on the OS/tool; independent host-UI eject is **not yet qualified**.
4. Read the copied file at `/sd/name` or `/app/name`. `board.storageReady()` still
   describes internal `/app`, not SD health. Built-in modules and non-filesystem
   work can continue while a volume is host-owned; code requiring that volume
   receives an actionable `ResourceBusyError` (`EBUSY`).

The adapter drains/synchronizes before acknowledging eject, stops new host I/O
immediately, then remounts only that filesystem in the main loop. Host removal
locks are honored. LOAD followed by EJECT cannot leave an obsolete claim queued.
Reconnect requests export again; eject any volumes needed by the running app.

The ESP backend preserves explicit, confirmed internal detach/eject events.
Its generic TinyUSB unmount callback is ambiguous and retains the host fence;
it is not a physical-detach detector. Use safe eject, including on no-SD ESP
profiles, rather than relying on cable removal to grant runtime file access.

USB reset, suspend or generic deconfiguration is **not evidence of safe eject**.
Those signals retain the host-ownership fence; there is no reclaim timer.
On ambiguous unplug, reconnect and safely eject. Failed SD host I/O fences that
lease; reconnect alone does not clear it or reinitialize another card under
cached host metadata. Stop host access before a deliberate device restart, and
inspect/repair media externally when necessary. A failed remount never triggers
formatting. Physical detach detection, live card swapping and power-loss-atomic
writes are not promised. Card identity checks reduce stale-media access but are
not a hot-swap qualification or an authentication mechanism.

### Verification boundary

Native tests compose **two independent FatFs R0.16 instances**, actual MCU.js
filesystem code and actual RP2 MSC callbacks over memory-backed block media.
They verify host copy -> sync/eject -> exact device read, the reverse direction,
per-volume ownership, labels/sentinels, and absent/unsupported/failed media with
no SD formatting. Additional callback and SPI fixtures cover bounds, write
protection, invalid LUNs, removal locks and card-identity changes.

Those fixtures are not physical USB/card tests. Separately, physical 1.47-A
firmware `85fe3587802ac15143d604b7c89d804b9a0823a2` passed Linux command-line
USB mount/copy/sync/eject and native read/write acceptance:

- Both app and SD volumes, independent ownership and expected busy errors.
- Exact binary contents: 1 MiB host-to-SD/native read and 4 KiB native-to-host
  write/readback; text and binary persistence after board reset and safe eject.
- Original filenames/content hashes preserved, test files removed, and a clean
  final FAT check. Existing card filesystem defects were repaired separately
  using backed-up, offline-verified metadata changes; firmware never reformats
  or automatically repairs the card.

SD USB capacity is the validated FAT volume, exported with logical sector zero
at its boot sector. It is not raw whole-card access. Every partition location
native FatFs could scan is checked before mount, including after host eject.

Physical 1.47-A firmware `bf7bb131ebf9bbf414552e0b58ec490feca037f1` also passed
bounded macOS FSKit acceptance: both app and SD mounted automatically, SD was
writable, and safe eject/software reset repeated the automatic mount in
16.261 seconds. File inventories covered 21 app and 141 SD entries; user-file
hashes were preserved, excluding expected Spotlight/fseventsd housekeeping
changes. Firmware readback and exact app/EEPROM preservation were verified;
both LUNs reported zero device read/write errors and retries.

The previous firmware transferred the mount-time FAT scan steadily at about
164 KiB/s without device errors, needing about 23 seconds for this card's FAT.
FSKit initiated unload after 20 seconds, followed by the read error. Bounded
FIFO reads and a board-specific 10 MHz target (below the manufacturer's supplied
SPI example) let the scan complete. Initialization stays at 400 kHz; other
boards keep the 5 MHz default. This fix does not modify card metadata or format
the card. A `F_NOCACHE` file read may still be FSKit-cached and is not, by itself,
a physical USB throughput measurement.

File-manager drag/drop UI, formal Windows host acceptance, live hot-swap and
interrupted-power behavior remain **NOT_RUN**. The macOS repeat used software
reset, not physical power removal. These bounded tests are not all-platform or
release-candidate qualification. Earlier asset evidence below predates USB SD
export.

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
