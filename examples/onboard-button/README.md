# Onboard button → LED (no wiring)

Supported firmware: Raspberry Pi **Pico (RP2040)** and Seeed **XIAO ESP32-S3**.
Use **BOOTSEL** on Pico or **BOOT** on XIAO (not RESET). Do not hold the button
while resetting or powering on: that can enter bootloader/recovery mode.

Copy `index.js` into `/lib/onboard-button/index.js` on the MCUJS filesystem,
then run manually from the REPL:

```js
require('/lib/onboard-button');
```

Do **not** copy it to `/index.js`; this example is not an autorun application.
It uses the same board API on both boards: `board.buttonPressed()` returns a
boolean and accepts no arguments. `board.devices.button` describes a managed,
read-only button, not a pin available to generic GPIO or other peripherals.
Unsupported boards omit the method and metadata and the demo exits harmlessly.

The LED initially follows the button directly. After that, three matching 10ms
samples debounce each change; only state changes are logged. After 60 seconds
the interval is cleared, the LED turns off, and `Demo complete!` is printed.
The REPL remains responsive throughout. As with other required modules, a reset
clears the module cache before running it again.
