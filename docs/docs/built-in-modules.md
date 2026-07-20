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

### I2C contract

The canonical I2C module is `require('i2c')`. Its portable surface is exactly
`i2c.init(options)`, `i2c.write(bus, address, data)`, and
`i2c.read(bus, address, length)`. The positional
`i2c.init(bus, sda, scl, frequency)` form remains as a 0.x migration alias.

Read `board.capability('i2c')` before configuration. It exposes `buses`, every
complete listed route, `defaultBus`, `defaultRoute`, inclusive frequency bounds,
and `maxTransferBytes`. Portable code normally selects the board default without
copying pin numbers:

```javascript
(function () {
  var boardApi = require('board');
  var i2c = require('i2c');
  var limits = boardApi.capability('i2c');

  i2c.init({frequency: 100000});
  i2c.write(limits.defaultBus, 0x42, [0x00, 0xff]);
  console.log(i2c.read(limits.defaultBus, 0x42, 2));
}());
```

`frequency` is a required finite integer in the advertised range. Omitting
`bus`, `sda`, and `scl` selects `defaultRoute`; selecting a non-default bus
requires both pins from one listed route. Unknown options, partial routes,
unlisted route combinations, and out-of-range values throw uncoded
`TypeError`/`RangeError`. An in-range frequency that cannot be represented
exactly throws `ERR_NOT_SUPPORTED` before replacing a working configuration.

Addresses are strict 7-bit integers `0..127`. Payload bytes are strict integers
`0..255`, and write/read lengths are `1..maxTransferBytes`; oversized requests
throw rather than being truncated. Use before initialization throws `EBUSY`.
A distinguishable address NACK/no-target result throws `ENXIO`; generic native
transfer failure throws `EIO`, while a distinguishable controller/state-busy
condition throws `EBUSY`. Timeout codes are backend-specific but stable: the RP
timeout result is `EBUSY`, while an ESP transfer timeout is `EIO`; ESP teardown
or configuration timeout remains `EBUSY`. Operational errors include the I2C
resource, bus, and applicable native diagnostics.

Successful reinitialization tears down the old controller and releases its old
route. A teardown failure retains any still-live controller state and every
unreleased pin claim so the call can be retried; another binding cannot silently
remux a possibly live bus. See
the non-destructive
[I2C peripheral acceptance protocol](./development/i2c-peripheral-protocol.md)
for target-emulator, NACK, transfer-boundary, and logic-analyzer evidence.

### SPI contract

The canonical SPI module is `require('spi')`. Its portable surface is exactly
`spi.init(options)` and `spi.transfer(bus, data)`. The positional
`spi.init(bus, sck, mosi, miso, frequency)` form remains as a 0.x migration
alias. Initialization is master-mode, full-duplex, fixed at 8-bit words and
MSB-first order; those formats are advertised by `bitsPerWord` and `bitOrders`
but are not selectable in 0.2.

Read `board.capability('spi')` before configuration. It exposes `buses`, complete
routes, `defaultBus`, `defaultRoute`, inclusive frequency bounds,
`maxTransferBytes`, and the implemented modes and format. Portable code can use
the board default without copying pin numbers:

```javascript
(function () {
  var boardApi = require('board');
  var spi = require('spi');
  var limits = boardApi.capability('spi');

  spi.init({frequency: 1250000, mode: 0});
  console.log(spi.transfer(limits.defaultBus, [0x9f, 0x00, 0x00, 0x00]));
}());
```

`frequency` is a required finite integer in the advertised range. Omitting
`bus`, `sck`, `mosi`, and `miso` selects `defaultRoute`; a non-default bus needs
all three pins from one listed route. `mode` defaults to 0 and must be listed in
`modes`. Unknown options, partial or unlisted routes, unadvertised modes, and
out-of-range values throw uncoded `TypeError`/`RangeError`. An in-range frequency
that the selected hardware cannot represent exactly throws `NotSupportedError`
with `code === 'ERR_NOT_SUPPORTED'` rather than rounding it.

`transfer()` accepts either one strict byte or an array of
`1..maxTransferBytes` strict bytes. Scalar input returns one byte; array input
returns an equally sized byte array. Empty or oversized arrays and bytes outside
`0..255` throw before native I/O. The current RP capability advertises 256-byte
transfers; the ESP32-S3 polling/no-DMA backend truthfully advertises its 64-byte
native limit. Use before initialization and live route/bus contention throw
`EBUSY`; driver allocation failure throws `ERR_RESOURCE_EXHAUSTED`; generic
configuration or transfer failure throws `EIO` with native diagnostics.

Successful reinitialization releases the previous route only after native
ownership is stopped. ESP teardown or rollback failure retains every still-live
device/bus handle and pin claim so a later `init()` can retry; other bindings
cannot remux quarantined hardware. RP route changes likewise invalidate stale
GPIO state, which must be explicitly reinitialized after release. See the
non-destructive
[SPI loopback and logic-analyzer protocol](./development/spi-loopback-protocol.md)
for exact-byte, maximum-boundary, mode, timing, and lifecycle evidence.

### NeoPixel contract

The canonical external addressable-LED module is `require('neopixel')`. Its
portable surface is exactly `init(options)`, `setPixel(index, red, green, blue)`,
`show()`, and `clear()`. External-driver availability is independent from
onboard inventory: feature-detect the module and capability, while checking
`board.devices.neopixel` separately before using an onboard shortcut.

```javascript
(function () {
  var boardApi = require('board');
  var modules = require('mcujs:module');
  if (!modules.has('neopixel')) return;

  var limits = boardApi.capability('neopixel');
  var pixels = require('neopixel');
  pixels.init({pin: limits.pins[0], length: 1, order: limits.orders[0]});
  pixels.setPixel(0, 32, 0, 0);
  pixels.show();
}());
```

`options` is closed and requires a finite integral `pin` from `pins` and a
finite integral `length` from `1..maxLength`. `order` defaults to `GRB` and, when
provided, must be an exact string in `orders`; unknown keys, lowercase aliases,
unlisted pins/orders, zero length, and `maxLength + 1` throw uncoded
`TypeError`/`RangeError` before replacing a working configuration. Current
shipping descriptors advertise both `RGB` and `GRB`, but portable code reads the
running capability rather than assuming that set.

`setPixel()` always accepts red, green, and blue as strict integer bytes
`0..255`; wire order is selected only by `init()`. The index must be a finite
integer in `0..(configured length - 1)`. Values are never coerced, clamped, or
wrapped. `show()` transmits the current buffer. `clear()` drives the configured
strip dark; call `show()` afterward when code needs an explicit portable frame
boundary before setting the next colors.

Calling `setPixel()`, `show()`, or `clear()` before successful initialization
throws `ResourceBusyError` with `code === 'EBUSY'`. Live pin contention also
throws `EBUSY`; finite waveform/buffer exhaustion throws
`ERR_RESOURCE_EXHAUSTED`; unsupported native configuration throws
`ERR_NOT_SUPPORTED`; native driver failure throws `EIO`. Successful
reinitialization releases the old pin. Validation or contention failure leaves
the existing strip usable, while creation failure leaves a safe uninitialized
state. A teardown failure retains the still-live handle and claim so a later
`init()` can retry cleanup rather than allowing another binding to remux active
hardware.

Use the non-destructive
[NeoPixel electrical and runtime acceptance protocol](./development/neopixel-hardware-protocol.md)
for maximum/max+1, RGB/GRB known-color photographs, waveform timing, electrical
safety, pin lifecycle, watchdog, CDC, and MSC evidence. Native stubs do not prove
physical color, wire order, voltage levels, or signal integrity.

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
