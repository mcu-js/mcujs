---
title: Configured display handles
---

# Configured display handles (experimental)

First vertical slice of [issue #5](https://github.com/mcu-js/mcujs/issues/5), on the
bounded-events/Promise stack. This is **not completion of all #5 peripherals or
shared display/SD arbitration**. No device event queue, DOM, plugin registry,
buttons, touch, SD, audio or sensors are added.

## Find, open, draw, show, release

```js
var devices = require('devices');
if (!devices.display) throw new Error('No configured display');
var display = devices.display.open();
try {
  var canvas = display.canvas;
  var ctx = canvas.getContext('2d');
  ctx.fillStyle = 'black';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = 'white';
  ctx.fillRect(0, 0, 8, 8);
  display.present();
} finally {
  display.close();
}
```

The portable example is `examples/portable/device-display/`. Its `draw.js`
consumer takes a display handle, not a board name or a driver. Setup can provide
an onboard `devices.display.open()` handle or the existing
`require('displays/st7789').connect(connectionOptions)` handle on an enabled RP
Canvas target. Put external pins/profile options in setup, never in `draw.js`.
This slice does not add external device registration to `devices`.

## Three different facts

- **Physical inventory:** `board.devices.display` describes board hardware. It
  does not prove firmware support, a working connection or display orientation.
- **Compiled support:** `board.capability('devices')` contains a `display` entry
  only when experimental Canvas **and a configured default adapter** are compiled.
  `require('devices')` exists on every target, but `.display` is otherwise absent.
- **Live state:** `devices.display.state` reads current native ownership/failure
  state. It does not probe the panel or claim that an external monitor is connected.

`devices` and its stable `display` discovery descriptor are frozen. Repeated
reads return the same descriptor, not new owners. `.capabilities` is the small
immutable registry record: `interface: 'canvas-2d-subset'`, `maxOpenHandles: 1`.
Discovery does not load Canvas JS, open SPI, refresh a panel or allocate a
framebuffer. It reads only the singular device capability, not the complete
capability graph. Actual logical dimensions become available lazily as
`handle.canvas.width` and `.height` after opening; physical inventory dimensions
may differ because of the configured orientation.

## Discovery and ownership contract

- Descriptor state **`idle`**: no Canvas connection owns the lease and no retained
  default-open/presentation failure. This is not a hardware-readiness guarantee.
- **`busy`**: a live Canvas connection prevents acquiring the configured default.
- **`unavailable`**: a default open or presentation failed and no connection is
  active. `open()` can retry; success clears this state. It is not cached forever.
- `display.open()` takes **no arguments**, acquires one exclusive configured
  Canvas lease, and returns a fresh owned handle. Closing then reopening returns
  a new handle; the discovery descriptor stays identical.
- While a default is owned, any other Canvas connection is rejected. Conversely,
  an existing external Canvas connection blocks default acquisition. This is
  deliberately conservative, not a general shared-bus/pin arbiter. External-only
  Canvas connections retain their existing native driver arbitration.
- No direct GPIO, legacy screen/DVI API or future SD driver is made safe to mix
  by this facade. The supported setup must not concurrently drive the display's
  dedicated pins through another API. Broader ownership belongs in later slices.

## Handle lifecycle

- `.canvas`: fixed identity and dimensions, existing opaque Canvas 2D subset.
- `.state`: **`open`**, **`closed`**, or **`error`**, read from the native instance.
  A presentation failure is visible even if it occurred in the main loop.
- `.present()`: synchronously presents this handle's pending frame. No pending
  drawing means no refresh. Ordinary main-loop presentation remains unchanged;
  there is no extra timer/job queue. E-paper may block for its existing bounded
  driver timeout and is not animation-rate or time-sliced.
- `.close()`: explicitly releases native resources, discards any pending frame,
  and is repeatable. **Draw then close does not show a frame: present first.**
  Closing an old or failed handle must never release a newer owner's resources.
- Failed presentation closes/releases the native connection. Its retained handle
  stays in `error`, including after repeated close; it cannot be revived. Open a
  new handle to retry. Cleanup/VM restart clears native leases and failure state.

Stable operational errors use the existing error vocabulary, with
`resource: 'display'`:

- **`EBUSY`**: exclusive configured Canvas lease conflict.
- **`ERR_RESOURCE_EXHAUSTED`**: known failure to allocate the native instance.
- **`EIO`**: adapter initialization or presentation failed. Existing adapter
  initializers return a boolean, so this slice deliberately does **not** pretend
  to distinguish allocation, external bus conflict and missing hardware there.
- **`ENXIO`**: drawing/presenting through a closed or failed handle.
- Invalid arguments remain `TypeError`/`RangeError`; they are not hardware state.

## Configuration and generated metadata

`runtime/board-registry.js` defines the small configured-device capability.
The generator emits its C constant, conditional compiled manifest, default
manifests, and `runtime/device-capabilities.json` from that source. The selected
CMake profile emits a matching `*.capabilities.json` beside build artifacts.
Default-profile manifests in `runtime/manifests/` intentionally do not advertise
experimental handles. Registry data **reports**, and never enables, a driver.
The existing platform CMake guards remain responsible for profile selection.

## Preview and verification boundary

A browser preview can pass `draw.js` a handle with an HTML canvas and explicit
`present`/`close` lifecycle, but it is an adapter, not the firmware device registry.
It must not advertise physical GPIO, bus ownership, e-paper waveform, power or
readiness from browser rendering. No generic browser plugin system is included.

Node tests cover discovery and the JavaScript contract. Native tests exercise
production bindings and adapters with SDK boundary fakes; they do not establish
physical orientation, ghosting, power use, cold boot or electrical bus behavior.
Firmware builds establish compile/link/packaging compatibility only. All physical
acceptance remains a separate, explicitly authorized step; nothing here flashes,
formats storage, migrates application paths, merges or releases firmware.

Pre-v1 change: default Canvas connections now have explicit exclusive ownership;
conflicts produce `EBUSY`. Use a single owned handle and release it before opening
another. `present()` and native-backed state are added to the display handle,
not to `CanvasRenderingContext2D`. No compatibility aliases or fallback devices
are introduced.
