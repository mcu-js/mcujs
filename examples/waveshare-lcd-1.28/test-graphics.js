// test-graphics.js - Phase 1 test for native graphics buffer
// Run on waveshare_rp2040_touch_lcd_1.28 board

console.log('=== Graphics Buffer Test ===');
console.log('Free memory before:', board.freeMemory());

// Test 1: Create buffer
console.log('\n1. Creating 240x240 buffer...');
var buf = graphics.createBuffer({ width: 240, height: 240 });
console.log('Buffer handle:', buf);

// Test 2: Get buffer info
var info = graphics.getBufferInfo(buf);
console.log('Buffer info:', JSON.stringify(info));
console.log('Expected byteLength: 115200, got:', info.byteLength);

// Test 3: Check memory usage
console.log('\nFree memory after create:', board.freeMemory());

// Test 4: color565 conversion
var red = graphics.color565(255, 0, 0);
var green = graphics.color565(0, 255, 0);
var blue = graphics.color565(0, 0, 255);
var white = graphics.color565(255, 255, 255);
console.log('\nColor conversions:');
console.log('Red (expect 63488/0xF800):', red);
console.log('Green (expect 2016/0x07E0):', green);
console.log('Blue (expect 31/0x001F):', blue);
console.log('White (expect 65535/0xFFFF):', white);

// Test 5: Fill buffer
console.log('\n5. Filling buffer with blue...');
graphics.fill(buf, blue);
console.log('Fill complete');

// Test 6: Set individual pixels
console.log('\n6. Setting pixels...');
graphics.setPixel(buf, 0, 0, red);      // top-left red
graphics.setPixel(buf, 239, 0, green);  // top-right green
graphics.setPixel(buf, 0, 239, white);  // bottom-left white
console.log('Pixels set');

// Test 7: Fill rectangle
console.log('\n7. Drawing rectangle...');
graphics.fillRect(buf, 100, 100, 50, 50, red);
console.log('Rectangle drawn');

// Test 8: Free buffer
console.log('\n8. Freeing buffer...');
graphics.freeBuffer(buf);
console.log('Buffer freed');

console.log('\nFree memory after free:', board.freeMemory());

console.log('\n=== Graphics Buffer Test COMPLETE ===');
