# Portable API conformance

MCU.js 0.2 has one data-driven conformance framework for RP2040, RP2350, and
ESP32-S3 firmware. The public contract is the `x-mcujs-contract` object in
`docs/static/schemas/mcujs-portable-api-0.2.schema.json`; tests must not copy its
method, type, unit, availability, or error tables into board-specific fixtures.

Run the complete host/native gate with:

```sh
npm run test:conformance
```

The command runs descriptor checks, the schema-driven JavaScript cases, native
registry/JerryScript tests, production GPIO/I2C/PWM bindings against SDK stubs,
and REPL help/capability tests. `scripts/verify-release.sh --allow-dirty --docs`
runs the same gate before the docs typecheck and production build.

## Test lanes

The gate deliberately includes three production board maps:

| Lane | Board map | What it proves |
|---|---|---|
| Full RP | `pico` | The complete RP module surface remains require-able and discoverable. |
| Constrained RP2350 | `waveshare_rp2350_lcd_1.47_a` | Missing SPI/ADC APIs stay absent while shared APIs remain discoverable. |
| ESP32 | `seeed_xiao_esp32s3` | The constrained ESP surface and real ESP GPIO/I2C/PWM bindings obey the same contract. |

`tests/portable-api-conformance.test.js` generates an execution plan for every
shipping board. A board-map lane with no production observation reports every
operation as an explicit skip; it cannot turn an expected result into a mock
pass. Neither the runner nor hardware probe branches on `board.name` or
`board.chip`; availability and limits come from the selected manifest and
contract, while pass/fail results must come from a production binding or a real
serial callback.

The framework checks these discovery views independently:

1. native registration / require-able factories;
2. `require('mcujs:module').builtinModules`;
3. `modules.has(name)`;
4. `.help` module entries;
5. `board.capability(name)` and `board.capabilities()`.

A change to only one view therefore fails instead of being hidden by another
projection of the same list. Native registry lanes compile the production
GPIO/I2C/PWM factories for full RP, constrained RP2350, and ESP32, verify their
linked symbols and exact exports, and use the selected production registry for
advertised and known-unavailable modules.

## Boundary and result cases

`tests/conformance/portable-api-conformance.js` walks method signatures in the
schema and resolves constraints against the selected board descriptor. It
generates stable case IDs such as:

- `pwm.init.frequency.maximum`
- `pwm.init.frequency.maximum+1`
- `i2c.write.data.maximum`
- `i2c.write.data.maximum+1`
- `i2c.write.data.variant.uint8.maximum+1`
- `spi.transfer.data.maximum+1`
- `adc.readVoltagePin.return`
- `pwm.init.error.ERR_RESOURCE_EXHAUSTED.0`

Exact minima/maxima are success cases. Values outside a schema or
capability-derived range are uncoded `RangeError` cases. The generator also
covers missing/wrong types, non-finite and fractional numbers, fixed ranges,
valid and invalid correlated routes, symbolic enum constants, conditional pin
sets, resource-relative maxima, initialized-configuration maxima, and invalid
resource handles. Every accepted union branch is traversed independently.
Branch-qualified IDs such as `data.variant.uint8.maximum+1` prevent scalar
constraints from colliding with collection constraints, while `argumentPath`
remains the real invocation path (`data`) and the case value remains a directly
runnable scalar root fixture. Array contracts are traversed recursively, so
byte-array and nested NeoPixel element types receive their own finite, integral,
and `0..255` cases. Each item case carries a complete root-argument value with
the targeted leaf changed at `argumentPath`; surrounding arrays and objects
remain valid so an adapter can execute the case directly. A legal zero-item
minimum has no impossible minus-one case.

Return observations include JavaScript type and public unit, and numeric results
are checked against fixed, capability-derived, or bit-width-derived limits.
Correlated union results produce one case per accepted argument form: scalar
input must return the scalar result type, while array input must return an array
of the same length. Operational observations must preserve the schema's exact
error class, `.name`, and `.code`.

Host board maps create plans and discovery fixtures, not backend verdicts.
Module cards connect stable case IDs to real factories plus vendor SDK stubs,
then add physical tests for waveforms, voltages, buses, or external devices.
Evidence validation rejects missing and extra case IDs, unknown `has` keys,
unexpected lanes, missing or altered per-case expectations, and summary totals
that disagree with the case records.

## Extending the suite for a module card

Every later module implementation card follows this sequence:

1. Add or amend the method signature, result, unit, availability discriminator,
   capability-derived limits, and operational mappings under
   `x-mcujs-contract`.
2. Update board descriptors only for hardware the compiled firmware actually
   exposes. Do not add a board-name branch to the conformance runner.
3. Add a RED assertion in `tests/portable-api-conformance.test.js` for the new
   generated case ID or availability rule.
4. Compile the real backend factory/handlers in `scripts/test-runtime-validation.sh`;
   replace only vendor SDK calls with controllable stubs. Exact maximum calls
   must reach the stub; maximum-plus-one calls must throw before side effects.
5. Add a serial hardware case using the same stable ID. Record a skip with a
   concrete fixture reason when electrical equipment is required; do not report
   a mock result as hardware evidence.
6. Run `npm run test:conformance`, directly affected firmware builds, and
   `scripts/verify-release.sh --allow-dirty --docs`.

A schema change automatically affects the generated host cases and docs-facing
contract. This is how CI and the documentation site consume the same source
rather than maintaining parallel API tables.

## Serial hardware evidence

`tests/conformance/serial-hardware-probe.js` is ES5-compatible, rerunnable in the
persistent MCU.js realm, and capability-driven. Copy it to the device and run:

```text
.run /lib/serial-hardware-probe.js
```

The first run emits discovery-only output. A host collector then calls
`createSerialPlan({contract, descriptor, helpModules})` with the module list it
independently captured from `.help`, configures the persistent probe, registers
electrically safe callbacks by stable case ID, and calls `run()` again.

Each record is one line framed by ASCII Record Separator (`0x1e`) and Unit
Separator (`0x1f`):

```text
<RS>MCUJS_CONFORMANCE_V1 {"formatVersion":1,...}<US>
```

The bytes between the prefix and `<US>` are JSON. Host collectors must ignore
command echo, prompts, unframed lookalikes, and unrelated asynchronous output;
`parseEvidenceLines()` accepts only complete, exactly framed records. The
envelope includes:

- `formatVersion`, `apiVersion`, board ID, and lane;
- independently require-able registration, observed built-ins, complete
  `modules.has()` keys, external `.help`, capability names, and module exports;
- stable case IDs with expected and observed status/type/error fields;
- pass/fail/skip totals.

The generic probe performs non-destructive discovery. Peripheral boundary calls
can still affect attached hardware when the implementation is wrong, so they
are never guessed. Every planned but unregistered callback is emitted as a
`skip` with a non-empty reason. A module-specific harness registers electrically
safe exact-maximum and maximum-plus-one callbacks with
`__mcujsConformanceProbe.addCase(id, callback, options)` and calls `run()` again.
Skipped evidence is accepted only when the host validator explicitly enables
`allowSkipped`; it never counts as success.

## Migration from ad hoc 0.1 tests

Older tests commonly hard-coded board names, transfer sizes, or expected module
lists. For 0.2:

- resolve limits from `board.capability(name)` or the selected manifest;
- derive optional methods from capability fields or onboard inventory;
- preserve exact schema case IDs across host, native, serial, and CI evidence;
- keep programming errors uncoded and operational errors stably coded;
- keep unsupported APIs absent instead of adding throwing placeholders.

Backend-specific electrical assertions remain appropriate, but the JavaScript
contract they verify is shared.
