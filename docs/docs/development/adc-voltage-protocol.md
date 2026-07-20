# ADC voltage and temperature acceptance protocol

This protocol is the physical acceptance gate for the portable MCU.js 0.2 ADC
contract. It validates an already-installed reviewed firmware image. It does not
flash firmware, enter a bootloader, mount or eject storage, format a filesystem,
or write user data.

## Safety and prerequisites

- Use only pins listed by `board.capability('adc').pins` and channel entries from
  `.channels`. Do not infer an ADC route from `board.name`, `board.chip`, or a
  silicon data sheet.
- Connect the board and reference source grounds before connecting the signal.
  Never exceed the board's electrical input limit or the capability's advertised
  `voltage.maxVolts`.
- Use a current-limited precision source or a measured divider. Verify each test
  voltage with a calibrated multimeter at the MCU pin before calling MCU.js.
- Disconnect sensors, pull-ups, displays, and other loads from the selected pin.
  Stop if the pin is not safely accessible on the exact board.
- Record the firmware commit SHA, board ID, `board.apiVersion`, full ADC
  capability descriptor, source and meter models, meter reading, wiring, ambient
  conditions, and every returned value.

`voltage.calibrated` describes the conversion implementation, not a promise that
all board-level source, reference, noise, and meter errors have vanished. ESP32-S3
uses its calibration driver and reports `calibrated: true`. RP2040/RP2350 convert
against the runtime's nominal 3.3 V reference and report `calibrated: false`.

## Route selection without board sniffing

Select one advertised alias, then use the same descriptor entry for both APIs:

```js
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
  if (!route) throw new Error('This board has no advertised A0 ADC route');

  console.log('raw pin', adc.readPin(route.pin));
  console.log('raw channel', adc.readChannel(route.channel));
  console.log('volts pin', adc.readVoltagePin(route.pin));
  console.log('volts channel', adc.readVoltageChannel(route.channel));
}());
```

Repeat route discovery for the first and last entries in `capability.channels`.
Every alias in an entry must resolve to the same numeric pin in `board.pins`.

## Ground and known-voltage checks

For each selected route:

1. Connect the input to ground. Take 32 raw pin readings, 32 raw channel readings,
   and 32 voltage readings through each form. Record minimum, maximum, and mean.
   The raw values must remain in `0..(2^resolutionBits - 1)`, and voltage must
   remain in `voltage.minVolts..voltage.maxVolts`.
2. Apply a meter-verified **1.000 V** reference. Take the same sample set. The
   voltage mean from both pin and channel forms must be within the larger of
   **0.100 V or 5% of the meter reading**. Pin and channel means must agree within
   **0.050 V**.
3. Apply a second reference near **2.000 V**, but only when it remains inside the
   documented accurate range for the exact board and attenuation. Use the same
   limits. Do not use 3.3 V as the ESP32-S3 accuracy oracle merely because it is
   an electrical rail.
4. Require the raw mean to increase monotonically from ground to each higher
   source. No method may return millivolts: a 1.000 V input must be approximately
   `1`, never approximately `1000`.

Noise may make consecutive pin and channel samples differ. Compare sample means,
not individual conversions. Never clamp an out-of-contract result in the test
harness; record it as a failure.

## Temperature checks

If `capability.temperature.supported` is true:

1. Call `adc.readTempC()` 32 times after the board has idled for five minutes.
2. Require finite numeric Celsius results and record minimum, maximum, and mean.
3. Repeat after a known workload and require continued responsiveness and no USB
   or watchdog failure.

The reading is MCU die temperature, not ambient temperature. Do not grade it
against room temperature without a board-specific thermal model. If
`temperature.rawChannel` is true and `adc.TEMP` exists, that property is an RP
0.x compatibility alias for raw channel 4; it is not part of the portable five
method surface and is not equivalent to `readTempC()`.

## Ownership and recovery checks

- Hold the selected pin with active PWM or another long-lived peripheral. Every
  ADC pin/channel/voltage operation targeting that route must throw
  `ResourceBusyError` with `code === 'EBUSY'` and must not disturb the owner.
- Release that peripheral and prove ADC succeeds.
- After a successful one-shot ADC read, explicitly initialize another supported
  peripheral on the pin and prove it can take over. A stale GPIO operation from
  before the ADC read must not silently regain access; reinitialize GPIO first.
- Injected native failures belong to the native SDK-stub suite. On hardware,
  record any `EIO` with its `resource`, `pin` when applicable, and `nativeCode`.

## RP compatibility channels

`adc.TEMP` and `adc.VSYS` are deprecated, nonportable 0.x compatibility
properties. Feature-detect the property itself. `adc.TEMP`, when present, is raw
channel 4. `adc.VSYS`, when present, is raw ADC channel 3 connected to the
board-specific VSYS/3 path; it does not mean every RP board exposes VSYS.

These internal compatibility channels are intentionally separate from
`capability.channels`, whose entries describe board-exposed pin/channel pairs.
Portable code uses `readTempC()` and advertised external routes instead.

## Evidence and pass criteria

Retain raw samples or CSV, calculated statistics, multimeter readings, submitted
JavaScript, exact errors, and a pass/fail line for every route and voltage. Mark a
row untested when safe access or suitable equipment is unavailable; do not
substitute native stubs, compilation, an unmeasured potentiometer, or a plausible
number for electrical evidence.
