---
sidebar_position: 2
---

# Quick Start

Select matching firmware, run a bounded example, then make it your startup app.
These instructions describe the **development 0.2 API**, not a claim that 0.2.0
has been released. Published assets are listed on
[GitHub Releases](https://github.com/mcu-js/mcujs/releases); do not combine an old
release image with examples from a newer source revision.

## 1. Identify and preserve the board

If MCU.js already runs, open its USB serial REPL at **115200 baud** and enter
`.info`. Record the board ID and complete build ID, then check:

```javascript
require('board').apiVersion
require('board').capability('fs')
```

The application namespace below requires `fs.appRoot` to be `/app`. The version
banner alone is insufficient: development builds can still say `0.1.0+<commit>`.
Use that exact build's board-qualified capability manifest and matching examples.
For build instructions, see [Building from source](./advanced-building.md).

**Before updating, back up existing application files and verify the copies.**
RP firmware-size changes can move the physical filesystem. Preserve a verified
full-flash recovery image and prepare file restoration before such an update;
copying a UF2 is not a promise of data preservation. See
[the explicit migration procedure](./development/app-namespace.md#explicit-upgrade-procedure).
Never format to resolve a USB ownership error.

## 2. Use the board's install/update path

### RP2040 or RP2350

1. Select the UF2 for the exact board ID—not merely the same chip family.
2. Hold **BOOTSEL** while connecting USB to enter ROM recovery.
3. Copy the UF2 to `RPI-RP2` (RP2040) or `RP2350` (RP2350).
4. Wait for the `MCUJS` volume and CDC serial port to return. Check `.info`
   against the intended build and restore your backed-up files if required.

### Seeed XIAO ESP32-S3

The application-only UF2 requires an **existing compatible TinyUF2 baseline**.
A blank or differently partitioned XIAO needs the separately documented
[preservation and provisioning procedure](https://github.com/mcu-js/mcujs/blob/development/platform/esp32/README.md#flash-ownership),
not the Pico BOOTSEL procedure or a generic full-flash command.

1. In the running MCU.js REPL, enter `require('board').enterUf2()`.
2. Wait for `XIAOS3BOOT`, then copy the matching
   `mcujs-<version>-seeed_xiao_esp32s3.uf2` to it.
3. Wait for `MCUJS` and serial to return. Check `.info` and preserved files.

That application update does not authorize replacing partitions, the bootloader,
TinyUF2 recovery, or user storage. Other ESP boards have their own procedures;
experimental boards are not interchangeable XIAO targets.

## 3. Run first light without replacing your startup app

Use the maintained [first-light lesson](https://github.com/mcu-js/mcujs/tree/development/examples/blink).
Copy its matching `index.js` into the **physical `MCUJS` volume root** as
`first-light.js`. Keep any existing `index.js`; do not create an extra `app/`
directory on the USB drive.

The USB file `first-light.js` is runtime **`/app/first-light.js`**. Properly eject
storage in your operating system, leaving USB connected for serial, then enter:

```javascript
require('board').storageReady()
```

Continue only when this returns `true`:

```text
.ls
.run /app/first-light.js
```

On Pico and XIAO the same source uses the declared onboard LED and polarity.
It blinks every 500 ms, remains responsive, turns off after 30 seconds and prints
`Demo complete!`. A board without a declared LED reports that requirement instead
of choosing an arbitrary GPIO. Rerun with `.run`, not `require()`; dependencies
remain cached, but `.run` executes the entry again and the lesson replaces its
previous timers. The lesson also documents early stop and the one-line timing edit.

## 4. Make it your application

Only after backing up and deliberately replacing any existing startup file,
copy the tested entry as USB **`index.js`**, runtime **`/app/index.js`**. Copy its
required modules beside it. For example, an entry's `require('./helper')` loads
its sibling `helper.js`; bare custom modules are searched for in `/app/lib`.
The [modules and config examples](https://github.com/mcu-js/mcujs/tree/development/examples)
show relative CommonJS and JSON dependencies without overwriting unrelated files.

Eject storage and check `storageReady()` again, then verify startup with a normal
restart. Current RP and XIAO firmware read `/app/index.js` **before exposing USB
storage to the host**. Later file reads or writes still require device ownership;
eject storage again if the host reclaimed it. Missing/failed startup does not
trigger a second legacy entry point. Use [Debugging and Recovery](./debugging-recovery.md) rather than
formatting or reflashing to stop a script.

## Next steps

- [Runtime Basics](./runtime-basics.md): paths, imports, caching and the REPL
- [Migration guide](./migration/0.2.md): explicit breaking changes, errors and units
- [Hardware boards](./hardware-boards.md): matching targets and optional limits
- [Configured display Canvas](./development/display-canvas.md): the default drawing path
- [Glossary](./glossary.md): USB, UF2, BOOTSEL and other terms
