---
sidebar_position: 6
---

# Built-in Modules

These modules are available with `require()` out of the box when the selected
firmware enables them. `require('mcujs:module').builtinModules`,
`require('mcujs:module').has(name)`, `.help`, and `board.capability(name)` all
come from the same board registry. Acronyms are explained in the
[Glossary](./glossary.md).

The returned `builtinModules` array is a frozen registry snapshot exposed
through a non-writable, non-configurable property. `has(name)` requires one
non-empty string shorter than the module-path limit: missing or non-string
arguments throw `TypeError`, while invalid string lengths throw `RangeError`.

```javascript
const boardApi = require('board');
const modules = require('mcujs:module');

if (modules.has('spi')) {
  const spi = require('spi');
  const limits = boardApi.capability('spi');
  console.log(limits.defaultRoute, limits.maxTransferBytes);
}
```

Do not branch on `board.name` to select an API. An unavailable module is absent;
a present module throws a typed error when a requested route is invalid or a
live resource is busy.

## Board discovery

`require('board')` is the canonical board API. The global `board` value remains
the same object as a 0.x compatibility alias. `apiVersion` identifies the
portable contract, `pins` contains semantic aliases, and `devices` describes
only physically onboard hardware:

```javascript
(function () {
  var boardApi = require('board');
  var modules = require('mcujs:module');

  if (!modules.has('i2c')) {
    console.log('I2C is not compiled into this firmware');
    return;
  }

  var capability = boardApi.capability('i2c');
  var i2c = require('i2c');
  console.log('Default route:', capability.defaultRoute);
  console.log('I2C init available:', typeof i2c.init === 'function');
}());
```

`board.capability(name)` parses and freezes only the requested descriptor so
ordinary feature detection does not materialize the complete registry on the
JerryScript heap. It returns `undefined` for an unknown capability.
`board.capabilities()` returns a larger, deeply frozen snapshot for diagnostics
and tooling. Capability names must be non-empty strings; invalid names throw
`TypeError` or `RangeError` rather than being truncated.

`builtinModules` lists every native module that the selected firmware can
actually require, including compatibility aliases such as `node:module`.
`modules.has(name)` is an exact predicate over that same frozen list; an absent
module follows the normal module-not-found path from `require()`.

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

External-driver support and onboard hardware are separate. For example, every
current target can enable the external `neopixel` module even when
`board.devices.neopixel` is absent.

### GPIO contract

When `gpio` is present, its portable surface is exactly `init(pin, mode)`,
`set(pin, value)`, `get(pin)`, and `toggle(pin)`, plus the four mode constants.
Select pins from `board.capability('gpio').pins` and output pins from
`.outputPins`; these lists contain only the board's safe exposed pins.

`set()` accepts only the booleans `true` and `false`. Numeric `0`/`1`, strings,
missing arguments, non-finite numbers, and fractional pins are rejected before
hardware access. `get()` always returns a boolean. Every access requires a
successful `init()` for that pin, and `set()`/`toggle()` additionally require
output mode. Use before initialization throws `ResourceBusyError` with
`code === 'EBUSY'` rather than silently initializing the pin.

GPIO is a soft pin owner. PWM or a routed peripheral may deliberately take over
an initialized GPIO after validating its complete configuration. From then on,
stale GPIO calls throw `EBUSY`. Releasing the peripheral does not resurrect the
old GPIO setup: call `gpio.init()` again before using the pin. An active
peripheral owner cannot be stolen by GPIO or another peripheral.

### PWM contract

The canonical PWM module is `require('pwm')`. Its portable surface is exactly
`pwm.init(pin, frequency)`, `pwm.setDuty(pin, duty)`, and `pwm.stop(pin)`.
Feature-detect the module with `require('mcujs:module').has('pwm')`, then select
only pins listed by `board.capability('pwm').pins`; do not infer a pin from the
board or chip name.

`frequency` is an integer in the advertised `frequency.minHz..maxHz` range.
Values outside that range throw uncoded `RangeError`. Hardware frequency sets
are sparse: an in-range value that cannot be represented exactly throws
`NotSupportedError` with `code === 'ERR_NOT_SUPPORTED'` instead of being
rounded or clamped.

`duty` is only a finite ratio from `0` through `1`. Values outside that range
throw uncoded `RangeError`; a valid ratio that the selected PWM period cannot
represent exactly throws `ERR_NOT_SUPPORTED`. Use capability-selected periods
and simple binary ratios such as `1 / 64` when exact cross-board behavior is
required.

`maxOutputs` reports independently addressable PWM outputs and `timerCount`
reports frequency-generating resources. Outputs may share a timer only at the
same frequency. Exhausting either pool throws `ResourceExhaustedError` with
`code === 'ERR_RESOURCE_EXHAUSTED'` and a numeric `limit`; conflicting RP slice
frequencies or aliased output pins throw `ResourceBusyError` with `code ===
'EBUSY'`. A successful same-pin reinitialization resets duty to zero before the
new waveform is used. Failed representability checks leave the existing output
unchanged.

`pwm.stop(pin)` drives the output low and releases the pin. A shared timer stays
active until its final output stops. Native teardown failure retains ownership
and returns a typed operational error so `stop()` can be retried. As with every
peripheral release, GPIO does not regain its previous configuration: call
`gpio.init()` explicitly before reusing the pin.

### ADC contract

The canonical ADC module is `require('adc')`. Its portable surface is exactly
`readPin(pin)`, `readChannel(channel)`, `readVoltagePin(pin)`,
`readVoltageChannel(channel)`, and `readTempC()`. Raw reads return integer ADC
counts in `0..(2^resolutionBits - 1)`. Voltage reads always return volts, and
`readTempC()` returns MCU die temperature in degrees Celsius.

Select pins and channels from `board.capability('adc')`; do not infer routes from
the board name. Each external channel entry contains its numeric channel, pin,
and semantic aliases. Every alias resolves through `board.pins`:

```javascript
(function () {
  var boardApi = require('board');
  var adc = require('adc');
  var capability = boardApi.capability('adc');
  var route;

  for (var i = 0; i < capability.channels.length; i++) {
    if (capability.channels[i].aliases.indexOf('A0') !== -1) {
      route = capability.channels[i];
      break;
    }
  }
  if (!route) throw new Error('This board has no A0 ADC route');
  console.log(adc.readVoltageChannel(route.channel), 'V');
}());
```

`voltage.calibrated` is calibration truth, not a unit switch. ESP32-S3 uses the
native calibration driver and advertises `true`; RP2040/RP2350 use the runtime's
nominal 3.3 V conversion and advertise `false`. Both return volts. The
temperature sensor reports chip temperature, not ambient temperature.

Missing values, strings, non-finite numbers, and fractional pin/channel values
are rejected before hardware access. Unadvertised routes throw uncoded
`RangeError`. Active peripheral ownership and busy native drivers throw `EBUSY`;
native conversion failures throw `EIO` with diagnostics. A completed one-shot
read releases its synchronous claim after restoring the pad, so a deliberately
initialized routed peripheral can take over safely; stale GPIO state still
requires an explicit `gpio.init()`.

RP firmware may temporarily expose deprecated numeric `adc.TEMP` and `adc.VSYS`
properties. They are nonportable 0.x compatibility aliases for raw internal
channel 4 and a board-specific VSYS/3 channel 3 respectively. Feature-detect the
property itself. These internal aliases are separate from `capability.channels`,
which describes board-exposed pin/channel pairs. Portable code uses
`readTempC()` and advertised routes. See the non-destructive
[ADC voltage and temperature acceptance protocol](./development/adc-voltage-protocol.md)
for known-voltage, temperature, and ownership hardware evidence.

## Example usage

```javascript
(function () {
  var boardApi = require('board');
  var led = boardApi.devices.led;
  if (!led) {
    console.log('This board has no onboard LED');
    return;
  }
  if (led.type !== 'gpio') {
    boardApi.led(true);
    return;
  }

  var gpio = require('gpio');
  gpio.init(led.pin, gpio.OUTPUT);
  gpio.set(led.pin, led.activeLow ? false : true);
  console.log('Logical LED state:', true);
}());
```

If your board has a built-in NeoPixel, use `board.neopixel` as a shortcut. It accepts:

- `board.neopixel([r, g, b])`
- `board.neopixel({ r, g, b })`
- `board.neopixel([[r,g,b], ...])` for multiple pixels
- `board.neopixel([{r,g,b}, ...])` for multiple pixels

Missing color values default to 0. Under the 0.2 contract, a color array longer than three bytes or a multi-pixel list longer than the onboard length throws `RangeError` rather than being truncated. Older 0.1 firmware ignored extra values. The helper only exists on boards with onboard NeoPixels. Portable 0.2 code reads `board.devices.neopixel.length`; `board.neopixelPin` and `board.neopixelLength` remain compatibility aliases through 0.x.
Object inputs always mean RGB. Array inputs follow the active `neopixel.init()` order.
Array-of-objects always stay RGB; array-of-arrays follows the order.

## Capability-gated compatibility extensions

The portable 0.2 SPI surface is `init()` and `transfer()`, with fixed 8-bit words and MSB-first order. Existing RP display builds may also expose `spi.writeBufferDMA(bus, bufferHandle, byteLength)` when the SPI capability reports `dma: true` and includes `writeBufferDMA` in `compatibilityExtensions`. Its opaque graphics handle is not portable; feature-detect the method and do not infer it from the board name. The requested length may equal—but not exceed—the selected handle's byte length. Invalid handles and oversized lengths throw `RangeError`; use before `spi.init()` throws `EBUSY`; DMA-channel exhaustion throws `ERR_RESOURCE_EXHAUSTED`.

Firmware with persistent boot-script recovery may expose `board.safeMode()` and `board.safeMode(enabled)` when `board.capability('boot').safeMode` is true. This getter/setter is a nonportable compatibility extension. The capability is static; the returned boolean is dynamic state. Clearing during the active boot qualification window throws `EBUSY`; a persistent-state write failure throws `EIO`.

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
let wasPressed = false;

setInterval(() => {
    const pressed = !GPIO.get(15);
    if (pressed && !wasPressed) {
        K.print('console.log("Hello!");');
        K.tap('enter');
    }
    wasPressed = pressed;
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
