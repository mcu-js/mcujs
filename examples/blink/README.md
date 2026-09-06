# First light: blink an onboard LED

Run a short program, watch the onboard LED change, then make it blink faster.
No breadboard or external wiring is needed. Use only the onboard LED; do not
connect loads or apply external voltages to GPIO for this lesson.

**Pilot evidence:** the JavaScript is executed in Node tests with simulated
timers and GPIO/board SDK calls. The device steps below are source-checked
instructions, not a report of connected boards or a physical demonstration.
Native-runtime tests, firmware builds, and physical checks remain pending.

## 1. Make the light blink

Start with a **Raspberry Pi Pico (non-wireless, `pico`)** already running MCU.js
firmware from this portable-API source line (0.2 API). You need the `MCUJS` file
volume and a serial terminal connected to its CDC port at **115200 baud**.
Use `.info` to check the board and record the firmware build ID. Older release
firmware may not expose `board.devices`; a 0.1 banner alone does not identify
this source/API. Firmware installation is a separate prerequisite: see the
[root setup guide](../../README.md), not a step performed by this pilot.

1. Open the mounted `MCUJS` volume in your file manager. Copy
   [`index.js`](index.js) from this folder to the volume root, **renaming the
   destination to `first-light.js`**. Do not overwrite an existing startup
   `index.js`. The [`blink.js`](blink.js) variant has the same behavior.
2. Save and **properly eject the volume** in your operating system, leaving
   the USB cable connected for serial. The device must regain filesystem
   ownership before it can read your file.
3. Enter these lines in the MCU.js serial REPL, one at a time (not in your
   computer's shell):

   ```javascript
   require('board').storageReady()
   ```

   Continue when this returns `true`, then:

   ```text
   .ls
   .run /first-light.js
   ```

Expected: `Blinking onboard LED every 500 ms.` The LED starts off, turns on
at the first timer tick, then alternates every 500 ms. After 30 seconds it
turns off and prints `Demo complete!`. Timers let the REPL remain responsive;
there is no blocking endless loop.

**Run again:** enter `.run /first-light.js` again. You can also rerun while
it is blinking: the example replaces its old interval and stop timer, starts
off, and gives the new run its own 30 seconds. Use `.run`, not `require()`
(which caches file modules), and not `.load` (not a supported REPL command).

**Stop:** simply wait for `Demo complete!`, or stop early by entering:

```javascript
clearInterval(globalThis.blinkInterval);
clearTimeout(globalThis.blinkTimeout);
require('board').led(false);
```

Early stop cancels the completion message too. It turns the LED off, but does
not promise to release GPIO ownership for another peripheral. Rerunning the
file is still supported. No reset, format, or reflashing is needed to stop.

## 2. Change one thing

In the local `index.js`, change just this line:

```javascript
var blinkPeriodMs = 500; // Change this one value to adjust the blink.
```

to:

```javascript
var blinkPeriodMs = 250; // Change this one value to adjust the blink.
```

Stop the old run, copy the edited file over `first-light.js` on the host-mounted
volume, eject it again, check `storageReady()`, and run `.run /first-light.js`.
If the ejected volume is no longer accessible to your file manager, reconnect
USB to obtain it again, then reopen the serial terminal before running.

Expected: the message now says `250 ms`; the LED alternates faster. This number
is the time **between changes**, not an entire on/off cycle. The automatic stop
is still 30 seconds. Change it back to `500` to restore the original behavior.

## 3. If nothing happens

- **Cannot open the file / filesystem busy:** check the filename with `.ls`.
  Eject the host volume and wait for `require('board').storageReady()` to be
  `true`. Do not format the filesystem to solve an ownership problem.
- **`This board has no onboard LED.`:** the example intentionally returns
  without starting timers. Choose a configuration with a declared LED; an RGB
  NeoPixel is not automatically the single LED this lesson uses.
- **`board.devices` or `storageReady` is missing:** check the firmware/source
  version prerequisite; do not replace the example with a guessed GPIO number.
- **`ResourceBusyError` / `EBUSY` for GPIO:** another peripheral may own the
  LED pin. Stop that peripheral using its documented API before retrying.
  Blink deliberately propagates initialization errors instead of announcing
  success. Cancelling a peripheral's timer alone may not release its pin.

## Appendix: run the same application on another board

The source-supported configurations for this pilot are:

| Firmware target | Onboard output | Electrical off / on |
| --- | --- | --- |
| `pico` (RP2040, non-wireless Pico) | GPIO 25, active-high LED | `false` / `true` |
| `seeed_xiao_esp32s3` (Seeed Studio XIAO ESP32-S3) | GPIO 21, active-low LED | `true` / `false` |

These are declarations in [`runtime/board-registry.js`](../../runtime/board-registry.js),
not an inventory of available or tested physical boards. Both need matching
firmware with GPIO, board inventory, timers, serial, and filesystem support.
For XIAO, an application UF2 requires a compatible existing TinyUF2 baseline;
initial provisioning/recovery is separate and must preserve its partitions.
Read the [ESP32 setup prerequisites](../../platform/esp32/README.md). Do not use
the Pico BOOTSEL/UF2 installation procedure on XIAO.

Once configured, the **same `first-light.js` and the same load/run/stop steps**
apply to XIAO; no manufacturer branch or timing change is required. The example
reads `require('board').devices.led`. A GPIO LED uses its declared pin and
`activeLow` polarity, with strict boolean `gpio.set` values. A managed LED
(such as the `pico2_w` declaration) instead uses `board.led(on)`. Absence is
handled before GPIO is loaded. See [Runtime Basics](../../docs/docs/runtime-basics.md)
for `.run`, module caching, and filesystem ownership details.

### What the host tests prove

From the repository root, with Node already available:

```sh
node --test tests/examples-gpio-contract.test.js
```

The existing GPIO example test file loads and executes both real blink files.
It checks simulated timestamped active-high/active-low writes, the actual
one-line timing edit and its message, the 30-second off/stop, timer replacement
on rerun, managed and missing LEDs, and propagation of an injected GPIO
ownership error before any writes or timers. Pin configurations come from the
registry; expected transition values are explicit test literals.

These are **host JavaScript observations with simulated timers/SDK calls**.
They do not prove JerryScript compatibility, native error generation or pin
arbitration, USB file transfer, real scheduling precision, or electrical light
output. No native harness, firmware build, hardware test, or learner session
was performed for this pilot; those remain separate evidence and consent gates.
