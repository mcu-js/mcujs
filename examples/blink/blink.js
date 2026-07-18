// Blink the onboard LED on any MCU.js board.
// Re-running this file replaces the previous blink interval.
if (globalThis.blinkTimer !== undefined) {
    clearInterval(globalThis.blinkTimer);
}

globalThis.ledOn = false;
board.led(globalThis.ledOn);

console.log("Blinking onboard LED every 500 ms.");
globalThis.blinkTimer = setInterval(() => {
    globalThis.ledOn = !globalThis.ledOn;
    board.led(globalThis.ledOn);
}, 500);
