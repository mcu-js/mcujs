# MCU.js 0.2 portable API and capability-discovery design

Status: implementation in progress; release qualification pending
Target: MCU.js 0.2.0
Goal: make ordinary JavaScript portable across shipping boards without pretending unavailable hardware exists.

## 0.2.0 release checklist

This is the current release checklist, not a claim that every design proposal
below is implemented. Reconciled against development source `d993b27` and the
hardware run of firmware `0.1.0+5fc1294` on 2026-09-07.

**Working policy:** push ongoing work to GitHub's `development` branch. Source
checks and docs builds run there; Pages deployment is skipped. Keep `main`,
release tags, and published firmware unchanged until release approval. The
runtime version remains `0.1.0` until the release-candidate versioning step.

### Implemented in development

- **Implemented:** canonical `require('board')`, `board.apiVersion`, pin/device
  metadata, capability queries, `modules.has()`, the shared registry, and
  generated board manifests.
- **Implemented:** strict validation and portable GPIO, PWM, ADC, I2C, SPI, and
  external NeoPixel contracts. Native tests cover simulated backend behavior;
  these are not electrical measurements.
- **Implemented:** runtime filesystem ownership handoff and current-image USB
  and image-decoder capability metadata. Portable graphics/display API
  normalization is still a later-0.x non-goal.
- **Implemented:** registry-backed `.capabilities` and `.capabilities NAME`,
  migration documentation, and portable examples. Their final consistency
  audit remains open below; they do not need to be rebuilt from scratch.
- **Implemented:** read-only onboard-button input and a bounded, debounced
  button-to-LED example for Pico and XIAO ESP32-S3.
- **Native REPL verified:** `.help` advertises `buttonPressed()` only on Pico
  and XIAO, using the same generated flag as native binding registration.
  `scripts/test-repl.sh` exercises actual REPL input/output for all ten shipping
  board configurations with their board-specific headers. This is host-native
  coverage, not a new firmware build or an on-device test of the updated help.
- **CI verified:** [source checks at d993b27](https://github.com/mcu-js/mcujs/actions/runs/34173032693)
  and [docs typecheck/build at d993b27](https://github.com/mcu-js/mcujs/actions/runs/34173032719)
  passed. Pages artifact upload and deployment were skipped. These checks do
  not build or physically test every shipping firmware image.

### Hardware evidence recorded

The same `examples/onboard-button/index.js` ran manually on both boards for
60 seconds using firmware built from `5fc1294ad378e92356a7cbe16b182086309eef91`.
The native validation lane and both target builds exited successfully. Both
UF2 payloads were checked against their application binaries.

- **Pico / RP2040:** serial output recorded two presses followed by releases.
  The final button and LED readings were false, storage was device-ready, and
  the temporary demo file was removed, leaving an empty filesystem.
  UF2 SHA-256: `b7872682fa32b74c337a4656ca4e673efe298d26f10cd5c27525c2ecd16a7e4f`.
- **XIAO ESP32-S3:** serial output recorded four presses followed by releases.
  The final button and LED readings were false, storage was device-ready, and
  the temporary demo file was removed, leaving an empty filesystem.
  UF2 SHA-256: `9d379f8a39e8a362e465b69368b6df280fb3f7575b700e54cc4ff06f4e4dfa1f`.
- **Both:** runtime build identity matched; `buttonPressed()` returned a
  boolean and rejected supplied arguments with `TypeError`.

This is hardware-executed button/demo evidence, not an all-peripheral pass,
measured PWM waveform evidence, or qualification of a future release candidate.
The firmware hashes above identify local test artifacts, not published assets.

### Still needed, in order

1. **Open — finish discovery/help consistency.** The button-help omission is
   fixed with positive and negative native coverage across all shipping board
   configurations; on-device verification awaits updated firmware. Check registration,
   `builtinModules`, `modules.has()`, optional exports, capability metadata,
   generated manifests, and help against each shipping image, including the
   independent graphics/screen/DVI surfaces. Close only with matching tests and
   target observations; source CI alone is not a full hardware discovery audit.
2. **Open — finish the release-facing docs and example audit.** The migration
   guide and examples exist, but `CHANGELOG.md` has no 0.2.0 entry yet. Reconcile
   the final API changes, supported targets, breaking behavior, and limitations.
   Re-run applicable examples on both backend families, including repeated
   execution and cleanup. The existing PWM fade uses the onboard LED only if
   its pin is advertised for PWM: Pico qualifies, but XIAO GPIO21 is currently
   excluded from its PWM capability. Investigate that restriction before
   promising a no-wiring two-board fade; do not silently widen the pin policy.
3. **Open — prepare one immutable release candidate.** Finish the 0.2.0 version
   and release notes, obtain the required independent review, and build all nine RP
   targets plus XIAO ESP32-S3 from that exact commit. Verify packaging,
   checksums, generated manifests, recovery/update layout, and applicable
   host/container reproducibility. Older builds remain historical evidence,
   not qualification of a different candidate.
4. **Open — qualify that candidate on shipping hardware.** Record per-board
   discovery, recovery, storage ownership/persistence, and supported peripheral
   results. Use the existing [ADC](./adc-voltage-protocol.md),
   [PWM](./pwm-waveform-protocol.md), [I2C](./i2c-peripheral-protocol.md),
   [SPI](./spi-loopback-protocol.md), and
   [NeoPixel](./neopixel-hardware-protocol.md) protocols. Missing hardware,
   instruments, and unperformed checks stay explicit; any release waiver
   requires maintainer approval. Today's Pico/XIAO demo is not a substitute for
   the remaining targets or electrical tests.
5. **Open — approve and publish 0.2.0.** Complete exact-candidate verification,
   then review the `development` to `main` PR
   before merging. Only after approval and merge: tag, publish firmware and
   documentation, and smoke-test downloads from the public release. Routine
   development pushes do not authorize any of these release actions.

**Next bounded change:** reconcile the existing 0.2 migration guide into an
unreleased changelog entry, without changing the firmware version or publishing
a release. Keep the remaining exact-image and hardware gates above open.

## Design principles

1. **One portable contract, multiple honest capability sets.**
   Shared APIs have identical names, argument types, units, return values, and error behavior on every board that exposes them.
2. **Feature-detect; do not board-sniff.**
   Code should ask whether a module/capability exists, not compare `board.name` or `board.chip`.
3. **Unsupported APIs are absent.**
   A module or optional operation that the firmware cannot implement must not be registered as a stub.
4. **Supported APIs throw for operational failure.**
   Invalid arguments, resource conflicts, temporary unavailability, missing external devices, and native driver errors are failures of a real API and should throw typed errors.
5. **Static capabilities and dynamic state are separate.**
   Limits such as transfer size and valid routes are immutable capability metadata. State such as filesystem ownership or an occupied PWM timer is queried from the module or represented by errors.
6. **External-driver capability is not onboard-device presence.**
   A board can support the `neopixel` module without having an onboard NeoPixel.
7. **Numeric limits are part of the API.**
   Embedded code needs discoverable transfer, channel, frequency, allocation, and buffer limits; method-existence checks alone are insufficient.
8. **No silent truncation, wrapping, coercion, or clamping.**
   Portable code must get the requested operation or a clear exception.
9. **One descriptor source drives registration, introspection, help, docs tests, and conformance tests.**
10. **Pre-1.0 minor versions may break deliberately, but each shipped minor has a documented contract.**

## Browser analogy

Use the web-platform split:

- An unimplemented optional browser API is absent and detected by property existence.
- An implemented API remains present when permission, runtime state, or a particular external device prevents an operation; the operation then rejects/throws a meaningful error.

MCU.js equivalent:

| Situation | Runtime behavior |
|---|---|
| Firmware has no SPI implementation | `spi` absent from `builtinModules`; `require('spi')` throws module-not-found |
| SPI exists but DMA is unsupported | `spi.writeBufferDMA` absent; capability says `dma: false` |
| SPI exists but requested mode is absent from advertised `spi.modes` | `spi.init` throws `RangeError` |
| SPI exists and frequency is in range but not exactly representable | `spi.init` throws `NotSupportedError` / `ERR_NOT_SUPPORTED` |
| SPI route is already owned | operation throws `ResourceBusyError` / `EBUSY` |
| I2C controller exists but external device NACKs | read/write throws `EIO` or `ENXIO` |
| Filesystem exists but MSC host owns it | filesystem calls throw `EBUSY` |
| Board has no onboard NeoPixel | `neopixel` module may still exist; `board.devices.neopixel` is absent |

Do not expose success-returning no-op methods for absent hardware.

## Public discovery API

### Canonical board module

Add `require('board')` as the canonical API. Preserve the existing `board` global as a compatibility alias during the 0.x series.

```js
const board = require('board');

board.name;                 // 'seeed_xiao_esp32s3'
board.chip;                 // 'ESP32-S3'
board.version;              // firmware version
board.apiVersion;           // '0.2'
board.pins;                 // semantic aliases for this board
board.devices;              // physically onboard devices only
board.capability('spi');    // descriptor or undefined
board.capabilities();       // complete snapshot for tooling/REPL
```

`board.capability(name)` should build only one descriptor, avoiding the heap cost of eagerly materializing the complete map. `board.capabilities()` is for diagnostics/tooling and may allocate a larger snapshot.

### Module detection

Keep `require('mcujs:module').builtinModules` authoritative and add a convenience predicate:

```js
const modules = require('mcujs:module');

modules.has('spi'); // true/false

if (modules.has('spi')) {
  const spi = require('spi');
}
```

`builtinModules`, `modules.has()`, native registration, `.help`, and `board.capability(name)` must all derive from the same registry.

Static registry projections are immutable. `builtinModules`, `board.apiVersion`,
`board.exposedPins`, `board.pins`, and `board.devices` are non-writable and
non-configurable; arrays and nested maps are frozen so mutation cannot make
discovery disagree with native lookup. `modules.has(name)` throws `TypeError`
for missing/non-string input and `RangeError` for invalid string lengths.

Do not add `require.optional()`: it is nonstandard, conceals spelling mistakes, and duplicates explicit feature detection.

### Human and host-tool discovery

Programmatic metadata should also improve the immediate REPL and editor experience:

- Add `.capabilities` for a compact board/module summary.
- Add `.capabilities spi` (or `.help spi`) for routes, limits, modes, and examples.
- Keep `.help` concise and board-accurate; do not dump the complete descriptor on every help call.
- Publish a board-qualified JSON capability manifest beside each release artifact, generated from the same registry.
- Generate/check JavaScript documentation and optional `.d.ts` declarations from the stable API schema; runtime capability checks still decide what this particular board exposes.

The release manifest lets installers, documentation sites, and IDE tooling explain a firmware image before a user flashes it. Runtime introspection remains authoritative for the firmware actually running.

### Capability descriptor shape

Descriptors contain only stable firmware/board facts. Omit irrelevant fields rather than filling them with magic values.

```js
board.capability('spi');
// {
//   buses: [0, 1],
//   routes: [
//     { bus: 0, sck: 7, mosi: 9, miso: 8 }
//   ],
//   defaultBus: 0,
//   defaultRoute: { bus: 0, sck: 7, mosi: 9, miso: 8 },
//   maxTransferBytes: 64,
//   modes: [0],
//   bitsPerWord: [8],
//   bitOrders: ['msb'],
//   fullDuplex: true,
//   dma: false
// }
```

```js
board.capability('adc');
// {
//   resolutionBits: 12,
//   pins: [1,2,3,4,5,6,7,8,9],
//   channels: [
//     { channel: 0, pin: 1, aliases: ['A0'] },
//     ...
//   ],
//   voltage: {
//     supported: true,
//     calibrated: true,
//     minVolts: 0,
//     maxVolts: 3.3
//   },
//   temperature: {
//     supported: true,
//     rawChannel: false
//   },
//   vsys: false
// }
```

```js
board.capability('pwm');
// {
//   pins: [...],
//   maxOutputs: 8,
//   timerCount: 4,
//   duty: { min: 0, max: 1, unit: 'ratio' },
//   frequency: { minHz: ..., maxHz: ..., resolutionVaries: true }
// }
```

Capability field names and units are public API and require conformance tests.

### Pin aliases

Expose semantic aliases so portable programs do not embed board GPIO numbers:

```js
board.pins = {
  D0: 1,
  D1: 2,
  A0: 1,
  SDA: 5,
  SCL: 6,
  SCK: 7,
  MISO: 8,
  MOSI: 9,
  LED: 21
};
```

`board.exposedPins` is the authoritative set of safe MCU GPIO identifiers exposed through public metadata and peripheral APIs. It includes intentional onboard GPIO endpoints and excludes reserved implementation pins. Every MCU pin published through `board.pins`, `board.devices`, or a capability descriptor must appear there; `board.pins` remains the smaller semantic alias map.

Only expose aliases that physically exist. Do not expose internal/reserved GPIO in `board.pins` or GPIO capability lists.

Aliases improve source portability, while capability route maps support boards with multiple valid routes.

### Onboard devices

Keep onboard inventory separate from external peripheral drivers:

```js
board.devices;
// {
//   led: { type: 'gpio', pin: 21, activeLow: true }
//   // or: led: { type: 'managed' } for a non-MCU-controlled onboard LED
//   // neopixel absent on XIAO ESP32-S3
// }
```

On a board with an onboard NeoPixel:

```js
board.devices.neopixel;
// { pin: 16, length: 1, order: 'GRB' }
```

`board.led()` and `board.neopixel()` must be absent when the corresponding onboard device is absent. A no-op method is a discoverability bug.

## Shared API contract rules

### Naming and imports

- Canonical documentation uses lower-case CommonJS modules: `require('gpio')`, `require('pwm')`, etc.
- Existing globals (`GPIO`, `PWM`, `I2C`, and others) remain compatibility aliases through 0.x but are not the portable API.
- Add `require('board')`; preserve global `board` as an alias.
- Module and method names are identical on every board that exposes them.

### Arguments

- Required arguments must be present.
- Numbers must be finite.
- Integer fields must be integral before conversion.
- Booleans must be booleans unless the contract explicitly accepts another type.
- Enum strings are explicit and case-normalized only when documented.
- Arrays and buffers exceeding limits throw; they are never truncated.
- Values outside ranges throw; they are never clamped or wrapped.

### Units

Choose one portable unit per operation:

- PWM duty: ratio `0.0..1.0` only.
- Frequency: Hz.
- Delay/time: milliseconds.
- ADC voltage: volts.
- Transfer sizes: bytes.
- Temperature: degrees Celsius.

If raw hardware units are useful, expose an explicitly named operation such as `setDutyRaw()` only when its semantics can be defined portably; do not overload one numeric range with two meanings.

### Errors

Use JavaScript built-ins for programming errors and stable `.code` values for operational errors.

| Failure | Error |
|---|---|
| Missing/wrong argument type | `TypeError` |
| Value violates an explicit type, range, length, route, enum, or capability-derived argument constraint | `RangeError` |
| Every value passes those explicit constraints, but the requested valid combination or exact hardware representation cannot be implemented | `Error`, `name='NotSupportedError'`, `code='ERR_NOT_SUPPORTED'` |
| Pin/bus/timer/channel is owned or temporarily unavailable | `Error`, `name='ResourceBusyError'`, `code='EBUSY'` |
| Finite hardware pool exhausted | `Error`, `name='ResourceExhaustedError'`, `code='ERR_RESOURCE_EXHAUSTED'` |
| External bus device absent/NACK | `Error`, `code='ENXIO'` when distinguishable, otherwise `EIO` |
| Native driver failure | `Error`, stable mapped code, optional native detail |
| Module unavailable in this firmware | normal CommonJS module-not-found error |

Errors may include diagnostic properties such as `resource`, `pin`, `owner`, `bus`, and `limit`, but portable code should branch primarily on `.code`.

The boundary is deliberate: a frequency below `minHz` or above `maxHz` is a `RangeError`; an in-range frequency that the backend cannot represent exactly is `ERR_NOT_SUPPORTED`. The same rule applies to capability-listed routes, enums, lengths, and correlated option combinations. Each core peripheral signature maps its operational codes to machine-readable conditions in the schema; initialization-dependent methods map use-before-init to `EBUSY` with cause `uninitializedUse`.

## Module-specific 0.2 normalization

### GPIO

- Keep `init`, `set`, `get`, and `toggle` for 0.2.
- Require successful initialization before access on every backend.
- Adopt shared pin ownership on RP as well as ESP. GPIO is a soft owner that a
  fully validated peripheral setup may take over; active peripheral owners stay
  exclusive. Stale GPIO state must check the current owner, and release or
  failed takeover never revives the previous GPIO setup without `init()`.
- Use strict booleans for writes.
- Expose only board-safe pins.
- Register onboard LED shortcuts only when `board.devices.led` exists. There is
  no success-returning no-op path for a board without an LED.
- Consider resource handles later; do not block the capability contract on a native-object redesign.

### PWM

- Keep `init(pin, frequency)`, `setDuty(pin, ratio)`, and `stop(pin)` for 0.2.
- Duty is only `0..1`; remove ambiguous `0..65535` overloading.
- Reject unsupported frequencies rather than silently changing them.
- Model and report timer/channel exhaustion consistently.
- Capability metadata reports output/timer limits and frequency constraints.

### ADC

- Keep the five cross-board methods.
- Advertise the ADC capability only with at least one executable raw pin and channel route; otherwise the capability and module are absent.
- Raw results are `0..(2^resolutionBits - 1)`; calibrated voltage results are `minVolts..maxVolts`. Encode both as machine-readable result constraints, not prose-only limits.
- `readVoltage*()` always returns volts.
- Gate `readVoltage*()` and `readTempC()` with the exact `capabilityField` support discriminator so each method is absent when its sub-capability is false.
- Do not expose `TEMP`/`VSYS` magic constants as portable API.
- Board-specific channel aliases belong in capability metadata and `board.pins`.
- If raw temperature-channel access is retained for RP compatibility, mark it board-specific and deprecated during 0.x.

### I2C

- Keep `init`, `write`, and `read` initially.
- Add a preferred options form that selects the board's declared default route when pins are omitted: `i2c.init({ bus: 0, frequency: 400000 })`.
- Retain the positional form through 0.x for migration, but document the options form for portable code.
- Use strict addresses, bytes, lengths, and baud rates.
- Reject oversize transfers on every backend.
- Expose buses, routes, `defaultBus`, one complete listed `defaultRoute`, baud limits, and max transfer size. A non-default bus requires explicit route pins.
- No attached device is an operational error, not lack of I2C capability.

### SPI

- Keep `init` and `transfer` as the portable core.
- Add a preferred options form with board defaults: `spi.init({ bus: 0, frequency: 1250000, mode: 0 })`; explicit route pins remain available for advanced wiring. The 1.25 MHz example is exactly representable on current RP and ESP targets; an in-range frequency that is not exact throws `ERR_NOT_SUPPORTED`.
- Retain the positional form through 0.x for migration.
- Reject oversize transfers on every backend.
- Expose modes, duplex, routes, one complete listed `defaultRoute`, transfer maximum, DMA availability, and any explicitly capability-gated compatibility extensions. The byte-oriented 0.2 contract implicitly uses exactly 8-bit words and MSB-first order; manifests must advertise only those executable formats until initialization makes format selectable.
- `writeBufferDMA` remains outside the portable core because its graphics handle is an RP display-stack accident. Preserve it only as a nonportable compatibility extension when `dma` is true and `compatibilityExtensions` contains `writeBufferDMA`; otherwise the method is absent. Its `byteLength` maximum comes from the selected handle, an invalid handle or oversize request throws `RangeError`, use before `spi.init()` throws `EBUSY`, and DMA-channel exhaustion throws `ERR_RESOURCE_EXHAUSTED`. Evaluate a portable buffer-transfer API later.

### NeoPixel

- Keep `init`, `setPixel`, `show`, and `clear`.
- Use strict RGB byte values and known order strings.
- Reject unsupported strip lengths and orders.
- Capability metadata reports valid pins, maximum length, and orders.
- Onboard presence remains under `board.devices`, not module availability.

### Board/storage/USB

- Core board identity methods are common.
- Optional onboard shortcuts are omitted when absent.
- Storage support is static capability; `storageReady()` is dynamic state.
- Persistent boot-script safe mode is a static `boot.safeMode` capability. The compatibility-only `board.safeMode()` getter/setter is present only with that capability; its current boolean state remains dynamic. `board.safeMode(false)` during the active boot qualification window throws `EBUSY`; persistence failure throws `EIO`.
- USB descriptors distinguish CDC, MSC, keyboard HID, mouse HID, and other configured classes.
- Silicon potential that is not enabled in the current firmware is not advertised as a runtime capability.

## Dynamic state

Do not mix current state into immutable capability descriptors.

Examples:

```js
board.capability('fs'); // static: filesystem implementation exists
board.storageReady();   // dynamic: device currently owns/mounted storage

pwm.status();            // possible later addition: allocations/timers
```

A supported operation may become temporarily unavailable and throw `EBUSY`; it must not disappear while the program is running.

## Single source of truth

Introduce a shared capability registry with platform/board descriptors. The registry should drive:

1. native module registration;
2. `builtinModules` and `modules.has()`;
3. `.help` output;
4. `board.capability()` and `board.capabilities()`;
5. board pin/device metadata;
6. generated or test-validated documentation;
7. native full/constrained feature tests;
8. hardware conformance probes.

Do not maintain a C feature macro, help list, JavaScript capability object, and documentation table independently.

Build-time feature switches describe the current firmware image, not everything the silicon could theoretically do.

The executable workflow, full/constrained native lanes, schema-derived boundary
case IDs, and `MCUJS_CONFORMANCE_V1` serial evidence envelope are documented in
[Portable API conformance](./portable-api-conformance.md). Later module work
extends that suite instead of creating board-specific contract tests.

## Portable examples

### Optional module

```js
const board = require('board');
const modules = require('mcujs:module');

if (modules.has('spi')) {
  const spi = require('spi');
  const caps = board.capability('spi');
  const payload = data.slice(0, caps.maxTransferBytes);
  // Initialize using a listed/default route, then transfer.
}
```

### Onboard LED with fallback

```js
const board = require('board');
if (board.devices.led) {
  const led = board.devices.led;
  const gpioCapability = board.capability('gpio');
  if (led.type === 'gpio' && gpioCapability &&
      gpioCapability.pins.indexOf(led.pin) !== -1) {
    const gpio = require('gpio');
    gpio.init(led.pin, gpio.OUTPUT);
    gpio.set(led.pin, !led.activeLow);
  } else {
    board.led(true);
  }
} else {
  console.log('This board has no onboard LED');
}
```

### ADC without board sniffing

```js
const board = require('board');
const adc = require('adc');
const caps = board.capability('adc');

const channel = caps.channels.find(entry => entry.aliases.includes('A0'));
if (!channel) throw new Error('Board has no A0 input');
console.log(adc.readVoltageChannel(channel.channel));
```

## Version plan

### 0.2.0 — portable contract foundation

- Add `require('board')`, `board.apiVersion`, `board.pins`, `board.devices`, and capability queries.
- Add `modules.has()`.
- Create the single capability registry.
- Normalize strict argument/range/error semantics across RP and ESP.
- Remove silent truncation/clamping/wrapping.
- Make unsupported modules/methods absent and remove no-op onboard shortcuts.
- Normalize PWM duty to `0..1`.
- Deprecate RP-only ADC magic constants as portable API.
- Add dual-platform conformance tests and update examples/docs.

This is intentionally breaking and warrants 0.2.0.

### 0.3.x — resource-lifecycle API spike

Evaluate handle-based APIs without committing blindly:

```js
const bus = i2c.open({ bus: 0, frequency: 400000 });
bus.read(address, length);
bus.close();
```

Compare code size, Jerry heap use, deterministic cleanup, finalizer safety, and DX against the current flat APIs. Explicit `close()` would be mandatory; GC finalizers can only be a backstop. Adopt only if the spike materially improves correctness and ergonomics.

### Later 0.x

- Portable buffer types and DMA-aware transfers.
- USB HID capability and API normalization.
- Portable image/graphics/display API normalization beyond current-image capability truthfulness.
- Event-driven input APIs where hardware supports them.
- Compatibility removals with explicit migration notes.

### 1.0.0 gate

Do not call the API stable until:

- capability schema is frozen and documented;
- all shipping boards pass the same contract suite;
- every advertised API is backed by hardware tests appropriate to its capability;
- unsupported APIs are consistently absent;
- shared errors/units/types are stable;
- examples contain no board-name branching for ordinary capability differences;
- release/docs tooling verifies the registry, help, module list, and firmware agree.

## Implementation sequence

1. Freeze a machine-readable 0.2 API/capability schema in docs/tests.
2. Add RED native tests for `board`, capability lookup, module presence, and constrained builds.
3. Implement the shared registry and board/module discovery APIs.
4. Add explicit RP board feature maps; stop relying on “full RP defaults” as implicit truth.
5. Normalize GPIO ownership and validation across backends.
6. Normalize PWM duty/frequency/resource behavior.
7. Normalize ADC constants/channel discovery.
8. Normalize I2C/SPI limits and error behavior.
9. Normalize NeoPixel validation and onboard/external separation.
10. Generate/validate `.help`, `builtinModules`, examples, and docs from the registry.
11. Run native conformance, every RP release board, ESP clean/repro builds, and target hardware tests.
12. Finish the 0.2.0 version and migration notes before freezing an independently reviewed candidate. Keep development work pushed to `development`; follow the release checklist above for exact-candidate QA and the separately approved `main` merge, tag, and publication.

## Acceptance tests

For every shipping board:

- `builtinModules` exactly equals registered/require-able modules.
- `.help` exactly matches that list and optional board methods.
- `modules.has(name)` agrees with `builtinModules`.
- `board.capability(name)` exists exactly when the module/capability is advertised.
- Capability limits match boundary behavior: max succeeds, max+1 throws.
- Unsupported methods are absent, not stubs.
- Valid shared calls return the same JavaScript types and units.
- Invalid shared calls throw the same error class/code.
- No backend silently truncates, clamps, wraps, or coerces.
- Pin aliases resolve to board-safe exposed pins.
- Onboard device inventory matches physical hardware.
- Dynamic unavailability preserves API existence and throws the documented operational error.

## Explicit non-goals for 0.2.0

- Pretending every board supports every module.
- Advertising latent silicon features not enabled in the firmware.
- Detecting whether an arbitrary external I2C/SPI/NeoPixel device is connected as static capability.
- Standardizing RP graphics-buffer handles as a generic DMA API.
- Rewriting every peripheral around native JS resource objects before the capability foundation is proven.
