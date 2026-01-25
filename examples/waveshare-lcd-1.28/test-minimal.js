// test-minimal.js - Minimal image test
var image = require('image');
console.log('image module loaded');

// Just create a small buffer first
var handle = graphics.createBuffer({width: 100, height: 100});
console.log('small buffer created');

// Try loading small icon
console.log('loading icon...');
image.drawBMP(handle, '/icon_32x32.bmp', {x: 0, y: 0});
console.log('icon loaded OK');

graphics.freeBuffer(handle);
console.log('done');
