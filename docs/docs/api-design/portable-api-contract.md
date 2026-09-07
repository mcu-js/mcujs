---
sidebar_position: 1
---

# Portable API contract for MCU.js 0.2

Status: frozen implementation contract for the 0.2 development series. Firmware work lands in later 0.2 cards; released firmware remains authoritative for what is currently implemented.

The [machine-readable Draft 2020-12 schema](/schemas/mcujs-portable-api-0.2.schema.json) freezes the 0.2 public module names, signatures, JavaScript types, units, capability fields, optionality rules, errors, aliases, and pre-1.0 evolution policy. It also validates the board-qualified capability manifest that release tooling will publish beside each firmware image.

## The three availability states

These states are deliberately different:

| State | API shape | Runtime behavior |
|---|---|---|
| Unsupported | The module, method, or property is absent | Detect it with `builtinModules`, `modules.has()`, or property existence. `require()` uses the normal module-not-found path. |
| Supported API, unsupported configuration | The API remains present | Throw `NotSupportedError` with `code === 'ERR_NOT_SUPPORTED'`. Examples include an otherwise-valid SPI option combination or an in-range frequency that cannot be represented exactly. |
| Supported API, temporarily unavailable or failed | The API remains present | Throw an operational error such as `EBUSY`, `ERR_RESOURCE_EXHAUSTED`, `ENXIO`, or `EIO`. |

A supported operation never disappears because a pin becomes busy or an external device disconnects. An unsupported operation is never represented by a throwing stub or a success-returning no-op.

## Discovery without board-name branches

Use lower-case CommonJS modules. `require('board')` is canonical; the `board` global is a compatibility alias through 0.x.

This example is wrapped so it can be run repeatedly in MCU.js's persistent REPL realm:

```js
(function () {
  var boardApi = require('board');
  var modules = require('mcujs:module');

  if (!modules.has('spi')) {
    console.log('SPI is not compiled into this firmware');
    return;
  }

  var caps = boardApi.capability('spi');
  var spi = require('spi');
  spi.init({
    bus: caps.defaultBus,
    frequency: Math.min(1000000, caps.frequency.maxHz),
    mode: caps.modes[0]
  });
}());
```

`builtinModules`, `modules.has(name)`, native registration, REPL help, and `board.capability(name)` must eventually derive from one registry. Do not use `board.name` or `board.chip` as a compatibility switch. Identity remains useful for diagnostics and logs.

## Canonical discovery surface

The schema defines these always-present board fields and methods:

- identity and limits: `name`, `chip`, `version`, `apiVersion`, `flashSize`, `ramSize`, and `cpuFreq`;
- immutable maps: `pins` and `devices`;
- discovery: `capability(name)` and `capabilities()`;
- shared board services: `freeMemory()`, `uniqueId()`, `reset()`, `millis()`, `delay(ms)`, and `enterUf2()`.

`board.capability(name)` returns one descriptor or `undefined`, which avoids constructing the complete object on a small JerryScript heap. `board.capabilities()` returns the complete immutable snapshot for diagnostics and tooling.

The portable hardware modules frozen by this contract are `gpio`, `pwm`, `adc`, `i2c`, `spi`, and `neopixel`. A firmware only registers the modules it implements. The full method signatures, required arguments, return types, and compatibility overloads are in `x-mcujs-contract.modules` in the schema.

## Static capability, dynamic status

A static capability describes immutable facts about this board and firmware image:

- safe exposed pins and valid routes;
- buses, modes, word sizes, and bit orders;
- transfer, output, strip-length, timer, and frequency limits;
- ADC resolution, calibrated-voltage support, and temperature support;
- current-image decoder methods, formats, destination format, and input ceiling;
- enabled filesystem and USB classes.

The release manifest's `board.exposedPins` array is the authoritative set of safe, runtime-exposed MCU GPIO identifiers. It includes intentional onboard GPIO endpoints and excludes reserved implementation pins. Every MCU pin published through `board.pins`, `board.devices`, or a GPIO, PWM, ADC, I2C, SPI, or NeoPixel descriptor must resolve into that set. `board.pins` remains the smaller runtime map of semantic aliases; it is not treated as a complete pin inventory.

Current ownership and health do not belong in a capability descriptor. Storage ownership, occupied channels, attached external devices, and current bus responses are dynamic. For example:

```js
(function () {
  var boardApi = require('board');
  if (boardApi.capability('fs') && !boardApi.storageReady()) {
    console.log('Storage exists, but is not currently device-owned');
  }
}());
```

The `fs` capability is static. `storageReady()` is dynamic and only exists when `fs` exists. A host-owned filesystem keeps its API surface and returns `EBUSY` from operations that require device ownership.

## Capability descriptors

The JSON schema closes every 0.2 descriptor with `additionalProperties: false`; changing a field name or unit is an API change.

| Capability | Required public fields |
|---|---|
| `gpio` | `pins`, `outputPins`, `modes` |
| `pwm` | `pins`, `maxOutputs`, `timerCount`, `duty`, `frequency` |
| `adc` | `resolutionBits`, non-empty `pins` and `channels`, `voltage`, `temperature`, `vsys` |
| `boot` | `safeMode` (capability omitted when persistent safe mode is unavailable) |
| `i2c` | `buses`, `routes`, `defaultBus`, `defaultRoute`, `frequency`, `maxTransferBytes` |
| `spi` | `buses`, `routes`, `defaultBus`, `defaultRoute`, `frequency`, `maxTransferBytes`, `modes`, fixed `bitsPerWord: [8]`, fixed `bitOrders: ['msb']`, `fullDuplex`, `dma`; optional `compatibilityExtensions` |
| `neopixel` | `pins`, `maxLength`, `orders` |
| `image` | `methods`, `formats`, `maxInputBytes`, `destination` (current-image compatibility descriptor; not a normalized portable API) |
| `fs` | `implementation`, `writable`, `hostTransfer` |
| `usb` | `classes` |

Irrelevant optional subfields are omitted rather than populated with `-1`, `null`, or another magic value. For example, unsupported calibrated voltage is `{supported: false}` instead of invented voltage limits.

JSON Schema enforces closed shapes, required fields, types, uniqueness, and non-empty collections where those rules are expressible. An advertised ADC capability therefore always has at least one raw pin and channel operation; firmware with no usable raw route omits the capability and module. Cross-field relations require the mandatory semantic validator at `scripts/validate-portable-api-manifest.js`; release tooling and manifest producers must use it rather than AJV alone. Run it with `npm run validate:manifest -- path/to/manifest.json`. It rejects contradictions such as pins or aliases outside `board.exposedPins`, GPIO output pins without output mode, zero-width or reversed ADC voltage ranges, duplicate bus-role pins, invalid default buses or default routes, operation defaults absent from a capability, SPI formats other than the executable 8-bit MSB-first contract, `writeBufferDMA` without DMA, and onboard NeoPixel metadata outside the driver capability. The machine-readable constraint IDs live under `x-mcujs-contract.manifestValidation`.

The `image` descriptor is deliberately narrower than a portable image API. It
reports only what the current firmware compiles and registers: the exact module
methods, baseline JPEG and supported BMP decoder variants, RGB565 destination
byte orders, and the shared input-buffer ceiling. It does not imply a graphics
buffer, framebuffer, screen, display, DVI output, or onboard device. Firmware
without the compiled decoder omits both the module and descriptor. Argument,
error, buffer-handle, and display API normalization remain deferred to later
0.x work.

Operation arguments and results carry executable limit annotations in `x-mcujs-contract.modules` and `types`: `defaultFromCapability`, `defaultRouteFromCapability`, `allowedFromCapability`, `conditionalAllowedFromCapability`, `minimumFromCapability`, `maximumFromCapability`, `maximumFromCapabilityBitWidth`, `maximumFromArgumentResource`, and `routeFromCapability`. Each bus descriptor contains one complete `defaultRoute` that is also present in `routes` and uses `defaultBus`; omitted route fields resolve through that object, so multiple routes on the default bus are not ambiguous. Raw ADC results are bounded by `0..(2^resolutionBits - 1)`, while voltage results use `voltage.minVolts..voltage.maxVolts`. Optional ADC exports use the `capabilityField` discriminator, so voltage and temperature methods are absent unless their exact static support field is true. Onboard shortcut list lengths use `maximumFromOnboardDevice`; NeoPixel indexes use `exclusiveMaximumFromConfiguration` because their upper bound is the strip length selected by a validated `init()` call. The RP DMA compatibility helper uses `maximumFromArgumentResource` to bind `byteLength` to the selected graphics handle's live `byteLength`. Conformance tooling uses these references to derive max/max+1, route, mode, enum, and numeric-result boundary tests instead of parsing prose.

## Onboard inventory is not driver capability

`board.devices` reports physically onboard devices. Module capability reports what the runtime can drive through exposed pins. They are separate axes.

```js
(function () {
  var boardApi = require('board');
  var modules = require('mcujs:module');

  if (boardApi.devices.led) {
    var led = boardApi.devices.led;
    var gpioCapability = boardApi.capability('gpio');
    if (led.type === 'gpio' && gpioCapability &&
        gpioCapability.pins.indexOf(led.pin) !== -1) {
      var gpio = require('gpio');
      gpio.init(led.pin, gpio.OUTPUT);
      gpio.set(led.pin, !led.activeLow);
    } else {
      // Managed and shortcut-only LEDs use the transport-neutral board API.
      boardApi.led(true);
    }
  }

  if (!boardApi.devices.neopixel && modules.has('neopixel')) {
    console.log('External NeoPixel strips are supported; none is onboard');
  }
}());
```

`board.led()`, `board.buttonPressed()`, and `board.neopixel()` are onboard-device-gated: they are absent when the corresponding key in `board.devices` is absent. No board gets a fake no-op shortcut.

An LED descriptor is either `{type: 'gpio', pin, activeLow}` or the transport-neutral `{type: 'managed'}`. A managed LED is still physically onboard and still enables `board.led()`, but it must not publish a wireless-controller or expander-local identifier as an MCU GPIO pin.

The LED shortcut preserves both existing signatures: `board.led()` returns the current logical boolean state, while `board.led(on)` requires a boolean and returns `undefined`.

### Onboard button (no wiring)

`board.buttonPressed(): boolean` takes **no arguments** and returns `true` while
the button is pressed, `false` when released. This is a read-only instantaneous
sample, not a debounced event or a setter. Any argument (including `undefined`)
throws `TypeError` before sampling. A native input-configuration failure throws
an operational error with `code: 'EIO'`; the method stays present.

Only **Raspberry Pi Pico (RP2040)** and **Seeed XIAO ESP32-S3** currently support
this method. Pico uses the **BOOTSEL** button; XIAO uses **BOOT**, not RESET.
Pico 2 and other boards omit both `buttonPressed` and `devices.button`.
The immutable descriptor is `{type: 'managed', name: 'BOOTSEL', activeLow: true,
readOnly: true}` on Pico, with `name: 'BOOT'` on XIAO. Neither descriptor has a
`pin`: Pico's flash chip-select and XIAO's GPIO0 are reserved implementation
endpoints, not generic GPIO/PWM/ADC/I2C/SPI/NeoPixel pins or aliases. XIAO samples
GPIO0 as input with a pull-up; its boot/recovery strapping policy is unchanged.

**Do not hold BOOT/BOOTSEL during reset or power-on** when using this demo:
that can enter the bootloader/recovery path rather than running the application.
Press only after firmware has started. Poll with a timer, not a busy loop.
The shared `examples/onboard-button/index.js` runs manually for 60 seconds,
debounces both edges with three matching 10ms samples, logs state changes, then
clears its interval, switches the LED off, and prints `Demo complete!`.
It uses `board.led(board.buttonPressed())` for the initial state and the same
portable board methods for debounced updates; there is no board-name branching.

Pico sampling reuses the native boot sampler, following the SRAM-only,
interrupt-masked settling sequence in Raspberry Pi's
[official button example](https://github.com/raspberrypi/pico-examples/blob/master/picoboard/button/button.c),
and restores the original QSPI control register before interrupts. Runtime
exposure is restricted to Pico: core 1 stays idle, DVI is disabled, SPI DMA
completes synchronously, and no XIP streamer is used. Any future asynchronous
flash access or core-1 use must revisit this safety constraint before enabling
button sampling.

The compatibility-only NeoPixel shortcut preserves its four input shapes: one color as `[r, g, b]` or `{r, g, b}`, and multiple colors as `[[r, g, b], ...]` or `[{r, g, b}, ...]`. Missing color components default to zero. Object fields are logical RGB; color-array positions follow the active NeoPixel order. A color array longer than three bytes or a multi-pixel list longer than `board.devices.neopixel.length` throws `RangeError`; it is never truncated.

## Types and units

The shared contract rejects missing arguments, wrong types, non-finite numbers, fractional integers, and values outside documented ranges before any native cast.

Portable units are fixed:

- PWM duty: ratio `0..1`;
- frequency: hertz (`Hz`);
- duration: milliseconds (`ms`);
- ADC voltage: volts (`V`);
- transfer sizes and memory: bytes;
- temperature: degrees Celsius (`°C`).

Arrays and buffers above a capability limit throw. Values are never silently truncated, clamped, wrapped, or coerced.

## Stable errors

See the [error reference](./errors.md) for executable handling examples and
the diagnostic-property contract.

Programming mistakes use JavaScript built-ins:

- `TypeError`: missing arguments, wrong types, and non-finite numbers;
- `RangeError`: fractional integer inputs, out-of-range values, and oversized values.

`RangeError` applies whenever a value violates an explicit type, range, length, route, enum, or capability-derived argument constraint. A requested SPI mode absent from advertised `spi.modes` therefore throws `RangeError`. `ERR_NOT_SUPPORTED` begins only after every supplied value passes those constraints but the backend cannot implement the requested valid combination or exact hardware representation. For example, a frequency outside advertised `minHz..maxHz` is a `RangeError`; an in-range frequency that cannot be represented exactly is `ERR_NOT_SUPPORTED`.

Operational failures use a stable `.code`:

| Code | Error name | Meaning |
|---|---|---|
| `ERR_NOT_SUPPORTED` | `NotSupportedError` | The API exists, but this configuration cannot be implemented. |
| `EBUSY` | `ResourceBusyError` | A resource is owned or temporarily unavailable. |
| `ERR_RESOURCE_EXHAUSTED` | `ResourceExhaustedError` | A finite channel, timer, or similar pool is exhausted. |
| `ENXIO` | `Error` | An external device absence or NACK is distinguishable. |
| `EIO` | `Error` | A hardware or native-driver I/O operation failed. |

Portable control flow should branch on `.code`. Optional fields such as `resource`, `pin`, `owner`, `bus`, `limit`, and `nativeCode` are diagnostic only.

Every GPIO, PWM, ADC, I2C, SPI, and NeoPixel signature carries `operationalErrors` plus machine-readable `operationalErrorConditions`. The condition taxonomy fixes unsupported configuration, resource ownership, finite-pool exhaustion, external-device absence, native I/O failure, and `uninitializedUse` to their stable codes. Initialization-dependent operations use `EBUSY` for `uninitializedUse`; they never silently auto-initialize.

## Versioning and aliases

`board.apiVersion` and release manifests use `major.minor`, here `0.2`. Before 1.0:

- a breaking contract change requires a new minor API version and migration notes;
- patch releases are non-breaking with respect to the frozen schema;
- aliases listed by the schema remain available through 0.x when their target capability exists;
- those aliases may be removed no earlier than 1.0.

Compatibility globals (`board`, `GPIO`, `PWM`, `adc`, `I2C`, `SPI`, and `neopixel`) are not canonical portable names. `require('node:module')` remains an alias of `require('mcujs:module')`. The positional `i2c.init(...)` and `spi.init(...)` forms remain compatibility overloads; new code uses options objects and board-declared defaults. The schema records every alias's kind, JavaScript type, capability/onboard gate, and removal policy. In particular, `board.ledPin` exists only for a GPIO-backed onboard LED, and deprecated RP compatibility constants `adc.TEMP` and `adc.VSYS` are gated by `adc.temperature.rawChannel === true` and `adc.vsys === true`. They identify internal raw channels 4 and 3; they are not portable ADC exports or external channel aliases. See [Migrating from 0.1 to 0.2](../migration/0.2.md).

Two live 0.1 surfaces are retained as explicitly nonportable compatibility extensions rather than silently disappearing from the inventory. `spi.writeBufferDMA(bus, bufferHandle, byteLength)` exists only when `spi.compatibilityExtensions` contains `writeBufferDMA` and `spi.dma` is true; its opaque graphics handle prevents it from being portable SPI. A stale or unknown handle throws `RangeError`. `byteLength` may be zero and may equal the handle's byte length; maximum+1 throws `RangeError` rather than clamping. Use before successful `spi.init()` throws `EBUSY`; if no DMA channel is available, it throws `ERR_RESOURCE_EXHAUSTED`.

`board.safeMode()` and `board.safeMode(enabled)` exist only when `boot.safeMode` is true. The getter reports dynamic state. The setter persists it, but `board.safeMode(false)` during the active boot qualification window throws `EBUSY`; persistence failure throws `EIO`. Portable programs must feature-detect these methods and must not infer them from a board or chip name.
