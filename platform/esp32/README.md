# ESP32-S3 backend

This backend targets the Seeed XIAO ESP32-S3 with ESP-IDF's fixed USB
Serial/JTAG console. It deliberately does **not** initialize USB-OTG, TinyUSB,
or MSC; the shared internal USB PHY remains owned by fixed Serial/JTAG while
TinyUF2 provides the recovery transport.

The GPIO API is restricted to the XIAO's exposed D0-D10 pins and onboard LED;
JavaScript cannot reconfigure internal flash or PSRAM pins.

## Pinned dependencies

- ESP-IDF `v5.3.2` (`9d7f2d69f50d1288526d4f1027108e314e8c879f`)
- JerryScript `v3.0.0` (`50200152feb724a74a5f64e44d7885151537cfad`)
- TinyUF2 source `9af754c408ae76e862830de33dbd03b93a9a82e1`
- Microsoft UF2 submodule `84444ddb9d2914edf7f6d9e89a7ce41b53d64d98`

The helper scripts default to checkouts under `$HOME/toolchains`; the paths can
be overridden with `IDF_PATH`, `JERRYSCRIPT_PATH`, and `TINYUF2_PATH`.
Builds fail if the pinned Git revisions differ or the toolchain trees are dirty.

## Build

```sh
platform/esp32/build.sh build
platform/esp32/make-uf2.py
```

`make-uf2.py` verifies the dependency pins and independently checks every UF2
block, family ID, target address, payload, and OTA boundary.

## Flash ownership

The product-compatible table is byte-identical to TinyUF2 0.35.0's official
`partitions-8MB-noota.csv` layout:

```text
nvs       0x009000    20 KiB
otadata   0x00e000     8 KiB
ota_0     0x010000      4 MiB
uf2       0x410000    256 KiB
ffat      0x450000   3776 KiB
```

ESP-IDF normally treats a `factory` partition as the default application flash
destination. Here that partition contains TinyUF2, so the project rewrites the
generated `flash` and `app-flash` metadata to the named `ota_0` partition and
fails configuration unless it remains at `0x10000`.

`platform/esp32/build.sh flash` is refused by default because a full ESP-IDF
flash also replaces the TinyUF2-aware second-stage bootloader, partition table,
and OTA metadata. Use `app-flash` for routine development updates:

```sh
platform/esp32/build.sh -p /dev/mcujs-dev app-flash
```

The destructive full flash requires an explicit
`MCUJS_ALLOW_DESTRUCTIVE_FULL_FLASH=1` override. That override is for recovery
engineering only; it is not a normal installation path.

Migrating a TinyUF2 dual-OTA board to the no-OTA table is a one-time layout
operation. Back up the complete physical flash first, verify two independent
reads match, and write only the partition-table and `ota_0` application ranges.
Do not write the stock ESP-IDF bootloader, NVS, OTA data, `uf2`, or `ffat`
regions. The resulting table should compare byte-for-byte with TinyUF2's pinned
8 MB no-OTA table before it is used.

For an existing compatible TinyUF2 baseline, copying the generated
application-only UF2 to `XIAOS3BOOT` remains supported. TinyUF2 remaps UF2
address zero into `ota_0`, bounds the write to that partition, marks OTA0
bootable, and preserves its bootloader, partition table, factory image, and
FFAT region.

A software reset can keep fixed USB Serial/JTAG enumerated. A transition
between TinyUF2 USB-OTG and fixed Serial/JTAG may require a physical reset or
USB power cycle because both controllers share the internal PHY.

`board.enterUf2()` sets TinyUF2's pinned `0x11F2` reset-reason hint and performs
a software reset. A verified TinyUF2-aware bootloader then selects the factory
`uf2` partition for that boot. The recovery volume should enumerate as
`XIAOS3BOOT`; power-cycle to return to the selected `ota_0` application without
rewriting it.

## Filesystem and modules

Milestone 3 mounts the `ffat` partition at the private ESP-IDF VFS path
`/mcujs`. JavaScript continues to see paths rooted at `/`; `..` traversal and
backslash paths are rejected before they reach VFS.

```js
const fs = require('fs');
fs.writeFileSync('/config.json', '{"enabled":true}');
fs.appendFileSync('/events.log', 'started\n');
fs.mkdirSync('/lib');
const config = require('/config.json');
```

The backend uses ESP-IDF FatFs with wear levelling, heap-backed long filenames,
and UTF-8 paths. It implements read, write, append, seek, rename, delete,
directory listing, nested stat, and CommonJS/JSON loading. Filesystem failures
throw JavaScript exceptions with Node-style error codes. Paths are measured
before copying, so overlong destructive paths fail with `ENAMETOOLONG` instead
of operating on a truncated prefix. Every CommonJS module receives a require
function bound to its own filename, including deferred relative loads.

The filesystem is formatted only when its NVS initialization marker is absent
**and** a pre-mount scan proves every byte of the FFAT partition is erased
(`0xff`). Missing NVS state is never treated as evidence that non-erased user
storage is disposable. Any non-erased mount failure leaves the partition
untouched and starts the runtime without storage. Raw sector writes are
unavailable while FatFs is mounted; MSC ownership and cache coordination belong
to Milestone 4.

## `/index.js` and persistent safe mode

When `/index.js` exists, MCU.js records a pending boot in NVS before executing
it. The pending marker is cleared only after the script returns and the normal
runtime loop remains healthy for five seconds.

- A syntax/runtime failure leaves the current REPL available; the next reset
  enters safe mode and skips `/index.js`.
- The main runtime task is independently subscribed to the task watchdog and is
  reset only from the healthy outer loop. Busy or yielding non-returning scripts
  therefore reboot and are skipped because their pending marker survived.
- A boot script cannot clear its own pending marker during qualification.
- NVS errors force safe mode and never trigger an automatic NVS erase.

The ESP32 board binding exposes:

```js
board.storageReady();       // true when FFAT is mounted
board.safeMode();           // current persistent safe-mode state
board.safeMode(true);       // skip /index.js on subsequent boots
board.safeMode(false);      // clear explicit/failure state outside qualification
```

Fix or remove the broken script from the REPL, call `board.safeMode(false)`, and
reset to resume automatic startup.

## Hardware smoke

With the runtime available at `/dev/mcujs-dev`:

```sh
source "$IDF_PATH/export.sh"
python platform/esp32/hardware-smoke.py --resets 2
python platform/esp32/boot-smoke.py
```

The first harness verifies the Milestone 2 runtime plus filesystem CRUD, nested
metadata, deferred relative CommonJS loading, path confinement, overlong-path
rejection, persistence across resets, exact line-framed REPL results, and a
watchdog-free idle window. The boot harness verifies healthy startup,
qualification, syntax-failure recovery, explicit safe mode, attempted pending
marker bypass, and automatic recovery from immediate and delayed yielding
infinite loops. Neither harness reads or prints the board's unique identifier.

## Deliberately unavailable

Until later milestones, this backend excludes runtime MSC/custom USB composite
mode, PWM, I2C, SPI, ADC, NeoPixel, graphics, and displays.
