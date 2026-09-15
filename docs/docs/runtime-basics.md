---
sidebar_position: 4
---

# Runtime Basics

This section covers how you interact with the runtime day to day: the [REPL](./glossary.md#repl), modules, and filesystem behavior.

## [REPL](./glossary.md#repl) commands

| Command | Description |
| --- | --- |
| `.help` | Show available commands |
| `.capabilities` | Show the selected board and firmware-enabled modules |
| `.capabilities NAME` | Show one capability descriptor, such as `spi` |
| `.info` | Show board info (chip, memory, filesystem) |
| `.ls` | List files on the device |
| `.cat FILE` | Display file contents |
| `.rm FILE` | Delete a file |
| `.run FILE` | Execute a JavaScript file |
| `.multiline [FILE]` | Multi-line input (end with `.end`) |
| `.format` | Format filesystem (prompted, 3s countdown) |
| `.format!` | Format filesystem immediately |
| `.uf2` | Reboot into [UF2](./glossary.md#uf2) mode (prompted) |
| `.uf2!` | Reboot into [UF2](./glossary.md#uf2) mode immediately |
| `.usbreset` | Reset [USB](./glossary.md#usb) connection (reboot) |

### Quick REPL flow

```text
> .ls
index.js
lib/
> .cat index.js
console.log("Hello from /app");
> .run index.js
```

If you want a quick refresher on REPL terms, check the [Glossary](./glossary.md).

## Runtime JavaScript

- Console logging, timers, and globals are documented in [Runtime JavaScript](./runtime-javascript.md)
- Module APIs like `fs`, `gpio`, and `neopixel` live in [Built-in Modules](./built-in-modules.md)
- NeoPixel is handy for boards with RGB LEDs
- Use `board.neopixel` on boards with built-in NeoPixels (arrays or objects, missing values default to 0)
- In 0.2, oversized color or multi-pixel arrays throw `RangeError`; 0.1 firmware truncated them to `board.neopixelLength`
- Object inputs are RGB; array inputs match the active `neopixel.init()` order
- Array-of-objects stays RGB even when order is GRB
- If you're just getting started, scan the examples below and then follow the links

## Module loading

The built-in module list is board-accurate. Use `require('mcujs:module').has(name)`
before loading an optional module, and use `require('board').capability(name)` for
its routes and limits. Unsupported modules are absent rather than registered as
throwing stubs.

- Relative imports (`./`, `../`) resolve from the requiring file, including later callbacks.
- Internal absolute paths use `/app/...`; old `/lib/...` and `/config.json` paths are not aliases.
- Bare custom imports resolve from `/app/lib/`; built-ins retain precedence.
- JSON files are parsed into objects. Dependencies remain cached until the existing cache-clear/reset path.
- `.run` and startup execute CommonJS entries with local `require`, `module`, `exports`, `__filename` and `__dirname`. `.run` executes the entry again, not its cached dependencies.
- Entry variables are not REPL globals. Export state or deliberately use `globalThis` for a documented stop/rerun control.
- `..` cannot escape the `/app` mount. Relative `fs` paths start at `/app`, not the module's directory.

### Example filesystem layout

USB volume root (do **not** add an extra physical `app/` directory):

```text
MCUJS/
├── index.js          # runtime /app/index.js
├── config.json       # runtime /app/config.json
└── lib/
    └── math.js       # runtime /app/lib/math.js
```

For a complete copyable lesson, use the maintained
[modules example](https://github.com/mcu-js/mcujs/tree/development/examples/modules).

### Example imports

```javascript
// /app/index.js; these dependencies must be copied with the entry.
var fs = require('fs');
var math = require('./lib/math');
var sameMath = require('math');
var config = require('./config.json');
console.log(math === sameMath); // One canonical module cache key.
console.log(math.add(2, 3));
console.log(fs.readFileSync('/app/config.json', 'utf8'));
```

Startup reads only `/app/index.js` and respects storage ownership and safe boot.
The [migration procedure](./development/app-namespace.md#explicit-upgrade-procedure)
explains application preservation and changes to old absolute paths.

## Filesystem behavior

- `require('board').capability('fs')` is the static contract: it describes
  whether this firmware has a filesystem and supports host transfer.
- `board.storageReady()` is dynamic. It is `true` only while the device owns a
  mounted, usable internal `/app` filesystem; it does not report SD health.
  Check that the method exists before calling it.
- On supported firmware, `fs.sd.hostTransfer` advertises a separate SD USB
  volume. Drive roots map directly to `/app` and `/sd`; no extra `app/` folder
  is required. See [SD copy/eject guidance](./development/sd-assets.md#copying-files-over-usb).
- USB mass storage and device-side JavaScript never access the volume at the
  same time. While the USB host owns it, `fs` operations and file-backed
  `require()` throw `ResourceBusyError` with `code === 'EBUSY'`,
  `resource === 'filesystem'`, and `owner === 'usb-host'`. Built-in modules such
  as `require('board')` remain available.
- Properly eject the affected volume on the host before using its device-side files.
  Once ownership returns, `board.storageReady()` becomes `true` and host writes
  are visible through the fresh mount.
- Host OS directory caches can delay visibility of device-written files. If a
  file still appears stale after the next handoff, disconnect and reconnect the
  board.

## Key terms

- [REPL](./glossary.md#repl)
- [CommonJS](./glossary.md#commonjs)
- [UF2](./glossary.md#uf2)
- [USB](./glossary.md#usb)
