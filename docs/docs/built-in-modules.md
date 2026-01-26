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
- `keyboard` for USB HID keyboard emulation
- `mouse` for USB HID mouse emulation

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

## USB HID Keyboard

The `keyboard` module lets the Pico act as a USB keyboard. No drivers needed on the host.

```javascript
const K = require('keyboard');

// Type text (handles shift for uppercase and symbols)
K.print('Hello, World!');

// Single key tap
K.tap('enter');

// Modifier combos
K.press('super');    // Hold Super/Win/Cmd key
K.tap('space');      // Tap space while Super is held
K.release('super');  // Release Super

// Safety: release all keys
K.releaseAll();

// Check key state
K.isPressed('shift');  // returns true/false
```

### Supported keys

- **Letters:** `a`-`z` (case insensitive)
- **Numbers:** `0`-`9`
- **Function keys:** `f1`-`f12`
- **Modifiers:** `ctrl`, `shift`, `alt`, `super` (also `gui`, `cmd`, `win`, `meta`)
- **Right modifiers:** `rctrl`, `rshift`, `ralt`, `rgui`
- **Navigation:** `up`, `down`, `left`, `right`, `home`, `end`, `pageup`, `pagedown`
- **Special:** `enter`, `tab`, `space`, `backspace`, `delete`, `escape`, `insert`
- **Locks:** `capslock`, `numlock`, `scrolllock`
- **Other:** `printscreen`, `pause`
- **Punctuation:** `-`, `=`, `[`, `]`, `\`, `;`, `'`, `` ` ``, `,`, `.`, `/`
- **Media:** `mute`, `volumeup`, `volumedown`, `playpause`, `nexttrack`, `prevtrack`, `stop`
- **Brightness:** `brightnessup`, `brightnessdown`

### Example: Macro button

```javascript
const K = require('keyboard');
const GPIO = require('gpio');

// Button on GPIO 15
GPIO.init(15, GPIO.INPUT_PULLUP);

setInterval(() => {
    if (GPIO.get(15) === 0) {
        K.print('console.log("Hello!");');
        K.tap('enter');
        while (GPIO.get(15) === 0) {} // debounce
    }
}, 10);
```

## USB HID Mouse

The `mouse` module lets the Pico act as a USB mouse.

```javascript
const M = require('mouse');

// Move cursor (relative, pixels)
M.move(100, 0);   // right
M.move(-50, 50);  // left and down

// Click buttons
M.click();           // left click (default)
M.click('right');    // right click
M.doubleClick();     // double left click

// Drag operation
M.press('left');
M.move(200, 0);      // drag right
M.release('left');

// Scroll
M.scroll(5);         // scroll up
M.scroll(-5);        // scroll down
M.scrollH(3);        // scroll right (horizontal)

// Release all buttons
M.releaseAll();
```

### Buttons

- `left` (or `l`) - Left mouse button (default)
- `right` (or `r`) - Right mouse button
- `middle` (or `m`) - Middle mouse button

### Example: Mouse jiggler

```javascript
const M = require('mouse');

// Prevent screen lock by moving mouse every 30 seconds
setInterval(() => {
    M.move(1, 0);
    M.move(-1, 0);
}, 30000);
```

## Key terms

- [GPIO](./glossary.md#gpio)
- [PWM](./glossary.md#pwm)
- [I2C](./glossary.md#i2c)
- [SPI](./glossary.md#spi)
- [ADC](./glossary.md#adc)
- [NeoPixel](./glossary.md#neopixel)
