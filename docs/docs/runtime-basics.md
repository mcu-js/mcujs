---
sidebar_position: 4
---

# Runtime Basics

This section covers how you interact with the runtime day to day: the [REPL](./glossary.md#repl), modules, and filesystem behavior.

## [REPL](./glossary.md#repl) commands

| Command | Description |
| --- | --- |
| `.help` | Show available commands |
| `.info` | Show board info (chip, memory, filesystem) |
| `.ls` | List files on the device |
| `.cat FILE` | Display file contents |
| `.rm FILE` | Delete a file |
| `.run FILE` | Execute a JavaScript file |
| `.multiline [FILE]` | Multi-line input (end with `.end`) |
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
- Module APIs like `fs` and `gpio` live in [Built-in Modules](./built-in-modules.md)
- If you're just getting started, scan the examples below and then follow the links

## Module loading

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

- Files written by JavaScript persist immediately
- Host OS directory caches can delay visibility of device-written files
- If a file seems missing on your computer, unplug and replug the device

## Key terms

- [REPL](./glossary.md#repl)
- [CommonJS](./glossary.md#commonjs)
- [UF2](./glossary.md#uf2)
- [USB](./glossary.md#usb)
