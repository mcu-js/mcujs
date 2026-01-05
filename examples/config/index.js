// JSON Config Example for mcujs
// Demonstrates loading configuration from JSON files
//
// File structure on device:
//   /index.js     (this file)
//   /config.json  (application settings)

// Create a config file
fs.writeFileSync('/config.json', JSON.stringify({
    appName: "My mcujs App",
    version: "1.0.0",
    led: {
        pin: 25,
        blinkRate: 500
    },
    debug: true,
    features: ["blink", "log", "status"]
}, null, 2));

console.log('Created /config.json');

// Load config using require
const config = require('/config.json');

console.log('');
console.log('=== Application Config ===');
console.log('App:', config.appName, 'v' + config.version);
console.log('Debug mode:', config.debug);
console.log('Features:', config.features.join(', '));
console.log('LED pin:', config.led.pin);
console.log('Blink rate:', config.led.blinkRate + 'ms');
console.log('');

// Use config values
GPIO.init(config.led.pin, GPIO.OUTPUT);

if (config.debug) {
    console.log('[DEBUG] GPIO initialized');
}

let ledState = false;
setInterval(() => {
    ledState = !ledState;
    GPIO.set(config.led.pin, ledState);
    
    if (config.debug) {
        console.log('[DEBUG] LED:', ledState ? 'ON' : 'OFF');
    }
}, config.led.blinkRate);

console.log(config.appName, 'running!');
