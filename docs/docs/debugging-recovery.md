---
sidebar_position: 9
---

# Debugging and Recovery

Start with `.info` and `.help`: record the exact board/build and use only the
commands that firmware advertises. A recovery operation must preserve the
application before replacing firmware or formatting storage.

## Stop an example before resetting anything

Maintained timed examples document their completion and rerun cleanup. Let the
bounded run finish or use its documented stop function. Clearing a timer alone
need not release a display, input handle or peripheral pin; close/stop the owned
resource too. `.run FILE` executes an entry again, while `require()` caches its
dependencies. Do not use a format or firmware update as a normal stop button.

## Storage busy is not corruption

When USB storage belongs to the host, runtime file access may report `EBUSY`.
Save and properly eject `MCUJS` while leaving the serial connection available,
then check:

```javascript
require('board').storageReady()
```

Wait for `true` before `.ls`, `.run`, file reads or writes. A missing ownership
method means the firmware's prerequisite differs; check `.info` and the matching
manifest instead of assuming readiness. `/app` is the logical internal volume;
USB-root `index.js` is `/app/index.js`, not `/app/app/index.js`.

## Board-specific safe boot and recovery

### RP2040 / RP2350

The runtime checks BOOTSEL before application startup. **Holding BOOTSEL while
attaching USB can enter the chip's ROM bootloader instead**, showing `RPI-RP2`
or `RP2350`, not the MCU.js filesystem or REPL. Do not mistake the recovery
volume for `MCUJS` or copy application files onto it.

A supported `.uf2` / `.uf2!` command enters that recovery transport from the
running REPL. Verify the exact target and prepare backups/rollback before an
update. Firmware-size changes can relocate the physical application filesystem;
follow the [explicit migration procedure](./development/app-namespace.md#explicit-upgrade-procedure).
Physical held-button recovery is a per-board hardware check, not proof supplied
by a host test or a generic BOOTSEL instruction.

### XIAO ESP32-S3

XIAO has persistent failed-startup recovery. A failed or unqualified startup can
leave safe mode enabled on the next reset. Inspect `require('board').safeMode()`
only when the installed boot capability exposes it. Repair or deliberately remove
the failing `/app/index.js` before clearing safe mode; preserve a copy first.

`safeMode(false)` is not a bypass for an active qualification window: the current
contract reports `EBUSY`, and persistence failures report `EIO`. Keep the serial
error rather than retrying destructive operations. See the
[XIAO recovery procedure](https://github.com/mcu-js/mcujs/blob/development/platform/esp32/README.md#indexjs-and-persistent-safe-mode).
Its compatible TinyUF2 recovery volume is `XIAOS3BOOT`; stock ESP-IDF full flash,
partition changes and Pico-specific button instructions are not substitutes.

## Formatting is destructive

Use a format only after identifying actual filesystem damage, verifying backups
and explicitly choosing to erase that volume. If the installed `.help` advertises
`.format`, it is prompted; `.format!` skips confirmation. Both lose data.
Platform first-boot and damaged-filesystem policies differ: do not infer a safe
repair policy or filesystem capacity from another board. Formatting cannot fix
USB host ownership, a wrong source/API version, or a missing capability.

## Checks to keep separate

Serial responsiveness, software restart, USB reconnect, held-button recovery and
a charger-powered cold start are different observations. Record the actual build
and the operation performed; do not report an unperformed cold-start or electrical
check as passed. See [Runtime Basics](./runtime-basics.md) and
[the board inventory](./hardware-boards.md) for supported interfaces and limits.
