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

## Globals

- `process` for runtime metadata
- `board` for device helpers

### Example

```javascript
console.log(process.version);
console.log(board.name, board.freeMemory());
```

See [Built-in Modules](./built-in-modules.md) for module APIs like `fs`, `gpio`, and `adc`.

## Key terms

- [Runtime](./glossary.md#runtime)
- [REPL](./glossary.md#repl)
