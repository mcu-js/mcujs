# Configured buzzer (first tone slice)

Discover support without opening hardware:

```javascript
var devices = require('devices');
if (devices.buzzer) {
  var buzzer = devices.buzzer.open();
  buzzer.beep({ frequency: 1000, duration: 200 }).then(function (tone) {
    console.log('Tone finished:', tone.frequency);
    buzzer.close();
  }, function (error) {
    buzzer.close();
    console.log('Tone failed:', error);
  });
}
```

Only Waveshare RP2350 Touch LCD 1.69 has this adapter. Other boards omit
`devices.buzzer`, its capability, and both private `mcujs:buzzer` modules.
No Sticky buzzer, speaker, microphone, PCM, WAV, mixing, queue or Web Audio.

## Contract

- `devices.buzzer.capabilities` is frozen static metadata. Descriptor `state`
  is `idle` or `busy` (including another owner of the shared PWM slice).
- `open()` takes no arguments and returns one exclusive frozen handle. No tone
  starts on open. A second owner or occupied PWM slice throws
  `ResourceBusyError` (`EBUSY`, resource `buzzer`).
- `beep({frequency, duration, signal?})` returns a Promise. Required frequency
  is an integer 500–4000 Hz; duration is an integer 1–1000 ms. Missing/non-number
  values reject with TypeError; fractional/out-of-bounds values and unknown
  options reject with RangeError. No coercion or implicit default tone.
- Frequency and 50% square-wave duty must be exactly representable by the
  current RP PWM clock/divider/period. This interval does **not** mean every
  integer frequency is supported. Unrepresentable values reject with
  `NotSupportedError` (`ERR_NOT_SUPPORTED`), without starting output; e.g. do
  not assume 880 Hz is representable. 1000 Hz is the initial proof tone.
  There is no silent quantization. Successful completion resolves a frozen
  `{frequency, duration}` describing requested/configured settings, not an
  acoustic measurement or a timing-precision guarantee.
- A concurrent beep rejects `EBUSY`; there is no replacement or queue.
- `signal` must be a branded MCU.js `events.AbortSignal`. Real cancellation
  rejects with its exact reason and stops output before ordinary abort event
  listeners run. Pre-aborted signals never start output; synthetic `abort`
  events do not cancel. Listener propagation cannot suppress cancellation.
- `stop()` silences immediately and rejects a pending beep with `AbortError`
  (`ABORT_ERR`). An idle open handle can be stopped repeatedly.
- `close()` stops, rejects pending work the same way, and releases ownership;
  repeated close is harmless, including after another handle opens. Other
  operations on a closed handle fail `ENXIO` (beep rejects).
- Handle `state` is `open`, `playing`, or `closed`. An expired hardware tone
  can report `open` while its Promise still awaits JavaScript servicing.
- Alarm or JS completion-timer exhaustion rejects
  `ResourceExhaustedError` (`ERR_RESOURCE_EXHAUSTED`). No PWM starts without a
  native safety alarm and a completion timer reserved first.

## Safety and qualification boundary

GPIO2 is reserved from generic GPIO/PWM/other pin APIs even while closed.
Opening reserves the entire PWM slice, excluding alias pins and its sibling
channel; existing generic PWM frequency/duty/sharing semantics on other
resources are unchanged. Closing/cleanup restores low output and frees the
slice. A native Pico alarm IRQ shuts PWM off without running JS or waiting
for Promise jobs; clearing a JS timer cannot leave a tone running forever.
Timer/VM teardown cancels that alarm and silences output before Jerry cleanup.
As with all IRQ deadlines, interrupt masking or hardware failure can delay
shutdown; this is not a hard real-time acoustic duration guarantee.

The adapter follows the [Waveshare schematic](https://files.waveshare.com/wiki/RP2350-Touch-LCD-1.69/RP2350-Touch-LCD-1.69.pdf):
GPIO2 `Buzz`, AC coupling via 10 µF C33 to an SS8050 base, a 4.7 kΩ base
pull-down, and a 3V3 buzzer. Idle is low. This is schematic-based adapter
qualification. The old generic GPIO2 PWM melody example is not this portable
API and must not be used as its proof.

### Hardware evidence (2026-09-10)

Firmware `24b1f5b533728ea9788872c7eb2c50ad575ab7ea` was built for all 13
boards and flashed to the 1.69 reference device. Full binary readback matched;
two matching full-flash backups were retained before the change. All 17 original
files were restored and hash-verified before testing.

A local C920 microphone recording and matching REPL markers verified:

- 1000 Hz / 400 ms completion, then a fresh 1000 Hz / 100 ms tone after reopening.
- A requested 1000 ms tone at 2000 Hz cancelled after about 200 ms by AbortSignal.
- A requested 1000 ms tone at 1500 Hz closed after about 150 ms.
- A requested 1000 ms tone at 3000 Hz stopped after about 150 ms.
- A 200 ms tone stopped while JS remained in a bounded 700 ms busy loop.
- A software reset requested during a tone left output quiet after reboot;
  a fresh handle then played and closed successfully. `board.reset()` has an
  existing 250 ms USB-reset delay: reset request is **not immediate mute**.

FFT analysis detected the expected tone bands, harmonics and quiet gaps. This is
functional acoustic proof, not calibrated frequency/duration, loudness, fidelity,
or power-cycle qualification. Windowing, room noise and buzzer ringing broaden
measured acoustic intervals; requested timings above are not calibrated results.
The first reset harness attempt waited too briefly for USB re-enumeration; the
successful rerun waited for the identified runtime and synchronised the REPL.

After reset testing, all original program files still matched their backups;
only the pre-existing app's `settings.json` boot counter changed (5 to 7).
The only added app file was the bench proof helper. Final buzzer state was `idle`.
The 1.47 SD/BMP device was not modified.

Local evidence: `plans/mcujs-buzzer/` contains UART logs, recordings and the
analysis script. Original recording SHA-256 values:

- Lifecycle: `7dc6211ce91b44afbb2facda605878a7b14f82f923ea69693f2d610157a5c233`.
- Busy-JS/reset: `98f76c378ab2ee0ef933f7fcd8772a9c97f646cc8b603a4d987e5cd5c0fe583b`.


`AbortSignal.subscribe(signal, callback)` is a bounded MCU.js lifecycle
extension added for native-device cancellation (returns an unsubscribe
function). It is not a Web/DOM standards-conformance claim. Native buzzer
module methods are implementation details; applications use `devices`.
