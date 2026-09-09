# The Wandering Library

*For the nights when the story needs to come to you.*

A patient snail carries a little bookshop through the night. A reader in
nightclothes waits beneath its hanging lamp. Original procedural black-and-white
Canvas art, composed for 200×200 e-paper, with no fonts or loaded bitmaps.

Copy `wandering-library.js` to the MCUJS drive and eject it. On a fresh runtime:

```js
var display = require('canvas').display;
require('/wandering-library.js')(display.canvas, function () {
  console.log('Demo complete!');
});
```

All drawing runs in one JavaScript task. The display backend performs one full
refresh after that task, then puts the panel to sleep and disables its power.
There is no animation loop, autorun or application-side `show()` call.
Requires experimental Canvas firmware for `waveshare_esp32s3_epaper_1.54_v2`.
This is not a V1 firmware or a published release.

`preview.png` is a software reference, not a physical display photograph.
