# A living bookshop

Copy `index.js`, `night-librarian.js`, and `wandering-library.js` into the root
of the MCUJS drive. Back up any existing `index.js` first; eject the drive.
Requires the V2 partial-refresh firmware **and** its private battery-power fix.
This opts into startup animation (unlike the original one-shot demo).

On boot, the library is drawn once. The first blink is scheduled after 4–8
seconds; subsequent idle pauses are normally 12–28 seconds, with a 20% chance
of an extra 10–25 seconds. These are timer delays, not exact observed eye
movements: the e-paper refresh itself takes time. Two blinks use four partial
updates, followed by a full cleaning refresh. Only one timer is outstanding.

The module uses ordinary Canvas calls. It does not control GPIO, SPI or panel
voltages. The board initializes its battery latch on GPIO17; GPIO42 disables
the unused audio rail. PWR is GPIO18: release after startup, then hold for about
two seconds to release battery power. Processing a display update can delay
button polling. USB still supplies power while connected. BOOT remains the
ROM recovery button and is not an application control.

Let startup finish before switching off: the runtime must pass its healthy-loop
window to clear the interrupted-start marker. USB is not needed to run the
script. A battery must be connected and charged; battery runtime, deep MCU
sleep and long-term continuous-refresh endurance are not qualified.

This also starts when USB-powered. At the console, `librarian.status()` reports
state and completed blinks; `librarian.stop()` cancels the timer without another
refresh. Rename or remove `/index.js` to disable subsequent autorun. No settings
or history are written by the blinking loop.
