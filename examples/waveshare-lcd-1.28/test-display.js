// test-display.js - Full display test
// Tests: GC9A01A init, buffer creation, DMA flush
// Run on waveshare_rp2040_touch_lcd_1.28 board

console.log('=== Display Test ===');

// Load the driver
var gc9a01a = require('./gc9a01a');
console.log('Driver loaded');

// Create driver instance
var driver = gc9a01a.createGC9A01ADriver();
console.log('Driver created');

// Initialize display
console.log('Initializing display...');
driver.init();
console.log('Display initialized - backlight should be on!');

// Create framebuffer
console.log('Creating framebuffer...');
var buf = graphics.createBuffer({ width: 240, height: 240 });
console.log('Buffer created:', graphics.getBufferInfo(buf));

// Fill with blue
console.log('Filling with blue...');
var blue = graphics.color565(0, 100, 255);
graphics.fill(buf, blue);

// Flush to display
console.log('Flushing to display via DMA...');
driver.flush(buf, 240, 240);
console.log('Flush complete!');

// Wait a moment, then try other colors
console.log('Screen should be BLUE');

// Try red
console.log('Changing to RED...');
var red = graphics.color565(255, 0, 0);
graphics.fill(buf, red);
driver.flush(buf, 240, 240);
console.log('Screen should be RED');

// Try green
console.log('Changing to GREEN...');
var green = graphics.color565(0, 255, 0);
graphics.fill(buf, green);
driver.flush(buf, 240, 240);
console.log('Screen should be GREEN');

// Draw some rectangles
console.log('Drawing rectangles...');
graphics.fill(buf, graphics.color565(32, 32, 64));  // Dark blue bg
graphics.fillRect(buf, 20, 20, 80, 80, graphics.color565(255, 0, 0));     // Red
graphics.fillRect(buf, 80, 80, 80, 80, graphics.color565(0, 255, 0));     // Green  
graphics.fillRect(buf, 140, 140, 80, 80, graphics.color565(0, 0, 255));   // Blue
graphics.fillRect(buf, 50, 100, 140, 40, graphics.color565(255, 255, 0)); // Yellow stripe
driver.flush(buf, 240, 240);
console.log('Should see colored rectangles!');

// Cleanup
graphics.freeBuffer(buf);
console.log('Buffer freed');

console.log('=== Display Test COMPLETE ===');
