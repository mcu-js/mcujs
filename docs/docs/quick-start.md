---
sidebar_position: 2
---

# Quick Start

This guide gets you from zero to blinking in just a few minutes.

## Flash the firmware

1. Download the latest [UF2](./glossary.md#uf2) for your board from [GitHub Releases](https://github.com/mcu-js/mcujs/releases).
2. Hold **BOOTSEL** and connect the board via [USB](./glossary.md#usb).
3. Drag the [UF2](./glossary.md#uf2) to the `RPI-RP2` drive on RP2040 boards or the `RP2350` drive on RP2350 boards.
4. The device reboots and shows up as a USB drive named `MCUJS`.

## Create your first script

Create an `index.js` file on the `MCUJS` drive:

Need help with terms? See the [Glossary](./glossary.md).

```javascript
const LED = 25;

GPIO.init(LED, GPIO.OUTPUT);

setInterval(() => {
  GPIO.toggle(LED);
}, 500);

console.log('Blinking!');
```

Reset the board. Your script runs automatically.

## Open the [REPL](./glossary.md#repl)

Connect at 115200 baud. Example session:

```text
mcujs v0.1.0 on pico
> console.log('Hello!')
Hello!
undefined
```

## Next steps

- Add a second file and try `require()` from [Runtime Basics](./runtime-basics.md)
- Skim the built-in modules in [Built-in Modules](./built-in-modules.md)
- If you are curious about hardware APIs, start with [API Reference](./api-reference.md)

## Key terms

- [UF2](./glossary.md#uf2)
- [USB](./glossary.md#usb)
- [BOOTSEL](./glossary.md#bootsel)
- [REPL](./glossary.md#repl)
