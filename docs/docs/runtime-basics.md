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
const led = 25;
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

- Relative paths (`./`, `../`) resolve from the current file
- Absolute paths (`/`) resolve from the filesystem root
- Bare imports resolve from `/lib/` (create it if you want shared modules)
- JSON files are parsed into objects
- If you know CommonJS from Node, this will feel familiar

### Example filesystem layout

```text
/
├── index.js
├── config.json
├── lib/
│   ├── math.js
│   └── blink.js
└── apps/
    └── status.js
```

### Example imports

```javascript
// index.js
const fs = require('fs');
const math = require('math');
const blink = require('/lib/blink');
const status = require('./apps/status');
const config = require('./config.json');

const contents = fs.readFileSync('/apps/status.js');
console.log({ config, contentsLength: contents.length, sum: math.add(2, 3) });
blink.start(25);
status.report();
```

## Filesystem behavior

- `require('board').capability('fs')` is the static contract: it describes
  whether this firmware has a filesystem and supports host transfer.
- `board.storageReady()` is dynamic. It is `true` only while the device owns a
  mounted, usable filesystem; check that the method exists before calling it.
- USB mass storage and device-side JavaScript never access the volume at the
  same time. While the USB host owns it, `fs` operations and file-backed
  `require()` throw `ResourceBusyError` with `code === 'EBUSY'`,
  `resource === 'filesystem'`, and `owner === 'usb-host'`. Built-in modules such
  as `require('board')` remain available.
- Properly eject the MCU.js volume on the host before using device-side files.
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
