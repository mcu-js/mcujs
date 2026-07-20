---
sidebar_position: 5
---

# Runtime JavaScript

These are the everyday JavaScript helpers that feel like a tiny Node-style runtime.

## Console

```javascript
console.log('message');
console.warn('warning');
console.error('error');
```

Console output shows up in your [REPL](./glossary.md#repl) session and over [USB](./glossary.md#usb) serial.

## Timers

```javascript
const timeoutId = setTimeout(() => {
  console.log('once');
}, 250);

clearTimeout(timeoutId);

const intervalId = setInterval(() => {
  console.log('tick');
}, 500);

clearInterval(intervalId);
```

Timers are great for LED blinks and polling sensors. Keep intervals short and work lightweight.

## Runtime objects

- `require('process')` for runtime metadata
- `require('board')` for board identity, semantic pins, onboard inventory, and
  capability discovery

The lowercase CommonJS modules are canonical in 0.2. The `process` and `board`
globals remain compatibility aliases through the 0.x series.

### Example

```javascript
const processApi = require('process');
const boardApi = require('board');

console.log(processApi.version);
console.log(boardApi.name, boardApi.freeMemory());
```

See [Built-in Modules](./built-in-modules.md) for module APIs like `fs`, `gpio`, and `adc`.

## Key terms

- [Runtime](./glossary.md#runtime)
- [REPL](./glossary.md#repl)
