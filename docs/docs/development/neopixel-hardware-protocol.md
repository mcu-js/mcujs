# NeoPixel electrical and runtime acceptance protocol

This protocol is the physical acceptance gate for the portable MCU.js 0.2
external NeoPixel/WS2812 contract. It is deliberately non-destructive: it uses
an already-installed, reviewed firmware image and does not flash firmware,
format storage, or modify recovery state.

## Electrical safety and prerequisites

- **Do not power a strip from a GPIO.** Use an appropriately rated external
  supply for the strip and connect the supply ground, strip ground, and MCU
  ground together.
- Keep every MCU input below its absolute maximum. A 5 V WS2812 data input may
  work from 3.3 V logic only when its actual threshold allows it; use a proper
  3.3 V-to-5 V logic-level shifter for release evidence.
- Add the strip vendor's recommended bulk capacitor across its supply and a
  small series resistor near the first pixel's data input. Budget worst-case
  current from the strip datasheet; do not qualify a 256-pixel limit from an
  undersized USB rail.
- Select only a pin from `board.capability('neopixel').pins`. Never infer an
  output pin, color order, or maximum length from a board/chip name. Confirm the
  physical header label before connecting data.
- Use a logic analyzer when possible. Sample at least 20 MS/s and record the
  measured bit period, high times for logical zero and one, reset/latch low
  time, frame length, and component-byte order.
- Record the firmware commit SHA, board ID, `board.apiVersion`, complete
  NeoPixel capability, strip model/length/order, power supply rating and measured
  voltage, level shifter, selected pin, analyzer settings, and wiring photograph.

`board.devices.neopixel` describes only a physically onboard device. Its absence
does not prevent use of an external strip when the module and capability exist.

## Capability-driven setup

Run this in the persistent runtime realm. Replace `testPin` only with another
entry from the advertised pin list; do not add a board-name branch.

```js
(function () {
  var boardApi = require('board');
  var modules = require('mcujs:module');

  if (!modules.has('neopixel')) {
    console.log('External NeoPixel driver is unavailable');
    return;
  }

  var limits = boardApi.capability('neopixel');
  var pixels = require('neopixel');
  var testPin = limits.pins[0];
  var testLength = 1;

  pixels.init({pin: testPin, length: testLength, order: 'RGB'});
  pixels.clear();
  console.log('NeoPixel test ready on pin', testPin);
}());
```

Require `pins` to be non-empty, `orders` to contain the requested exact string,
and `maxLength` to be a positive integer. Current targets advertise both `RGB`
and `GRB` with a maximum length of 256, but the running capability remains the
authority.

## Known-color and manual photograph sequence

Darken or shield the fixture enough that each lit component is visually clear.
Use one pixel initially and run each step separately:

```js
var pixels = require('neopixel');
pixels.setPixel(0, 255, 0, 0); pixels.show(); // red
pixels.setPixel(0, 0, 255, 0); pixels.show(); // green
pixels.setPixel(0, 0, 0, 255); pixels.show(); // blue
pixels.setPixel(0, 32, 32, 32); pixels.show(); // dim white
pixels.clear();                                // all off
```

For every step:

1. Require the intended physical color and no change on adjacent pixels.
2. Capture one photograph that includes the board, first pixel, wiring, and a
   written or displayed step label. Do not rely on filenames alone to identify
   colors.
3. When using an analyzer, decode the exact transmitted component bytes and
   verify that `RGB` sends red/green/blue component order while `GRB` sends
   green/red/blue component order. A visually correct library call alone does
   not prove byte order because the backend driver may reorder components.
4. After `clear()`, require every pixel dark and an all-zero frame or equivalent
   driver clear transaction.

Repeat the sequence after `init()` with the other advertised order. The public
`setPixel(index, red, green, blue)` argument order does not change; only the wire
component order does.

## Strict validation and exact boundaries

With an analyzer attached, verify that every rejected call produces no data
frame or pin-remux side effect:

- missing/non-object options and unknown option keys;
- missing, string, non-finite, fractional, negative, or unadvertised pins;
- missing, string, non-finite, fractional, zero, and oversized lengths;
- any order not exactly listed by the capability, including lowercase aliases;
- string, non-finite, fractional, negative, or out-of-range indexes;
- missing, string, non-finite, fractional, negative, or greater-than-255 RGB
  components.

Initialize a one-pixel strip and require `setPixel(0, ...)` to succeed while
`setPixel(1, ...)` throws an uncoded `RangeError`. Programming errors remain
uncoded `TypeError`/`RangeError`; they must not be replaced by initialization or
resource-state errors.

For the length boundary, use a fixture that is electrically safe for the full
advertised load or leave the output disconnected and state that the row proves
only software lifecycle/timing health:

```js
(function () {
  var boardApi = require('board');
  var limits = boardApi.capability('neopixel');
  var pixels = require('neopixel');
  var pin = limits.pins[0];

  pixels.init({pin: pin, length: limits.maxLength, order: limits.orders[0]});
  pixels.setPixel(limits.maxLength - 1, 1, 2, 3);
  pixels.show();

  try {
    pixels.init({pin: pin, length: limits.maxLength + 1,
                 order: limits.orders[0]});
    throw new Error('maximum plus one was accepted');
  } catch (error) {
    if (!(error instanceof RangeError) || ('code' in error)) throw error;
  }

  pixels.show(); // the maximum-length configuration must still be usable
}());
```

Require exactly `maxLength` pixels' worth of data in the accepted frame and no
frame for `maxLength + 1`. The rejected reinitialization must leave the working
configuration intact because full argument validation occurs before teardown.

## Lifecycle, ownership, watchdog, and USB health

1. Before initialization, `setPixel()`, `show()`, and `clear()` must throw
   `ResourceBusyError` with `code === 'EBUSY'`.
2. Initialize an advertised pin, set a known color, show it, clear it, and show a
   second color. Repeat this sequence at least 100 times. Require no stale frame,
   allocation leak, watchdog reset, or loss of the REPL prompt.
3. Successfully reinitialize on a second advertised pin. Require the first pin
   inactive and explicitly reusable only after `gpio.init()`; stale GPIO state
   must not be restored automatically.
4. While another peripheral owns a candidate pin, `neopixel.init()` must throw
   `EBUSY` without disrupting the current strip. Once that peripheral releases
   the pin, retry initialization explicitly.
5. Exercise `maxLength` for at least 60 seconds. During the run, evaluate a
   simple expression through CDC at least once per second and require every
   response with no reconnect. Also list/read one already-existing user file
   when storage is device-owned; do not create or rename user data for this
   protocol.
6. Keep the board idle for longer than its task-watchdog interval, then issue
   another `setPixel()`/`show()` and a REPL expression. Require no watchdog log,
   spontaneous reset, USB disconnect, lost prompt, or MSC ownership failure.
7. On composite CDC+MSC firmware, keep the host volume unmounted or properly
   ejected while device-side filesystem checks run. Confirm that CDC remains
   responsive during maximum-length `show()` calls and that MSC still reports
   its stable nonzero capacity afterward.

A disconnected-strip smoke test can prove registration, validation, allocation,
pin ownership, maximum synchronous transmission time, watchdog health, and USB
responsiveness. It cannot prove color, wire order, voltage levels, timing, power
integrity, or signal integrity; record those rows as untested rather than passed.

## Operational failures and evidence

Host-native SDK-stub tests inject allocation, pin-contention, set, refresh,
clear, and teardown failures. On hardware, do not manufacture shorts or corrupt
live driver state. Require observable allocation exhaustion to throw
`ResourceExhaustedError`/`ERR_RESOURCE_EXHAUSTED`, contention to throw
`ResourceBusyError`/`EBUSY`, unsupported native configurations to throw
`NotSupportedError`/`ERR_NOT_SUPPORTED`, and generic driver failures to throw
`EIO`. Operational errors must identify `resource === 'neopixel'` and include
applicable pin, limit, and native diagnostics.

Retain submitted JavaScript, exact returned values/errors, serial logs, reboot
counter or uptime evidence, CDC response timestamps, MSC-capacity observations,
analyzer captures/decodes, power measurements, and labeled photographs. List
every skipped row with its precise fixture, equipment, electrical-safety, or
capability reason. Native tests and manual hardware evidence prove different
layers and must not be reported as substitutes for one another.
