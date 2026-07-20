# PWM waveform acceptance protocol

This protocol is the physical acceptance gate for the portable MCU.js 0.2 PWM
contract. It is deliberately non-destructive: it validates an already-installed,
reviewed firmware image and does not alter firmware, bootloaders, partition
tables, or user storage.

## Safety and prerequisites

- **No flashing**, UF2 copying, mounting/ejecting, formatting, or filesystem
  writes are part of this protocol. Stop if the expected firmware is not already
  running.
- Disconnect unrelated loads. Drive only a logic-analyzer input or an LED through
  a suitable resistor, and connect analyzer ground to board ground.
- Confirm the selected pins in `board.capability('pwm').pins` before wiring.
  Never substitute a pin based on the board or chip name.
- Use a logic analyzer with a sample rate of at least **10 MS/s**. Capture at
  least **100 cycles** for every non-static waveform.
- Record the firmware commit SHA, board ID, `board.apiVersion`, capability
  descriptor, instrument model/settings, physical pins, and either exported CSV
  data or screenshots. A host/native test result is not waveform evidence.

## Acceptance limits

For every requested non-static waveform:

- measured frequency error must be at most **0.5%**;
- measured duty error must be at most **0.25 percentage point**;
- no unexpected pulse, missing cycle, or phase-length discontinuity may appear
  after the observation window begins.

For duty `0` and `1`, observe a stable low or high rail respectively for at least
the duration of 100 nominal periods. Record the measured rail voltage; do not
infer a static level from a decoder that reports no edges.

## Board and pin matrix

| Target | Primary outputs | Alias/contention probe | Notes |
|---|---|---|---|
| RP2040 Pico-class board | GPIO0 and GPIO1 | GPIO16 aliases GPIO0's slice/channel | Confirm GPIO16 cannot own the already-active GPIO0 output. |
| RP2350 Pico 2-class board | GPIO0 and GPIO1 | GPIO16 aliases GPIO0's slice/channel | Run with the production 150 MHz firmware clock. |
| Seeed XIAO ESP32-S3 | GPIO1 and GPIO2 | GPIO1 through GPIO9 for exhaustion | LEDC exposes eight output channels and four timers. |

If a named pin is absent from the running capability descriptor, record the
matrix row as not executable for that exact board; do not improvise an internal
or reserved pin.

## Common exact-waveform baseline

For each target and primary output:

1. Verify `require('mcujs:module').has('pwm')` and read
   `board.capability('pwm')`.
2. Call `pwm.init(pin, 1250)`.
3. Apply duty ratios `0`, `1 / 64`, `1 / 2`, and `1` in that order. Capture and
   assess each steady state separately against the limits above.
4. Reinitialize the same pin at 1000 Hz. Confirm the output first resets to a
   stable low level, then apply `1 / 2` and verify the new frequency and duty.
5. Call `pwm.stop(pin)`. Confirm a stable low rail, then initialize the same pin
   with `gpio.init(pin, gpio.OUTPUT)` and prove one deliberate low/high/low GPIO
   sequence. This is explicit GPIO reuse, not restoration of stale GPIO state.

Run the example twice in one persistent realm as a separate lifecycle check.
Only `examples/pwm-fade/index.js` may be used for this step; require one active
interval, ratio-only updates, clean replacement of the old output, and the final
`Demo complete!` marker.

## Non-destructive rejection checks

Keep a 1250 Hz, `1 / 2` reference waveform active and capture continuously while
issuing the invalid reconfiguration:

- RP2040 and RP2350: request **1100 Hz** on that same pin and require
  `NotSupportedError` with `code === 'ERR_NOT_SUPPORTED'`.
- ESP32-S3: request **1601 Hz** on that same pin and require
  `NotSupportedError` with `code === 'ERR_NOT_SUPPORTED'`.

The pre-existing waveform must remain within the frequency/duty limits with no
stoppage or discontinuity. A thrown error without a preserved waveform is a
failure.

## Sharing, aliases, and finite resources

### RP2040 and RP2350

1. Initialize GPIO0 and GPIO1 at 1250 Hz and set both to `1 / 2`. Verify both
   waveforms concurrently.
2. Stop GPIO0. GPIO1 must continue without interruption; stop GPIO1 only after
   its survivor capture passes.
3. Initialize GPIO0 at 1250 Hz, then attempt GPIO16 at 1250 Hz. Require
   `ResourceBusyError` with `code === 'EBUSY'` and no change to GPIO0.
4. On one slice, initialize one channel at 1000 Hz and request 1250 Hz on the
   other channel. Require `EBUSY`; the existing waveform must survive.
5. Stop the owner before proving that the released alias or slice channel can be
   initialized successfully.

### ESP32-S3

1. Initialize GPIO1 and GPIO2 at 1250 Hz. Verify same-frequency timer sharing,
   stop GPIO1, and prove GPIO2 remains writable and waveform-clean. The timer
   must be released only after GPIO2 stops.
2. Initialize GPIO1 through GPIO8 at 1250 Hz. The GPIO9 initialization must throw
   `ResourceExhaustedError` with `code === 'ERR_RESOURCE_EXHAUSTED'` and
   `limit === 8`, with all eight existing outputs preserved. Stop one output,
   then prove GPIO9 can take the freed channel.
3. Use four exact frequencies—1000, 1250, 2000, and 4000 Hz—on four outputs.
   A fifth distinct exact frequency of 5000 Hz must throw
   `ERR_RESOURCE_EXHAUSTED` with `limit === 4`; all existing waveforms must
   survive. Stop one frequency owner, then prove the fifth frequency can start.

## Evidence record

For every row, retain:

- submitted JavaScript and exact returned error class/name/code/diagnostics;
- pre-operation and post-operation captures, including the rejection window;
- measured frequency, duty, rail voltage, cycle count, and analyzer sample rate;
- pass/fail against each numeric limit;
- any skipped row with the exact missing capability, equipment, or safe pin.

Do not report this protocol as passed from native stubs, compilation, or visual
LED inspection. Those prove different layers of the contract.
