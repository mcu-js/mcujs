// Copy this file and config.json together, e.g. /app/config/.
// Read-only lesson: never creates or overwrites application settings.
const config = require('./config.json');
console.log('App:', config.appName, 'v' + config.version);
console.log('Debug mode:', config.debug);
console.log('Features:', config.features.join(', '));
console.log('Demo complete!');
