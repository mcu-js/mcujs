---
title: Configured button events
---

# Configured button events

First, buttons-only slice of [issue #8](https://github.com/mcu-js/mcujs/issues/8),
stacked on configured display handles. No touch, Pointer Events, generic GPIO
registration, or new scheduler is included.

```js
var devices = require('devices');
if (devices.button) {
  var button = devices.button.open();
  button.addEventListener('press', function () { console.log('Pressed'); });
  button.addEventListener('release', function () { console.log('Released'); });
  button.addEventListener('error', function (event) { console.error(event.error); });
  console.log('Initial state:', button.pressed);
  // At the end of this owner's lifetime: button.close();
}
```

The runnable `examples/onboard-button/index.js` uses the same input code on Pico
and XIAO, mirrors state to an available onboard LED, and closes after 60 seconds.
Application code contains no GPIO, controller names, or board-name branches.

## Qualified inputs and recovery boundaries

- **Pico RP2040:** the existing managed, read-only BOOTSEL sampler. Its SRAM and
  interrupt/QSPI-state preservation is unchanged. The existing restrictions on
  concurrent other-core, DMA or XIP-streamer flash access still apply.
- **XIAO ESP32-S3:** the existing managed, input-only BOOT sampler on its reserved
  strapping pin. No public GPIO write access is added.
- Other profiles omit `devices.button`, including Pico 2 and Sticky. Physical
  presence of a reset/power/boot control does not establish a qualified app input.
  **Reset is not an app button.** Sticky power/reset behavior is untouched; this
  slice does not yet make The Night Ferry interactive on Sticky.

Bootloader, safe-mode and recovery behavior remain authoritative. Sampling does
not consume or override them. No GPIOs, pull settings, or polarity options are
accepted by `open()`.

## Discovery, ownership and timing

`require('devices')` and its stable `button` descriptor are frozen. Discovery,
capability reads and descriptor state do not sample hardware or allocate timers.
Availability follows the compiled board, timer, require and managed-button
features, not physical inventory alone. Display and button capability records
compose without replacing each other.

Descriptor `.state` is `idle`, `busy`, or `unavailable`. Idle means no polling
owner, not physical readiness. `.open()` takes no arguments and returns one
exclusive polling handle. Direct read-only `board.buttonPressed()` remains a
raw observation; this lease does not claim a GPIO/bus arbiter or prevent reads.

The handle has:

- `.pressed`: the initial read at open, then the latest **debounced** boolean.
- `.state`: `open`, `closed`, or `error`.
- `press` / `release` events: genuine bounded `Event` instances, synchronous on
  the existing cooperative timer pass, with `target`, `currentTarget` and callback
  receiver equal to the handle. There is no initial synthetic edge, held-button
  repeat, or synthetic release at close. Manually dispatching an event does not
  change the physical/debounced state.
- `addEventListener`, `removeEventListener` and `dispatchEvent`: the existing
  bounded EventTarget behavior, including `once`, AbortSignal cancellation,
  listener mutation and exception reporting. No second callback queue is added.
- `.close()`: repeatable timer release and removal of all current listeners and
  external signal subscriptions. It does not abort callers' signals, drive a pin,
  or turn off hardware. Closing an old handle cannot release a new owner's timer.

Each owner uses one existing interval slot, requested every **10 ms**. Both edges
need an unchanged sampled candidate for **30 ms**, measured with wrap-safe
32-bit `board.millis()` differences. This is cooperative polling, not interrupt
capture or a hard real-time guarantee. Blocking JavaScript or display refresh can
delay delivery and miss entire taps; missed transitions are not synthesized or
replayed. The initial sample establishes state immediately without an event.

Closing stops further samples, including when called inside a listener. Reading
`.pressed`, adding listeners or dispatching through the closed handle rejects
with `ENXIO`; removing listeners remains harmless cleanup. As with the other JS
modules, encapsulation is not a hostile-realm boundary: applications must use the
owned handle methods rather than bypassing them through borrowed base methods.

## Failure and lifecycle

- `EBUSY` / `ResourceBusyError`: another owner exists.
- `EIO` / `Error`: the managed sampler fails or returns an invalid value.
- `ERR_RESOURCE_EXHAUSTED` / `ResourceExhaustedError`: no polling timer can be allocated.
- `ENXIO` / `Error`: use of a closed/failed handle.
- Invalid open arguments produce `TypeError`; listener argument errors and limits
  retain the events contract. The shared limit remains 16 listeners and 16 signal
  subscriptions, with the existing nested-dispatch budget.

A poll failure stops the timer, releases the owner, sets handle `error` and
descriptor `unavailable`, then dispatches one `error` event whose `.error` carries
the failure. All listener/subscription cleanup follows, even if listeners throw
or close the handle. The old handle remains `error` after close. A successful
reopen clears unavailable; stale handles cannot disturb it.

VM teardown uses the existing production timer cleanup and builtin-module cache
cleanup before Jerry destruction. No new native sampler state, interrupt handler
or timer pool is introduced.

The events foundation adds one small MCU.js extension:
`EventTarget.clear(target)` removes all **current** registrations and signal
links. It does not close the target, stop hardware, abort a signal or prevent
future registration. The button owner uses it during close/error; normal apps
should use `button.close()` to release the device. This is not a DOM-standard API.

## Evidence boundary

Node tests replay the real JS modules with simulated clock/input. Native tests
link the production Pico/XIAO board and timer bindings plus devices/button/events
factories into pinned JerryScript with a 64 KiB heap. GPIO/QSPI sampling and time
are SDK-boundary fakes; an explicit sampler decorator injects EIO. Their small
cached factory router is not the production module loader; the existing runtime
binding suite separately tests that loader and VM cache recreation.

Tests cover debounce/bounce/hold, initial state, both edges, close during delivery,
AbortSignal/once/remove/pressure, timer exhaustion, error/recovery, counter wrap,
GC workloads and VM recreation. Firmware builds establish compile/link/packaging,
not electrical or physical timing acceptance. No device is flashed, merged or
released by these tests. Touch and browser pointer adapters remain separate.
