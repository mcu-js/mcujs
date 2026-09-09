# Star Fisher

*A tiny machine, fishing for stars.*

An astronaut beside an alien lake, fishing for a fallen star beneath a
rose-coloured ringed planet. Drawn procedurally in JavaScript using the Canvas
2D subset: no loaded bitmap, fonts, transforms or controller calls in the art.

## Website assets

- `star-fisher.js`: exact source run on the RP2350 LCD.
- `preview.png`: browser-rendered reference, **not a photograph or LCD capture**.
  The board's RGB565 colour and antialiasing can differ from the browser preview.
- Suggested alt text: “An astronaut fishes for a glowing star beside a dark
  alien lake, with mountains, reflected planet light and a pink ringed planet.”

## Run on MCU.js

Requires the experimental display.canvas firmware. Copy `star-fisher.js` to the
board, then use a freshly opened display (or reuse an already-open one):

```js
var display = require('displays/st7789').connect();
require('/star-fisher.js')(display.canvas, function () {
  console.log('Demo complete!');
});
```

The verified board was Waveshare RP2350-LCD-1.47-A, landscape 320×172, firmware
`0.1.0+f717c17`. Rendering completed with four presentations and no reported
presentation failures. Native allocations were unchanged across the draw.
Three one-shot callbacks finish the scene; it is not a looping animation and
adds no autorun.

For a future browser embed, load the same JS and call
`starFisher(canvasElement)` on a canvas with width 320 and height 172.

This package is saved for website use; adding it to a homepage/showcase and
publishing the site are separate steps.
