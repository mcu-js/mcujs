---
title: Canvas pointer input
---

# Canvas pointer input (experimental)

The first pointer slice is a single-contact input surface on the **configured
onboard display** of `waveshare_rp2350_touch_lcd_1.69` in an experimental Canvas
build. It does not enable touch on the 2.8, Sticky, external ST7789 panels, or
other profiles. Native/host tests and a Chromium drawing check have passed.
**Physical portrait drawing, release and basic finger alignment passed on
`f349bb4`**; precise edge calibration and physical landscape acceptance remain
outside this bounded acceptance.

```js
var display = require('devices').display.open();
var canvas = display.canvas;
if (canvas.maxTouchPoints === 1) {
  canvas.addEventListener('pointerdown', function (event) {
    console.log(event.offsetX, event.offsetY);
  });
  display.startPointer();
  setTimeout(function () { display.close(); }, 30000);
} else {
  display.close();
}
```

Opening the display does not open touch. `canvas.maxTouchPoints` reports the
implemented configuration (one or zero), **not a successful controller probe**.
`startPointer()` acquires the controller; repeated starts are idempotent.
Unsupported/closed displays throw `ENXIO`, conflicts throw `EBUSY`, and controller
initialization/transport failures throw `EIO`. `stopPointer()` stops sampling and
cancels any active contact; the display may remain open for drawing.

## Bounded event contract

Canvas uses the existing bounded `events.EventTarget`, including `once`, abort
signals and its existing listener limits. There is no DOM tree or propagation.

- Events: `pointerdown`, `pointermove`, `pointerup`, `pointercancel`.
- `pointerId` is stable during a contact and advances for the next contact.
- Hardware events have `pointerType: 'touch'` and `isPrimary: true`.
- Down/up: `button: 0`; move/cancel: `button: -1`.
- `buttons` is one during contact and zero on up/cancel.
- `offsetX/Y` are logical Canvas bitmap coordinates. In this subset `clientX/Y`
  are aliases of those coordinates, **not browser viewport coordinates**.
- `event.target` and `event.currentTarget` identify the Canvas during dispatch.
- No gestures, pressure, multitouch, hover, missed-tap replay or capture API is
  promised on firmware. Samples are cooperative, nominally every 16 ms; blocking
  application/display work can delay them. The controller interrupt pulse is not
  used as a held-contact signal.

A sample failure cancels instead of generating a false release, dispatches an
`error` event with its `error` cause, stops polling and clears subscriptions.
Explicit display close, presentation failure and orderly Canvas reset cancel an
active contact and release resources. Listeners are cleared on display teardown.
A native GC finalizer releases resources without invoking JavaScript. Abrupt
power loss/hardware reset cannot promise a delivered cancellation event.

## Controller and ownership

The [official Waveshare demo archive](https://files.waveshare.com/wiki/RP2350-Touch-LCD-1.69/RP2350-Touch-LCD-1.69-Code.zip)
is the protocol reference: `C/lib/Config/DEV_Config.h` and
`C/lib/LCD/Touch_1in69.{c,h}`. Its CST816-family labels differ between language
examples and the older MCU.js CST816S label; this implementation qualifies the
observed protocol/identity rather than asserting an exact silicon suffix.

The private driver uses I2C address `0x15`, identity register `0xA7 == 0xB5`,
finger count `0x02`, packed coordinates `0x03`–`0x06`, and the vendor sleep/IRQ
configuration. Register selection and reading are separated by a STOP, matching
the working raw-read path on the tested panel. The original repeated-start path
failed during report reads; STOP-separated native sampling and physical drawing
passed without retrying or hiding failures. Each bus operation has a timeout.
A failed read is not finger-up.
A count above one or out-of-range coordinates fails closed.

Touch exclusively owns I2C1 while started, including the shared IMU route.
Existing user I2C initialization—even on other pins—or claimed touch pins rejects
start. Public I2C cannot take over that lease. Stopping touch releases only its
own bus and pins; shared IMU arbitration is not implemented.

Portrait coordinates are 240×280. Landscape coordinates are 280×240:
`x = rawY`, `y = 239 - rawX`, derived from the configured ST7789 MADCTL mapping.
The panel's 20-pixel RAM window offset is not a public coordinate offset.
These mappings have native corner tests, not yet physical corner acceptance.

## Portable drawing example

`examples/portable/pointer-draw/draw.js` is the identical drawing consumer for
firmware and browser. It contains no pins, controller names or board branches.

- Firmware: copy `index.js` and `draw.js` together to an application directory;
  run its `index.js` through the existing module loader. It runs for 30 seconds.
- Browser: serve that example directory and open `browser.html`. The explicit
  mouse-only adapter converts axis-aligned CSS coordinates into bitmap
  coordinates, captures the primary mouse pointer, cancels on blur/lost capture,
  and tears down after 30 seconds. It is not a general Canvas/DOM emulator.

The browser page intentionally has no Canvas border/padding or CSS rotation;
those transforms are outside this adapter. Touch and pen browser input are
ignored in this first example. Releasing outside the Canvas ends the stroke.

## Verification and remaining acceptance

Host fixtures cover pointer fields, lifecycle and the shared consumer. The native
Jerry fixture executes the production Canvas, events and private I2C boundary
against SDK fakes: drag/release, rotation, read failure, presentation failure,
reset/reentrancy, ownership release and GC. Existing ST7789 and e-paper lifecycle
regressions also run. The fixture executes `js_canvas_reset`, not the complete
`js_engine_cleanup` path.

A separate Chromium run exercised real mouse input, visible drawing, release
outside the Canvas and the unmodified 30-second timeout with no uncaught browser
exceptions.

On the actual 1.69, firmware `f349bb4` completed 1,044 consecutive native samples
without an error after the transport fix. The same `draw.js` consumer then
recorded 12 pointer downs, 102 moves and 12 ups, with zero cancellations or errors
at the observed checkpoint. The operator confirmed visible drawing and said the
ink appeared under the finger. This is a basic portrait drawing/release/alignment
pass, not precision calibration or exhaustive controller-recovery qualification.

Each candidate flash was preceded by two matching full-chip reads. Candidate
readback matched the binary, and the filesystem/EEPROM tail remained unchanged
for the transport-fix iterations. The earlier filesystem relocation restored all
12 original application files with matching hashes. Squashing this work for
integration preserves the tested code; the integration commit has its own build
identity and does not retroactively change the hardware-tested firmware ID.

Remaining physical coverage: precise edge/corner calibration, landscape,
long-duration repeated-open/recovery testing, and other touch-capable boards.
These do not expand the accepted one-contact 1.69 portrait slice.
