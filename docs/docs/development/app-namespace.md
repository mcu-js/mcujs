---
title: Application filesystem namespace
---

# Internal `/app` namespace (first slice of #7)

Every configured internal application volume is mounted logically at **`/app`**.
The RP2 FatFs volume root and ESP32 internal FAT volume root implement the same
paths. This is a runtime namespace, not a new physical directory or partition.

```js
var fs = require('fs');
fs.writeFileSync('/app/settings.json', JSON.stringify({ color: 'aqua' }));
var settings = JSON.parse(fs.readFileSync('settings.json', 'utf8'));
```

Relative filesystem paths always start at `/app`. There is no mutable cwd or
`chdir` in this slice. `fs.readdirSync('/')` lists the configured `app` mount;
`fs.readdirSync('/app')` lists its files. An absent or USB-host-owned volume is
not replaced by another backend. Existing ownership errors remain visible.
`board.capability('fs').appRoot` reports `/app`; its existing `implementation`,
`writable` and `hostTransfer` fields describe configured support, not whether
storage is currently available. REPL `.info` reports filesystem free space (or
host ownership). A JS volume-capacity API is not added in this slice. No SD
capability is advertised here.

## Paths, modules and startup

- `/app/index.js` is the only automatic application entry point. Existing
  safe-boot gates still run before the entry point. No second legacy startup is
  attempted after an error or absent configured storage.
- Boot and `.run FILE` execute a CommonJS entry: `require`, `module`, `exports`,
  `__filename` and `__dirname` are local to that file. Entry-local variables no
  longer become REPL globals. Each `.run` executes the entry again; dependency
  modules remain cached until the existing cache-clear/reset path is used.
- `require('./child')` and `require('../settings.json')` resolve relative to the
  requiring module, including inside later callbacks. Global REPL `require('./x')`
  starts at `/app`. Bare non-built-in names search `/app/lib`; built-ins keep
  their existing names and precedence.
- Absolute imports use `/app/...`. Canonical separators, `.` and internal `..`
  segments give one module cache key. Traversing above `/app` fails immediately,
  even if the path would later return to `/app`.
- Logical `/` and `/app` cannot be opened as files, removed, renamed or created
  over. Unknown mounts (including `/sd`) are unavailable, not aliases.
- Backslashes and raw FAT drive prefixes such as `0:` are rejected. Paths fail
  rather than truncate. The namespace/module ceiling is 127 bytes excluding NUL;
  the existing synchronous JS filesystem argument ceiling remains 63 bytes.
- REPL `.ls` lists `/app`; `.cat`, `.rm`, `.run`, and `.multiline FILE` accept
  logical `/app/...` paths or application-relative paths.

This does not add SD adapters, streams, asynchronous filesystem I/O, cross-volume
rename support, transparent overflow storage or automatic migration. Existing
small synchronous file operations retain their memory limits and are not
power-loss-atomic transactions.

## Explicit upgrade procedure

This is a breaking pre-1.0 path change. Old absolute runtime paths such as
`/settings.json` and `/lib/widget.js` must become `/app/settings.json` and
`/app/lib/widget.js`. There are no compatibility aliases.

1. Back up the current application and verify the copies before updating
   firmware. RP2 firmware-size changes can relocate the physical filesystem;
   use verified full-flash recovery and the separately approved restore plan.
2. Update absolute paths in the application's source. Relative file paths now
   mean `/app/...`; relative imports mean the module's own directory. Replace
   any reliance on entry variables becoming REPL globals with explicit exports
   or deliberately assigned globals where appropriate.
3. Keep files in the **physical volume root**. A USB-visible `index.js` is runtime
   `/app/index.js`; USB-visible `settings.json` is `/app/settings.json`. Do not
   create an extra USB `app/` directory: that would be runtime `/app/app/`.
4. Review the existing physical `index.js` before enabling normal startup. It
   becomes the mounted `/app/index.js`; this mapping does not edit, move or copy
   it. Remove/replace it only through an explicitly approved file update.
5. Eject USB storage before runtime filesystem access. Reboot without a USB host
   claiming the volume for the standalone startup check. Safe boot and REPL
   remain the recovery routes; there is no fallback to another app volume.

The namespace change itself neither formats media nor relocates files. Existing
platform first-boot/format policies are unchanged by this slice; that is **not** a
promise that flashing an arbitrarily larger RP2 image preserves its filesystem.

## Saved-settings drawing demo

After backing up any existing files, explicitly install:

- `examples/portable/app-draw/index.js` as USB `index.js` (runtime `/app/index.js`).
- `examples/portable/pointer-draw/draw.js` as USB `draw.js` (runtime `/app/draw.js`).

The demo creates `/app/settings.json` only when it is absent. It validates an
existing file rather than silently replacing malformed settings. Settings contain
`color` (`white`, `lime`, `aqua`, `yellow`), integer `lineWidth` (1–8), and integer
`starts`. Each successful startup increments and saves `starts`. A strip in the
saved color marks readiness; touches draw using the same portable consumer.
It prints `APP_READY` with the saved settings and runs for 60 seconds, then closes
its display. Reset runs it again. The demo saves settings, **not drawn strokes**.

For example, while storage belongs to the runtime:

```js
require('fs').writeFileSync('/app/settings.json',
  JSON.stringify({ color: 'aqua', lineWidth: 5, starts: 0 }));
```

Then reset and verify the saved color, line width and incremented start count.
Host tests are not physical reboot/persistence acceptance; record the actual
candidate build ID and separate power-cycle result when hardware QA is performed.

### Hardware acceptance and limits

Hardware evidence is from the RP2350 Touch LCD 1.69 running candidate `c746dd7`.
The development integration preserves its executable source; acceptance notes
and the changelog are updated separately. Builds for other boards are compile
coverage, not physical qualification.

- Before the initial namespace update, two complete flash reads matched and all
  14 original files were backed up, restored and hash-checked after remounting.
- Candidate `df7e1e5` proved logical paths, direct entry execution and settings
  writes, but automatic startup was blocked by the existing BOOTSEL sampler
  reading the RP2040 SIO bit on RP2350. The fix uses the RP2350 SDK-defined bit
  without bypassing safe boot. Independent held/released register fixtures pass
  for both MCU families; **physical held-BOOTSEL recovery remains unverified**.
- The update to `c746dd7` used fresh matching full-flash backups. Its physical
  filesystem boundary did not move, and the storage tail was verified unchanged
  before first boot; no second filesystem recreation was needed.
- After a software reset, serial output reported `Running /app/index.js...` and
  `APP_READY {"color":"aqua","lineWidth":5,"starts":4}`. Saved settings survived
  and the start count advanced from 3 to 4 without a manual `.run`.
- The operator then confirmed charger-powered cold-start, automatic appearance
  of the aqua readiness strip, and visible aqua drawing following the finger.
  This is a bounded user-observed demonstration, not a long-duration soak test.

Earlier USB-connected drawing and draw-only probes reported touch-read failures.
Changing settings-write order did not resolve them; neither filesystem writes
nor USB ownership is an established cause. The successful standalone check does
**not** resolve those failures. Touch reliability, physical held-button recovery,
other boards' hardware, precision calibration and power-loss atomicity are not
claimed by this slice. `/sd` remains separate follow-up work under issue #7.
