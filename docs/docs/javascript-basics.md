---
sidebar_position: 1
---

# JavaScript Basics

This quick tour covers the core JavaScript you will use in mcujs. It is friendly for beginners, and still useful if you are coming from other languages.

:::note REPL tip
If you are copy-pasting examples in the REPL, reset the session (`.usbreset`) between sections to avoid redeclared variables.
:::

## Variables and values

Use variables to store simple data like strings, numbers, and booleans.

```javascript
const name = 'mcujs';
let counter = 0;
const enabled = true;
const tempC = 21.5;
```

Use `const` for values that do not change, and `let` for values you update.

## Arrays and objects

Arrays hold ordered data, objects hold named fields.

```javascript
const pins = [25, 2, 3];
const device = {
  name: 'pico',
  version: '0.1.0',
  ledPin: 25,
};
```

Arrays are ordered lists. Objects are key-value maps.

## Functions

Functions let you group logic so you can reuse it later.

```javascript
function blink(pin, delayMs) {
  console.log('Blinking', pin, 'every', delayMs, 'ms');
}

blink(25, 500);
```

Functions let you reuse logic and organize your scripts.

## Control flow

Use `if` and loops to make decisions and repeat work.

```javascript
const tempC = 22;

if (tempC > 30) {
  console.log('Too warm');
} else if (tempC < 10) {
  console.log('Too cold');
} else {
  console.log('All good');
}

for (let i = 0; i < 3; i += 1) {
  console.log('count', i);
}
```

## Timers

Timers schedule work in the future without blocking everything else.

```javascript
let ticks = 0;

const id = setInterval(() => {
  ticks += 1;
  console.log('tick', ticks);

  if (ticks >= 5) {
    clearInterval(id);
  }
}, 500);
```

Here is a one-shot timer with `setTimeout`.

```javascript
setTimeout(() => {
  console.log('done waiting');
}, 1000);
```

Timers are how you keep doing work without blocking the runtime.

## Modules with require()

Modules help you split code into smaller files.

```javascript
// index.js
const math = require('./math');
console.log(math.add(2, 3));

// math.js
exports.add = (a, b) => a + b;
```

## Files with fs

The `fs` module lets you save and load small files on the board.

```javascript
const fs = require('fs');

fs.writeFileSync('/note.txt', 'hello from mcujs');
const note = fs.readFileSync('/note.txt');
console.log(note);
```

## A tiny mcujs script

This is a complete LED blink program using GPIO.

```javascript
const GPIO = require('gpio');

const LED = 25;
GPIO.init(LED, GPIO.OUTPUT);

let on = false;
setInterval(() => {
  on = !on;
  GPIO.set(LED, on);
}, 500);
```

## Next steps

- Move on to [Runtime Basics](./runtime-basics.md) for REPL and module loading
- Explore hardware modules in [Built-in Modules](./built-in-modules.md)

## Key terms

- [Runtime](./glossary.md#runtime)
- [REPL](./glossary.md#repl)
- [GPIO](./glossary.md#gpio)
