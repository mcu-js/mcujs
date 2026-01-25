// test-step.js - Step by step test
console.log('Step 1: require image');
var image = require('image');
console.log('OK');

console.log('Step 2: create 240x240 buffer');
var handle = graphics.createBuffer({width: 240, height: 240});
console.log('OK - buffer handle: ' + handle);

console.log('Step 3: fill buffer');
graphics.fill(handle, 0x001F);
console.log('OK');

console.log('Step 4: load small BMP');
image.drawBMP(handle, '/icon_32x32.bmp', {x: 0, y: 0});
console.log('OK');

console.log('Step 5: load JPEG');
image.drawJPEG(handle, '/test_240x240.jpg', {x: 0, y: 0});
console.log('OK');

console.log('All steps passed!');
