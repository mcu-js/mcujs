// Standalone variant of index.js for loading directly from the REPL.
// Re-running this file replaces the previous timers. The demo stops after 30s.
(function () {
    if (globalThis.blinkInterval !== undefined) clearInterval(globalThis.blinkInterval);
    if (globalThis.blinkTimeout !== undefined) clearTimeout(globalThis.blinkTimeout);

    var boardApi = require('board');
    var led = boardApi.devices.led;
    if (!led) {
        console.log('This board has no onboard LED.');
        return;
    }

    var writeLed;
    if (led.type === 'gpio') {
        var gpio = require('gpio');
        gpio.init(led.pin, gpio.OUTPUT);
        writeLed = function (on) {
            gpio.set(led.pin, led.activeLow ? !on : on);
        };
    } else {
        writeLed = function (on) { boardApi.led(on); };
    }

    globalThis.blinkLedOn = false;
    writeLed(false);
    console.log('Blinking onboard LED every 500 ms.');
    globalThis.blinkInterval = setInterval(function () {
        globalThis.blinkLedOn = !globalThis.blinkLedOn;
        writeLed(globalThis.blinkLedOn);
    }, 500);
    globalThis.blinkTimeout = setTimeout(function () {
        clearInterval(globalThis.blinkInterval);
        globalThis.blinkInterval = undefined;
        globalThis.blinkTimeout = undefined;
        globalThis.blinkLedOn = false;
        writeLed(false);
        console.log('Demo complete!');
    }, 30000);
}());
