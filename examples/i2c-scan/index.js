// Portable I2C bus scanner for MCU.js.
// Uses the board-declared default route and probes readable standard addresses.
// A target that does not support a one-byte read may not appear in this scan.
(function () {
    var modules = require('mcujs:module');
    if (!modules.has('i2c')) {
        console.log('I2C is not compiled into this firmware');
        return;
    }

    var boardApi = require('board');
    var i2c = require('i2c');
    var capability = boardApi.capability('i2c');
    var bus = capability.defaultBus;
    var route = capability.defaultRoute;
    var frequency = 100000;

    if (frequency < capability.frequency.minHz ||
        frequency > capability.frequency.maxHz) {
        console.log('This board does not advertise 100 kHz I2C');
        return;
    }

    console.log('I2C Scanner Example');
    console.log('===================');
    console.log('Bus:', bus);
    console.log('SDA: GPIO', route.sda);
    console.log('SCL: GPIO', route.scl);
    console.log('Frequency:', frequency, 'Hz');

    // Omitting SDA/SCL selects capability.defaultRoute for the default bus.
    i2c.init({bus: bus, frequency: frequency});

    var devices = [];
    var address;
    for (address = 0x08; address <= 0x77; address++) {
        try {
            var result = i2c.read(bus, address, 1);
            if (result.length === 1) {
                devices.push(address);
                console.log('Found device at 0x' +
                            address.toString(16).toUpperCase());
            }
        } catch (error) {
            // ENXIO is the portable no-target/NACK result. Do not hide a busy
            // controller, wiring fault, timeout, or other native I/O failure.
            if (!error || error.code !== 'ENXIO') throw error;
        }
    }

    console.log('Scan complete!');
    console.log('Found', devices.length, 'device(s)');
    console.log('Connect SDA/SCL to the printed route with 3.3 V pull-ups');
    console.log('and connect every device to the same ground.');
    console.log('Demo complete!');
}());
