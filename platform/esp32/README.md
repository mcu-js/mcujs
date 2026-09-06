# ESP32-S3 backend

This backend targets the Seeed XIAO ESP32-S3 with a TinyUSB USB-OTG composite
device: CDC provides the JavaScript REPL and MSC exposes the persistent `MCUJS`
filesystem. UART0 remains mirrored on XIAO D6/D7 as an independent 3.3 V
recovery console. TinyUF2 provides the application-update transport.

The GPIO API is restricted to the XIAO's exposed pins and onboard LED, excluding
UART recovery pins D6/D7. JavaScript cannot reconfigure recovery, flash, or
PSRAM pins.

## Pinned dependencies

- ESP-IDF `v5.3.2` (`9d7f2d69f50d1288526d4f1027108e314e8c879f`)
- JerryScript `v3.0.0` (`50200152feb724a74a5f64e44d7885151537cfad`)
- `espressif/esp_tinyusb` `1.7.6~2`, resolved by `dependencies.lock`
- TinyUSB `0.21.0~1`, resolved transitively by `dependencies.lock`
- TinyUF2 source `9af754c408ae76e862830de33dbd03b93a9a82e1`
- Microsoft UF2 submodule `84444ddb9d2914edf7f6d9e89a7ce41b53d64d98`

The helper scripts default to checkouts under `$HOME/toolchains`; the paths can
be overridden with `IDF_PATH`, `JERRYSCRIPT_PATH`, and `TINYUF2_PATH`.
Builds fail if the pinned Git revisions differ or the toolchain trees are dirty.

## Build

Local build using the pinned toolchains under `$HOME/toolchains`:

```sh
platform/esp32/build.sh build
platform/esp32/make-uf2.py
```

`make-uf2.py` verifies the dependency pins and independently checks every UF2
block, family ID, target address, payload, and OTA boundary.

The top-level build entrypoint selects the dedicated ESP32 Docker lane:

```sh
./build.sh seeed_xiao_esp32s3 --prepare-image  # Explicit acquisition phase
./build.sh seeed_xiao_esp32s3                 # Networkless compilation
```

The existing `platform/esp32/docker-build.sh` remains usable directly with the
same `--prepare-image` flag. Compilation requires an existing local image,
resolves `MCUJS_ESP32_DOCKER_IMAGE` to its immutable ID, and uses `--pull never`.
`--docker-network` is preparation-only; `MCUJS_DOCKER_NETWORK` cannot enable a
runtime network. See [building options](../../docs/docs/advanced-building.md).

It pins the reviewed AMD64 child image for `espressif/idf:v5.3.2`, verifies the
same ESP-IDF commit, and embeds pinned JerryScript, TinyUF2, Microsoft UF2, and
strictly validated ESP component trees. It runs as the invoking user with no
runtime network or USB devices, accepts only a non-flashing build action, mounts
source read-only, and writes final artifacts only to the fixed, symlink-checked
`platform/esp32/build-docker/` directory. ARM hosts therefore require Docker's
AMD64 emulation rather than silently selecting a different native toolchain.

To prove the local and container toolchains produce identical application and
UF2 bytes from the same source tree:

```sh
platform/esp32/verify-docker-repro.sh
```

Reproducible-build mode removes compile timestamps and normalizes all source,
JerryScript, build, and compiler-tool paths. The verifier performs fresh
isolated builds and fails unless the ELF, application `.bin`, and `.uf2`
compare byte-for-byte.

## Flash ownership

The build can warn that the factory `uf2` partition is smaller than the runtime
application. This is a diagnosed target-selection mismatch: pinned ESP-IDF's
`check_sizes.py partition --type app` compares the runtime against **all** app
partitions, including the separate 256 KiB TinyUF2 factory recovery application.
It warns when some app partitions fit and others do not; it fails when none fit.
The warning neither measures the TinyUF2 image nor indicates a write to factory.
The ordinary second-stage `bootloader.bin` has its own size check.

Keep this generic diagnostic unchanged: the inspected esptool CMake invocation
hard-codes `PARTITION_TYPE app`; no supported narrow override was established.
Although the checker accepts `--subtype`, a missing match only warns and returns,
so that selector alone would not establish a valid runtime destination. Do not
filter the warning, disable size checks, or expand/retype the factory partition.

The wrapper separately validates a single runtime image at `ota_0` (`0x10000`)
and rejects empty images, overflow past `0x410000`, and wrong flash metadata.
The release verifier additionally requires the supported five-entry partition
contract (names, types, subtypes, offsets, sizes and no flags) and checks the
zero-relative, padded UF2 payload against the 4 MiB runtime slot, not factory.
Its table validation accepts explicit hexadecimal or decimal byte counts, not
implicit offsets or size suffixes. `make-uf2.py` retains its more conservative
2 MiB default; this warning-only correction does not raise that generation limit.

Run the host-only regressions with `node --test tests/release-artifacts.test.js`.
They exercise the release verifier and the wrapper's actual embedded metadata
validator, not a native ESP-IDF build. Successful validation is **not** flashing
or recovery clearance: the installed partition table, TinyUF2-aware bootloader,
factory recovery image and physical recovery behavior still require independent
verification. No partition layout or loader/update behavior is changed here.

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

The build wrapper refuses every ESP-IDF action containing `flash` or `erase`
except exact `app-flash`. Full flashing can replace the TinyUF2-aware
second-stage bootloader, partition table, OTA metadata, recovery image, or user
storage. Invoke ESP-IDF through the wrapper; direct `idf.py` writes bypass these
project safeguards and are unsupported.

The normal Milestone 4 update path is application-only UF2:

```sh
# In the CDC REPL:
board.enterUf2()

# Then copy the generated file to the XIAOS3BOOT volume:
cp platform/esp32/build/mcujs-0.1.0-seeed_xiao_esp32s3.uf2 /path/to/XIAOS3BOOT/
```

The UF2 filename follows the release convention
`mcujs-<version>-<board-id>.uf2`; the XIAO board ID intentionally includes its
manufacturer as `seeed_xiao_esp32s3`.

`app-flash` remains available only when an esptool-compatible ROM or fixed USB
Serial/JTAG transport is already active; TinyUSB CDC is not an esptool port.
Destructive targets require an explicit
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

`board.enterUf2()` sets TinyUF2's pinned `0x11F2` reset-reason hint and performs
a software reset. A verified TinyUF2-aware bootloader then selects the factory
`uf2` partition for that boot. The recovery volume should enumerate as
`XIAOS3BOOT`. Installing an application UF2 returns to `ota_0` without rewriting
the bootloader, partition table, recovery image, or FFAT.

Before TinyUSB initialization, MCU.js commits a USB-startup `pending` marker in
NVS. TinyUSB is serviced from the task-watchdog-supervised main task rather than
an independent task. A reset before the first service pass leaves the marker;
the next boot clears it and enters TinyUF2. NVS failures also enter TinyUF2 and
never erase NVS automatically.

The XIAO ESP32-S3 also has physical BOOT and RESET buttons. They are extremely
small and recessed, but can be pressed with a pen tip or fingernail when direct
hardware recovery is needed. The software recovery paths above remain the
normal first choice.

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

The XIAO build exposes these native modules:

```text
board, fs, process, gpio, pwm, i2c, spi, adc, neopixel, mcujs:module, node:module
```

`require('mcujs:module').builtinModules` and `.help` are generated from the
build's feature flags and are the authoritative per-board lists. The ESP32 ADC
binding supports XIAO GPIO1-GPIO9 (ADC1 channels 0-8), calibrated voltage in
volts, and the internal chip-temperature sensor. XIAO has no RP-style ADC VSYS
divider or raw temperature channel, so ESP32 does not expose `adc.VSYS` or
`adc.TEMP`; use `adc.readTempC()`. PWM uses eight LEDC channels sharing up to
four frequency timers. I2C bus 0/1 and SPI bus 0/1 accept only the exposed XIAO
pins; GPIO43/GPIO44 remain reserved for the recovery UART. SPI array transfers
are limited to 64 bytes, matching the ESP32-S3 polling/no-DMA transaction limit.

The external `neopixel` module supports WS2812 strips on header GPIO1-GPIO9,
with GRB (default) or RGB ordering and a 256-pixel limit. The XIAO ESP32-S3 has
no onboard NeoPixel, so `board.neopixel()` and an onboard default pin remain
unavailable. Use a common ground, adequate external strip power, and a level
shifter when the strip requires 5 V logic; never power a strip from a GPIO.

Image, keyboard, and mouse modules are not enabled in this build. Image remains
deferred until graphics/display support; keyboard and mouse require a dedicated
CDC+MSC+HID USB-composite milestone.

The backend uses ESP-IDF FatFs with wear levelling, heap-backed long filenames,
and UTF-8 paths. It implements read, write, append, seek, rename, delete,
directory listing, nested stat, and CommonJS/JSON loading. Filesystem failures
throw JavaScript exceptions with Node-style error codes. Paths are measured
before copying, so overlong destructive paths fail with `ENAMETOOLONG` instead
of operating on a truncated prefix. Every CommonJS module receives a require
function bound to its own filename, including deferred relative loads.
The `.ls` REPL command hides dot-prefixed host metadata such as `.Trash-1000`,
matching normal `ls` behavior. Explicit filesystem paths and `readdirSync()`
still expose those entries; firmware does not delete or rewrite host trash.

The filesystem is formatted only when its NVS initialization marker is absent
**and** a pre-mount scan proves every byte of the FFAT partition is erased
(`0xff`). Missing NVS state is never treated as evidence that non-erased user
storage is disposable. Any non-erased mount failure leaves the partition
untouched and starts the runtime without storage.

### USB storage ownership

FFAT has exactly one owner:

- During boot, FatFs is device-owned so `/index.js` can run.
- After startup, FatFs/VFS is flushed, unmounted, and detached from diskio while
  the wear-levelling handle remains live. MSC then exposes that same WL logical
  volume to the host as `MCUJS`, using 4096-byte logical sectors.
- While host-owned, CDC and non-filesystem JavaScript remain available, but all
  JavaScript filesystem operations throw `EBUSY` and explain that `MCUJS` must
  be ejected first.
- Host `SYNCHRONIZE CACHE` is safe because every MSC write completes its
  WL-aware erase/write synchronously. `START STOP UNIT` eject immediately blocks
  new MSC I/O, drains in-flight operations, and remounts FatFs on the main task.
- Host load transfers ownership back to MSC only when no local file handle is
  open. Local filesystem access is restricted to its owning main task.

USB suspend or a transport reset is not treated as eject. Without a board-level
VBUS detector, physical disconnect cannot be distinguished safely from a bus
reset, so ownership remains host-side until an explicit eject or firmware
reset. Always sync/eject `MCUJS` before JavaScript filesystem access or unplug.
An interrupted acknowledged host write can preserve FAT structure while losing
file contents that were still in host caches; this is normal removable-storage
behavior, not something firmware can make transactional.

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

With the runtime available at `/dev/mcujs-dev`, first sync/eject the host
`MCUJS` volume so FatFs is device-owned, then run:

```sh
source "$IDF_PATH/export.sh"
python platform/esp32/hardware-smoke.py --port /dev/mcujs-dev --resets 0
```

This harness verifies the Milestone 2 runtime plus filesystem CRUD, nested
metadata, deferred relative CommonJS loading, path confinement, overlong-path
rejection, exact line-framed REPL results, and a watchdog-free idle window.
`hardware-smoke.py --resets 2` and `boot-smoke.py` remain Milestone 3 regression
harnesses; each reset in Milestone 4 returns storage to host ownership, so an MSC
orchestrator must eject between their reset steps. Milestone 4 qualification
additionally covers CDC while host-owned, exact JavaScript `EBUSY`, host
write/sync/eject/device read, device write/load/host read, ordinary and abrupt
resets, TinyUF2 recovery, repeated handoffs, and offline `fsck.fat -n`.
No harness reads or prints the board's unique identifier.

## Deliberately unavailable

Until later milestones, this backend excludes graphics, displays, USB keyboard,
and USB mouse support.
