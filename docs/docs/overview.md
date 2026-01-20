---
sidebar_position: 1
---

# Overview

mcujs is a JavaScript runtime for microcontrollers, built for Raspberry Pi Pico and Pico 2 boards. It turns your board into a little drive and a [REPL](./glossary.md#repl) so you can drop in an `index.js`, reboot, and see results right away.

## Why mcujs

- JavaScript-first workflow on MCUs
- Drag-and-drop scripts plus a [REPL](./glossary.md#repl) for fast iteration
- Minimal runtime built on JerryScript
- Modular design so the engine can be swapped later

## What it ships with

- A simple on-device drive for `index.js`
- A [REPL](./glossary.md#repl) over [USB](./glossary.md#usb) serial for quick experiments
- Core hardware APIs ([GPIO](./glossary.md#gpio), [PWM](./glossary.md#pwm), [I2C](./glossary.md#i2c), [SPI](./glossary.md#spi), [ADC](./glossary.md#adc), [NeoPixel](./glossary.md#neopixel))
- Works with boards that include onboard RGB LEDs
- CommonJS-style modules with `/lib/` resolution

## If you are coming from Node or Bun

- Your entry point is `index.js` instead of `index.ts`
- `require()` works for modules and JSON
- Use built-in modules like `gpio` and `fs` instead of npm packages
- Most of the flow is file-and-[REPL](./glossary.md#repl) based, not a full package manager

## Key terms

- [Firmware](./glossary.md#firmware)
- [Runtime](./glossary.md#runtime)
- [REPL](./glossary.md#repl)
- [CommonJS](./glossary.md#commonjs)
