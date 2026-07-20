// Portable PWM LED fade for MCU.js 0.2.
// Uses an onboard GPIO LED only when that pin is advertised for PWM.
// The demo is safe to run again in the same REPL realm and stops after 15 seconds.

(function () {
    var previous = globalThis.pwmFadeState;
    if (previous !== undefined) {
        if (previous.interval !== undefined) clearInterval(previous.interval);
        if (previous.timeout !== undefined) clearTimeout(previous.timeout);
        previous.pwm.setDuty(previous.pin, previous.offDuty);
        previous.pwm.stop(previous.pin);
        globalThis.pwmFadeState = undefined;
    }

    var modules = require('mcujs:module');
    if (!modules.has('pwm')) {
        console.log('PWM is not compiled into this firmware');
        return;
    }

    var boardApi = require('board');
    var led = boardApi.devices.led;
    var capability = boardApi.capability('pwm');
    var pin = globalThis.pwmFadePin;
    var activeLow = globalThis.pwmFadeActiveLow === true;
    if (pin === undefined && led && led.type === 'gpio') {
        pin = led.pin;
        activeLow = led.activeLow === true;
    }
    if (capability.pins.indexOf(pin) < 0) {
        console.log('Select a PWM-capable pin with globalThis.pwmFadePin');
        return;
    }

    var pwm = require('pwm');
    var frequency = 1250;
    var stepCount = 64;
    var step = 0;
    var direction = 1;
    var offDuty = activeLow ? 1 : 0;
    var state = {
        pwm: pwm,
        pin: pin,
        offDuty: offDuty,
        interval: undefined,
        timeout: undefined,
    };

    pwm.init(pin, frequency);
    pwm.setDuty(pin, offDuty);
    globalThis.pwmFadeState = state;

    state.interval = setInterval(function () {
        step += direction;
        if (step >= stepCount) {
            step = stepCount;
            direction = -1;
        } else if (step <= 0) {
            step = 0;
            direction = 1;
        }

        var brightness = step / stepCount;
        pwm.setDuty(pin, activeLow ? 1 - brightness : brightness);
    }, 40);

    state.timeout = setTimeout(function () {
        if (globalThis.pwmFadeState !== state) return;
        clearInterval(state.interval);
        pwm.setDuty(pin, offDuty);
        pwm.stop(pin);
        globalThis.pwmFadeState = undefined;
        console.log('Demo complete!');
    }, 15000);

    console.log('PWM fade on GPIO', pin, 'at', frequency, 'Hz');
}());
