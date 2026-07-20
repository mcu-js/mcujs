# SPI loopback and logic-analyzer acceptance protocol

This protocol is the physical acceptance gate for the portable MCU.js 0.2 SPI
master contract. It is deliberately non-destructive: it exercises an
already-installed, reviewed firmware image and does not alter firmware or user
storage.

## Safety and prerequisites

- **No flashing**, UF2 copying, mounting, formatting, or filesystem writes are
  part of this protocol. Stop if the expected firmware is not already running.
- Use 3.3 V logic and a **common ground**. Connect MOSI directly to MISO only for
  this digital loopback; disconnect external SPI peripherals first so two
  outputs cannot contend. Connect SCK, MOSI, and MISO to a logic analyzer.
- Select one complete route from `board.capability('spi').routes`. Never infer
  pins, transfer limits, modes, or DMA support from a board/chip name.
- Use a logic analyzer at no less than 10 samples per requested SCK period. For
  a 1.25 MHz test, sample at 12.5 MS/s or faster. Record decoded bytes, bit order,
  clock idle level, sampling edge, measured frequency, and transfer length.
- Record the firmware commit SHA, board ID, `board.apiVersion`, complete SPI
  capability, analyzer model/settings, physical route, and wiring photograph.
- The portable module does not control chip select. If a later known-peripheral
  row needs CS, drive only a capability-approved GPIO explicitly and keep it out
  of the three SPI route pins.

## Discovery and default-route initialization

1. Verify `require('mcujs:module').has('spi')` and capture
   `board.capability('spi')`.
2. Require non-empty `buses`, `routes`, and `modes`; a `defaultBus`; one complete
   `defaultRoute` present in `routes`; inclusive frequency bounds; positive
   `maxTransferBytes`; `bitsPerWord: [8]`; `bitOrders: ['msb']`; and booleans for
   `fullDuplex` and `dma`.
3. Wire MOSI to MISO on `defaultRoute`, attach the analyzer, and initialize with
   an exactly representable rate used by current RP and ESP targets:

   ```js
   (function () {
     var boardApi = require('board');
     var spi = require('spi');
     var limits = boardApi.capability('spi');
     spi.init({bus: limits.defaultBus, frequency: 1250000, mode: 0});
   }());
   ```

4. Require SCK on the declared pin at 1.25 MHz within the analyzer's measurement
   tolerance, mode 0 idle-low/rising-edge sampling, MSB-first eight-bit words,
   and no activity on another listed route.
5. Repeat initialization and require clean teardown/reinitialization without a
   watchdog reset, stale transfer, or loss of REPL responsiveness.

If 1.25 MHz or mode 0 is outside a future descriptor, record this row as not
executable and select an in-range, advertised, exactly representable candidate.
Do not silently accept a different measured frequency.

## Exact scalar, array, and boundary transfers

With the default bus initialized and MOSI connected to MISO:

1. Require `spi.transfer(limits.defaultBus, 0xa5) === 0xa5` and one decoded `a5`
   byte.
2. Transfer `[0x00, 0xff, 0x55, 0xaa, 0x81, 0x7e]`. Require an equally sized
   returned array with exact byte equality and one matching analyzer transaction.
3. Build exactly `limits.maxTransferBytes` bytes as `index & 0xff`. Require the
   full returned array and analyzer capture byte-for-byte; no prefix or suffix
   may be duplicated, truncated, or zero-filled.
4. Attempt an array of `limits.maxTransferBytes + 1`. It must throw an uncoded
   `RangeError` before SCK or MOSI activity. Current RP descriptors advertise
   256 bytes; the ESP32-S3 polling/no-DMA descriptor advertises 64 bytes. Assert
   the capability value rather than branching on the platform name.
5. Attempt an empty array, missing data, string/fractional/non-finite bytes, `-1`,
   and `256`. Require the schema's uncoded `TypeError`/`RangeError` and no bus
   activity.

A floating-MISO smoke test is not a substitute. It can prove driver lifecycle
and responsiveness, but it cannot prove returned bytes, full duplex, or signal
integrity.

## Modes, timing, and routes

- Repeat the scalar and six-byte array rows for every value in `limits.modes`.
  For each accepted mode, require the standard clock contract:

  | Mode | SCK idle | Sample edge | Shift edge |
  |---|---|---|---|
  | 0 | low | rising | falling |
  | 1 | low | falling | rising |
  | 2 | high | falling | rising |
  | 3 | high | rising | falling |

  A mode absent from `limits.modes` must throw uncoded `RangeError` before native
  teardown or bus activity.
- Verify eight SCK periods per byte and MSB-first decoding. `bitsPerWord` and
  `bitOrders` are descriptive fixed capabilities in 0.2; passing similarly
  named init options must throw because those formats are not selectable.
- Test each physically accessible listed bus/route. A non-default bus requires
  explicit `bus`, `sck`, `mosi`, and `miso` from one listed route. Mixed or
  partial routes must throw before pin muxing.
- Test at least one additional in-range exact frequency per backend. An in-range
  integer that cannot be represented exactly must throw `NotSupportedError`
  with `code === 'ERR_NOT_SUPPORTED'`, leaving the previous configuration usable.
- Measure each accepted frequency rather than trusting a rounded SDK getter.

## Ownership, reinitialization, and native failures

1. Initialize the route pins as GPIO, then initialize SPI. Stale GPIO access must
   throw `ResourceBusyError` with `code === 'EBUSY'` while SPI owns the route.
2. Successfully reinitialize onto another listed route. Require the old pins to
   become inactive and explicitly reclaimable with `gpio.init()`; stale GPIO
   configuration must not reappear automatically.
3. Transfer before initialization must throw `EBUSY`. A generic native transfer
   failure must throw `EIO` with `resource === 'spi'`, selected `bus`, and numeric
   `nativeCode`; it must not return a partial or zero-filled success value.
4. During fault-injected ESP testing, device-removal failure must retain the
   device handle and all route claims. Bus-free failure after successful device
   removal must retain bus allocation and route claims. A later `init()` must be
   able to retry cleanup after the fault is removed.
5. Driver-slot or allocation exhaustion must throw `ResourceExhaustedError` with
   `code === 'ERR_RESOURCE_EXHAUSTED'` and a numeric `limit`. No other binding may
   remux a still-live or quarantined route.

Host-native SDK-stub tests supply the repeatable fault injection for teardown and
allocation rows. Hardware runs prove route ownership and recovery without
manufacturing dangerous electrical faults.

## Optional RP DMA compatibility extension

`writeBufferDMA` is not portable SPI. Check both `limits.dma` and
`limits.compatibilityExtensions.includes('writeBufferDMA')`, then feature-detect
the method. Firmware without that exact capability must omit it.

Where the RP display stack exposes the compatibility method, test it only with a
live graphics buffer from that stack. An invalid/stale handle and a byte length
larger than that buffer must throw `RangeError`; use before `spi.init()` must
throw `EBUSY`; DMA-channel exhaustion must throw
`ERR_RESOURCE_EXHAUSTED`. Do not invent or standardize numeric graphics handles,
and do not use this row to claim portable DMA parity.

## Evidence and limitations

Retain submitted JavaScript, exact returned values/errors, analyzer captures,
measured frequency/mode, route/bus identity, transfer lengths, firmware logs,
and pass/fail status for every row. List skipped routes, modes, rates, or DMA rows
with the exact physical-access, capability, fixture, or electrical reason.

The repository's host tests prove production binding validation, exact
64/65-byte and 256/257-byte boundaries, scalar/array shape, error mapping, and
lifecycle state against SDK stubs. This protocol proves physical clock mode,
timing, full-duplex byte integrity, and electrical interoperability. Neither
layer substitutes for the other.
