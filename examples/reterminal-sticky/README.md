# The Night Ferry

An original procedural ink engraving for reTerminal Sticky's 800x480 screen.
A whale carries a glass observatory and sleeping cottages above the town; a
lantern keeper waits at its landing. **Someone keeps the stars lit.**

`night-ferry.js` uses only the supported Canvas 2D subset: paths, straight-line
approximations of curves, filled rectangles, fill/stroke styles and line width.
No image decoder, fonts, network, hardware pins, display commands or timers.
The composition fits uniformly; the intended target is 800x480 monochrome.
The little lettering is geometric artwork, not a new Canvas text API.

Load `night-ferry.js` into the root filesystem using the UART REPL's existing
`fs.writeFileSync` / `fs.appendFileSync`; read back before executing it. The
Sticky has a USB-UART bridge, **not** an MCUJS USB drive. For one-shot startup,
copy `index.js` to `/index.js` only after a successful manual render. Back up any
existing startup file. Remove/rename that file to disable startup rendering.

`ferryDone` records completion of the JS drawing function, not successful panel
presentation: also check `require('mcujs:canvas-native').stats()` for one
presentation and zero failures. The native adapter sleeps and powers off the
panel after refreshing; the ESP32 runtime stays awake. Battery life and physical
viewing/ghosting need separate testing. Do not interpret a software preview as
a photograph of the device.
