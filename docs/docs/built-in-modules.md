---
sidebar_position: 6
---

# Built-in Modules

These modules are available with `require()` out of the box. Acronyms are explained in the [Glossary](./glossary.md).

## Core modules

- `console` for logging
- `fs` for files on the onboard drive
- `process` for runtime info
- `board` for board helpers (LED, reset, memory)

## Hardware modules

- `gpio` for pins and digital IO ([GPIO](./glossary.md#gpio))
- `pwm` for PWM output ([PWM](./glossary.md#pwm))
- `i2c` for sensors and peripherals ([I2C](./glossary.md#i2c))
- `spi` for fast serial devices ([SPI](./glossary.md#spi))
- `adc` for analog inputs ([ADC](./glossary.md#adc))
- `neopixel` for WS2812 LEDs ([NeoPixel](./glossary.md#neopixel))

## Example usage

```javascript
const fs = require('fs');
const GPIO = require('gpio');
const neopixel = require('neopixel');

fs.writeFileSync('/log.txt', 'Hello Pico');
GPIO.init(25, GPIO.OUTPUT);
GPIO.toggle(25);

neopixel.init({ pin: 16, length: 1, order: 'GRB' });
// order can be "GRB" (default) or "RGB"
neopixel.setPixel(0, 255, 80, 10);
neopixel.show();
```

If your board has a built-in NeoPixel, use `board.neopixel` as a shortcut. It accepts:

- `board.neopixel([r, g, b])`
- `board.neopixel({ r, g, b })`
- `board.neopixel([[r,g,b], ...])` for multiple pixels
- `board.neopixel([{r,g,b}, ...])` for multiple pixels

Missing color values default to 0, and extra pixels are ignored. The helper only exists on boards with onboard NeoPixels. You can also check `board.neopixelPin` and `board.neopixelLength` at runtime.
Object inputs always mean RGB. Array inputs follow the active `neopixel.init()` order.
Array-of-objects always stay RGB; array-of-arrays follows the order.

### More ideas

- Log sensor data to a file every few seconds
- Use `fs.readdirSync('/')` to inspect what is on the device
- Combine `gpio` and `pwm` for LED fades

If you are looking for timers or console logging, those are in [Runtime JavaScript](./runtime-javascript.md).

## Key terms

- [GPIO](./glossary.md#gpio)
- [PWM](./glossary.md#pwm)
- [I2C](./glossary.md#i2c)
- [SPI](./glossary.md#spi)
- [ADC](./glossary.md#adc)
- [NeoPixel](./glossary.md#neopixel)
