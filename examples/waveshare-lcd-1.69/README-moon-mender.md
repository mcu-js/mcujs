# The Moon Mender

*Even the moon needs someone to stay up for it.*

A fox in a turquoise scarf climbs a rooftop ladder to stitch a broken moon
with golden thread. Below, a spool rests beside a warm attic window.

![A small fox on a ladder sews a cracked golden moon above blue rooftops.](moon-mender.png)

This is a **software-rendered reference**, not a photograph of the LCD.
RGB565 colour and antialiasing on the device can differ.

## Run on the 1.69-inch board

Requires experimental Canvas firmware for `waveshare_rp2350_touch_lcd_1.69`.
The onboard profile defaults to a 240×280 portrait canvas. Touch, buzzer and
sensors are not used by this drawing.

Copy `moon-mender.js` to the MCUJS USB drive and eject it to return filesystem
ownership to the board. On a fresh runtime:

```js
var display = require('canvas').display;
require('/moon-mender.js')(display.canvas, function () {
  console.log('Demo complete!');
});
```

Reuse the existing display if one is already open. The illustration uses only
Canvas subset drawing calls. Panel wiring, initialization and presentation stay
inside the firmware. A single uniform scale preserves proportions on other
canvas sizes and centres any margins.

Three one-shot callbacks build the scene and then finish, leaving the image on
the panel. No animation loop, external images, fonts or autorun are required.
