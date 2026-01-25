// test-features.js - Advanced image features test
// Tests positioning, overlays, and graphics combinations

var image = require('image');

var WIDTH = 172, HEIGHT = 320;
var pins = { spiBus: 0, sck: 18, mosi: 19, cs: 17, dc: 16, rst: 20, bl: 21 };

function cmd(c) { GPIO.set(pins.dc, 0); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, c); GPIO.set(pins.cs, 1); }
function dat(d) { GPIO.set(pins.dc, 1); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, d); GPIO.set(pins.cs, 1); }

console.log('=== Image Features Test ===');

// Initialize GPIO and SPI
GPIO.init(pins.cs, GPIO.OUTPUT);
GPIO.init(pins.dc, GPIO.OUTPUT);
GPIO.init(pins.rst, GPIO.OUTPUT);
GPIO.init(pins.bl, GPIO.OUTPUT);
GPIO.set(pins.cs, 1);
GPIO.set(pins.bl, 0);

GPIO.set(pins.rst, 1); board.delay(100);
GPIO.set(pins.rst, 0); board.delay(100);
GPIO.set(pins.rst, 1); board.delay(150);

SPI.init(pins.spiBus, pins.sck, pins.mosi, 255, 40000000);

cmd(0x11); board.delay(120);
cmd(0x36); dat(0x00);
cmd(0x3A); dat(0x05);
cmd(0x21);
cmd(0x29); board.delay(50);

var handle = graphics.createBuffer({width: WIDTH, height: HEIGHT});

function flush() {
  var x0 = 0x22, x1 = WIDTH - 1 + 0x22;
  cmd(0x2A);
  dat(x0 >> 8); dat(x0 & 0xFF);
  dat(x1 >> 8); dat(x1 & 0xFF);
  cmd(0x2B);
  dat(0); dat(0);
  dat((HEIGHT - 1) >> 8); dat((HEIGHT - 1) & 0xFF);
  cmd(0x2C);
  GPIO.set(pins.dc, 1);
  GPIO.set(pins.cs, 0);
  SPI.writeBufferDMA(pins.spiBus, handle, WIDTH * HEIGHT * 2);
  GPIO.set(pins.cs, 1);
}

GPIO.set(pins.bl, 1);

// Test 1: Position icons in corners
console.log('Test 1: Icon positioning');
graphics.fill(handle, 0x001F); // Blue background

// Place icon in each corner (32x32 icon)
image.drawBMP(handle, '/icon_32x32.bmp', {x: 0, y: 0});           // Top-left
image.drawBMP(handle, '/icon_32x32.bmp', {x: WIDTH - 32, y: 0});  // Top-right
image.drawBMP(handle, '/icon_32x32.bmp', {x: 0, y: HEIGHT - 32}); // Bottom-left
image.drawBMP(handle, '/icon_32x32.bmp', {x: WIDTH - 32, y: HEIGHT - 32}); // Bottom-right
flush();
console.log('  4 icons in corners on blue background');
board.delay(2000);

// Test 2: Centered icon
console.log('Test 2: Centered icon');
graphics.fill(handle, 0xF800); // Red background
var cx = (WIDTH - 32) / 2;
var cy = (HEIGHT - 32) / 2;
image.drawBMP(handle, '/icon_32x32.bmp', {x: cx, y: cy});
flush();
console.log('  Icon centered on red background');
board.delay(2000);

// Test 3: Image with graphics overlay
console.log('Test 3: JPEG with graphics overlay');
image.drawJPEG(handle, '/test_gradient_simple.jpg', {x: 0, y: 0});

// Draw some shapes on top using fillRect
graphics.fillRect(handle, 10, 10, 50, 50, 0xFFFF);  // White rect
graphics.fillRect(handle, WIDTH - 60, 10, 50, 50, 0x0000);  // Black rect
// Draw a horizontal line using fillRect (1 pixel high)
graphics.fillRect(handle, 0, HEIGHT/2, WIDTH, 2, 0xFFE0);  // Yellow line
flush();
console.log('  Gradient JPEG with white/black rects and yellow line');
board.delay(2000);

// Test 4: Clipping test (image partially off-screen)
console.log('Test 4: Clipping test');
graphics.fill(handle, 0x07E0); // Green background

// Draw icon partially off each edge
image.drawBMP(handle, '/icon_32x32.bmp', {x: -16, y: 50});   // Left edge (half visible)
image.drawBMP(handle, '/icon_32x32.bmp', {x: WIDTH - 16, y: 100}); // Right edge
image.drawBMP(handle, '/icon_32x32.bmp', {x: 70, y: -16});   // Top edge
image.drawBMP(handle, '/icon_32x32.bmp', {x: 70, y: HEIGHT - 16}); // Bottom edge
flush();
console.log('  4 icons clipped at edges on green background');
board.delay(2000);

// Test 5: Multiple images layered
console.log('Test 5: Layered images');
image.drawJPEG(handle, '/test_gradient_simple.jpg', {x: 0, y: 0});
// Overlay multiple icons
for (var i = 0; i < 5; i++) {
  var ix = 20 + i * 30;
  var iy = 140 + (i % 2) * 20;
  image.drawBMP(handle, '/icon_32x32.bmp', {x: ix, y: iy});
}
flush();
console.log('  JPEG background with 5 icons overlaid');
board.delay(2000);

// Test 6: Full-screen BMP
console.log('Test 6: Full-screen 24-bit BMP');
var t0 = board.millis();
image.drawBMP(handle, '/test_172x320_24bit.bmp', {x: 0, y: 0});
var elapsed = board.millis() - t0;
flush();
console.log('  165KB BMP loaded in ' + elapsed + 'ms');
board.delay(2000);

// Final: Show gradient
console.log('');
console.log('=== All tests complete! ===');
image.drawJPEG(handle, '/test_gradient_simple.jpg', {x: 0, y: 0});
flush();
