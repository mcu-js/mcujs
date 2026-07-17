# ESP32-S3 headless backend

This backend is the Milestone 2 bring-up target for the Seeed XIAO ESP32-S3.
It uses ESP-IDF's fixed USB Serial/JTAG console and deliberately does **not**
initialize USB-OTG, TinyUSB, MSC, or the filesystem.

The GPIO API is restricted to the XIAO's exposed D0-D10 pins and onboard LED;
JavaScript cannot reconfigure internal flash or PSRAM pins.

## Pinned dependencies

- ESP-IDF `v5.3.2` (`9d7f2d69f50d1288526d4f1027108e314e8c879f`)
- JerryScript `v3.0.0` (`50200152feb724a74a5f64e44d7885151537cfad`)
- TinyUF2 source `9af754c408ae76e862830de33dbd03b93a9a82e1`
- Microsoft UF2 submodule `84444ddb9d2914edf7f6d9e89a7ce41b53d64d98`

The helper scripts default to checkouts under `$HOME/toolchains`; the paths can
be overridden with `IDF_PATH`, `JERRYSCRIPT_PATH`, and `TINYUF2_PATH`.

## Build

```sh
platform/esp32/build.sh build
platform/esp32/make-uf2.py
```

`make-uf2.py` verifies the dependency pins and independently checks every UF2
block, family ID, target address, payload, and OTA boundary.

## Flash ownership

The product-compatible 8 MB table is:

```text
nvs       0x009000    20 KiB
otadata   0x00e000     8 KiB
ota_0     0x010000      4 MiB
tinyuf2   0x410000    256 KiB
ffat      0x450000   3776 KiB
```

ESP-IDF normally treats a `factory` partition as the default application flash
destination. Here that partition contains TinyUF2, so the project rewrites the
generated `flash` and `app-flash` metadata to the named `ota_0` partition and
fails configuration unless it remains at `0x10000`.

`platform/esp32/build.sh flash` is refused by default because a full ESP-IDF
flash also replaces the TinyUF2-aware second-stage bootloader, partition table,
and OTA metadata. Use `app-flash` to preserve recovery. The destructive full
flash requires an explicit `MCUJS_ALLOW_DESTRUCTIVE_FULL_FLASH=1` override.

For the original TinyUF2 0.35.0 baseline, copy the generated application-only
UF2 to `XIAOS3BOOT`. TinyUF2 remaps UF2 address zero into its current `ota_0`,
bounds the write to that partition, marks OTA0 bootable, and preserves the
bootloader, partition table, factory TinyUF2 image, and FFAT region.

A software reset can keep fixed USB Serial/JTAG enumerated. The first transition
from TinyUF2 USB-OTG to fixed Serial/JTAG may require a physical reset or USB
power cycle because both controllers share the internal PHY.

## Hardware smoke

With the runtime available at `/dev/mcujs-dev`:

```sh
source "$IDF_PATH/export.sh"
python platform/esp32/hardware-smoke.py --resets 2
```

The harness verifies arithmetic, board identity, GPIO policy/readback, timer
callbacks and edge cases, measured uptime restart across two reset cycles, and
a watchdog-free idle window. It intentionally does not read or print the
board's unique identifier.

## Deliberately unavailable

Until later milestones, this backend excludes filesystem/module loading,
boot scripts, custom USB CDC/MSC, TinyUF2 software entry, PWM, I2C, SPI, ADC,
NeoPixel, graphics, and displays. `board.enterUf2()` throws an explicit
Milestone 2 error rather than pretending to work.
