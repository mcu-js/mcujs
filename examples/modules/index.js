// Copy index.js and math.js together, e.g. /app/modules/.
// Relative CommonJS imports do not create or overwrite files.
const math = require('./math');
console.log('10 + 5 =', math.add(10, 5));
console.log('10 * 5 =', math.multiply(10, 5));
const again = require('./math');
console.log('Module caching:', math === again);
console.log('Demo complete!');
