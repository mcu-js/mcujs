# Onboard button → LED (no wiring)

Use firmware that advertises `devices.button`: for example **Pico (RP2040)**
BOOTSEL or **XIAO ESP32-S3** BOOT (not RESET). Do not hold the button while
resetting or powering on; that can enter bootloader/recovery mode.

Copy `index.js` to the application volume as `onboard-button.js`, eject safely,
and wait for `require('board').storageReady()` before running:

```text
.run /app/onboard-button.js
```

The script checks module availability, opens `devices.button`, and listens for
`press`, `release`, and `error` events. It logs the initial state, then changes;
the declared onboard LED follows the button if present. No GPIO pin is guessed.
Missing configured input produces a skip message without timers or handles.

After **60 seconds**, it removes listeners, closes the button, turns the LED off,
and prints `Demo complete!`. Stop early with `onboardButtonStop()`. Running the
same `.run` command again first cleans up the previous run; a stale timeout
cannot close the new handle. Do not install this as `/app/index.js` unless you
intentionally want it to run at startup. `require()` caches modules; use `.run`
to execute the entry again without resetting.
