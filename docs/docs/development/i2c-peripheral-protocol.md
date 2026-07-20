# I2C peripheral acceptance protocol

This protocol is the physical acceptance gate for the portable MCU.js 0.2 I2C
master contract. It is deliberately non-destructive: it exercises an
already-installed, reviewed firmware image against an isolated I2C target and
does not alter firmware or user storage.

## Safety and prerequisites

- **No flashing**, UF2 copying, mounting, formatting, or filesystem writes are
  part of this protocol. Stop if the expected firmware is not already running.
- Use 3.3 V logic, a **common ground**, and external SDA/SCL pull-ups appropriate
  for the bus capacitance (2.2–4.7 kΩ is a typical bench range). Disconnect any
  onboard or external device that could contend for the selected address.
- Select only a complete route from `board.capability('i2c').routes`. Never infer
  pins from a chip or board name. Confirm that the route is physically exposed
  before wiring.
- Use a logic analyzer with I2C decoding at a sample rate of at least 10 MS/s.
  Record decoded addresses, ACK/NACK bits, payload bytes, START/STOP conditions,
  and measured SCL frequency. Analyzer evidence is required; native SDK stubs do
  not prove electrical interoperability.
- Record the firmware commit SHA, board ID, `board.apiVersion`, complete I2C
  capability, target-emulator firmware/version, pull-up values, analyzer model
  and settings, and physical route.

## Target fixture

Use a dedicated I2C **target emulator** at 7-bit address `0x42`; a second MCU or
an analyzer with target mode is suitable. Its out-of-band console must expose
transaction counts and captured payloads so ACK alone cannot be mistaken for a
correct transfer. Configure it with this deterministic behavior:

1. Every master write is ACKed and captured exactly, including its byte count.
2. Every master read returns byte `((index * 17) + 3) & 0xff`, starting at index
   zero for each read.
3. Address `0x43` remains unclaimed and therefore NACKs.
4. The fixture can report the last write and read count without using the I2C bus.

Do not substitute an EEPROM unless a scratch region, original-byte backup, and
verified restoration procedure are explicitly part of the test. Register-pointer
writes to an arbitrary sensor do not prove general payload integrity or the
advertised transfer maximum.

## Discovery and default-route initialization

1. Verify `require('mcujs:module').has('i2c')` and capture
   `board.capability('i2c')`.
2. Require the capability to contain non-empty `buses` and `routes`, a
   `defaultBus`, one complete `defaultRoute` present in `routes`, an inclusive
   frequency range, and a positive `maxTransferBytes`.
3. Wire the emulator and analyzer to `defaultRoute`, then call:

   ```js
   var boardApi = require('board');
   var i2c = require('i2c');
   var limits = boardApi.capability('i2c');
   i2c.init({bus: limits.defaultBus, frequency: 100000});
   ```

4. Require the analyzer to show the declared pins active at **100 kHz** within
   2%, with clean logic levels and no activity on another listed route.
5. Repeat the same call and require successful teardown/reinitialization without
   a stuck-low line, duplicate transaction, watchdog reset, or loss of REPL
   responsiveness.

If 100 kHz lies outside the advertised range, record the row as not executable
for that descriptor rather than selecting an undeclared value. The current
shipping descriptors include it.

## Exact transfers and limits

With the default bus initialized:

1. Write `[0x00, 0xff, 0x55, 0xaa]` to `0x42`. Require return value `4`, exact
   target capture, and one decoded four-byte write.
2. Read 4 bytes from `0x42`. Require `[0x03, 0x14, 0x25, 0x36]`, target read count
   4, and one decoded four-byte read.
3. Write exactly `maxTransferBytes` using byte `index & 0xff`. Require the full
   length as the return value and byte-for-byte target capture; no suffix may be
   dropped.
4. Read exactly `maxTransferBytes`. Require the complete deterministic sequence
   and the exact target/analyzer byte count.
5. Attempt write and read lengths of `maxTransferBytes + 1`. Both must throw an
   uncoded `RangeError` before any START condition or target transaction count.
6. Attempt empty writes, byte values `-1`, `256`, fractions, `NaN`, and infinity;
   invalid read lengths `0`, fractions, `NaN`, and infinity; and addresses `-1`
   and `128`. Require uncoded `TypeError`/`RangeError` as specified by the schema
   and no bus activity.

## NACK and native failures

1. Read one byte from unclaimed `0x43`. Require an `Error` with
   `code === 'ENXIO'`, `resource === 'i2c'`, the selected `bus`, and numeric
   `nativeCode`. The analyzer must show address NACK and no payload.
2. Disconnect the emulator while leaving pull-ups installed and repeat. Require
   `ENXIO`; reconnect it before continuing.
3. Hold SCL low only through a current-limited fixture designed for this test.
   Require a typed operational failure with `nativeCode`, then release SCL and
   prove a later valid transfer succeeds. The current RP timeout mapping is
   `EBUSY`; the current ESP transfer-timeout mapping is `EIO` (ESP teardown or
   configuration timeout is `EBUSY`). Never short a driven rail directly.
4. A generic native failure must surface as `EIO`; it must not be converted into
   an empty array, zero-length success, or `ENXIO` unless the backend actually
   distinguishes a no-target/NACK result.

## Frequencies, routes, and buses

- Run the exact-transfer rows at 100 kHz on every physically accessible listed
  bus/route. A non-default bus must be initialized with explicit `sda` and `scl`
  from one listed route.
- If the target and wiring are rated for **400 kHz**, request it and either prove
  the measured clock within 2% or require `ERR_NOT_SUPPORTED` before any route
  teardown when that in-range value is not exactly representable. The existing
  100 kHz bus must remain usable after rejection.
- Also test one exact high-rate candidate accepted by each backend, up to the
  target's electrical rating. Capability range membership alone does not promise
  that every integer frequency is representable.
- After successful reinitialization onto another route, require the old pins to
  be inactive and explicitly reclaimable with `gpio.init()`. The new route must
  remain exclusively owned until another successful I2C reinitialization.
- If any native teardown step fails, require `EBUSY` or `EIO`, retained ownership,
  and a successful retry after the injected fault is removed. Never accept a
  teardown error followed by another peripheral silently remuxing a live bus.

## Evidence and limitations

Retain submitted JavaScript, exact returned values/errors, target-emulator logs,
analyzer captures, measured SCL frequency, route/bus identity, transfer lengths,
and pass/fail status for every row. List skipped routes or rates with the exact
physical-access, fixture, or electrical-rating reason.

The repository's host tests prove production binding validation, transfer
boundaries, error mapping, and lifecycle state against SDK stubs. This protocol
alone proves physical ACK/NACK behavior, pull-up/line integrity, measured timing,
and byte-level interoperability. Neither layer substitutes for the other.
