// Modules Example for mcujs
// Demonstrates the CommonJS require() system
//
// File structure on device:
//   /index.js      (this file)
//   /lib/math.js   (math utilities)
//   /lib/logger.js (logging helper)

// First, create the /lib directory and modules
// (In production, you'd copy these files separately)

// Create /lib directory if it doesn't exist
if (!fs.existsSync('/lib')) {
    fs.mkdirSync('/lib');
    console.log('Created /lib directory');
}

// Create math module
fs.writeFileSync('/lib/math.js', `
// Math utilities module
exports.add = (a, b) => a + b;
exports.subtract = (a, b) => a - b;
exports.multiply = (a, b) => a * b;
exports.divide = (a, b) => b !== 0 ? a / b : 0;
exports.PI = 3.14159;
exports.circleArea = (r) => exports.PI * r * r;
`);
console.log('Created /lib/math.js');

// Create logger module
fs.writeFileSync('/lib/logger.js', `
// Simple logger module
const startTime = board.millis();

function timestamp() {
    const elapsed = board.millis() - startTime;
    return '[' + elapsed + 'ms]';
}

exports.info = (msg) => console.log(timestamp(), 'INFO:', msg);
exports.warn = (msg) => console.warn(timestamp(), 'WARN:', msg);
exports.error = (msg) => console.error(timestamp(), 'ERROR:', msg);
`);
console.log('Created /lib/logger.js');

// Now use the modules!
const math = require('math');      // Loads /lib/math.js
const log = require('logger');     // Loads /lib/logger.js

log.info('Modules loaded successfully!');

// Use math module
const a = 10, b = 5;
log.info('Math operations:');
console.log(`  ${a} + ${b} = ${math.add(a, b)}`);
console.log(`  ${a} - ${b} = ${math.subtract(a, b)}`);
console.log(`  ${a} * ${b} = ${math.multiply(a, b)}`);
console.log(`  ${a} / ${b} = ${math.divide(a, b)}`);

log.info('Circle with radius 5:');
console.log(`  Area = ${math.circleArea(5)}`);

// Show module caching works
const math2 = require('math');
log.info('Module caching: math === math2 ? ' + (math === math2));

// Show require.cache
log.info('Cached modules: ' + Object.keys(require.cache).join(', '));

log.info('Example complete!');
