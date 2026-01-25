// test-all.js - Comprehensive image test for Waveshare RP2350-LCD-1.47-A
// Tests JPEG and BMP decoding with various images

var image = require('image');

var WIDTH = 172, HEIGHT = 320;
var pins = { spiBus: 0, sck: 18, mosi: 19, cs: 17, dc: 16, rst: 20, bl: 21 };

function cmd(c) { GPIO.set(pins.dc, 0); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, c); GPIO.set(pins.cs, 1); }
function dat(d) { GPIO.set(pins.dc, 1); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, d); GPIO.set(pins.cs, 1); }

console.log('=== Image Module Test Suite ===');
console.log('Display: 172x320 ST7789');
console.log('');

// Initialize GPIO and SPI
GPIO.init(pins.cs, GPIO.OUTPUT);
GPIO.init(pins.dc, GPIO.OUTPUT);
GPIO.init(pins.rst, GPIO.OUTPUT);
GPIO.init(pins.bl, GPIO.OUTPUT);
GPIO.set(pins.cs, 1);
GPIO.set(pins.bl, 0);

// Hardware reset
GPIO.set(pins.rst, 1); board.delay(100);
GPIO.set(pins.rst, 0); board.delay(100);
GPIO.set(pins.rst, 1); board.delay(150);

SPI.init(pins.spiBus, pins.sck, pins.mosi, 255, 40000000);

// Display init
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

// Clear and turn on backlight
graphics.fill(handle, 0x0000);
flush();
GPIO.set(pins.bl, 1);
console.log('Display initialized');
console.log('');

// Test images
var tests = [
  { file: '/test_gradient_simple.jpg', type: 'JPEG', desc: 'Red-blue gradient' },
  { file: '/test_172x320.jpg', type: 'JPEG', desc: 'Full screen JPEG' },
  { file: '/test_solid_blue.jpg', type: 'JPEG', desc: 'Solid blue' },
  { file: '/test_172x320_24bit.bmp', type: 'BMP', desc: '24-bit BMP' },
  { file: '/icon_32x32.bmp', type: 'BMP', desc: '32x32 icon' }
];

var passed = 0;
var failed = 0;

for (var i = 0; i < tests.length; i++) {
  var test = tests[i];
  console.log('Test ' + (i + 1) + '/' + tests.length + ': ' + test.file);
  console.log('  Type: ' + test.type + ' - ' + test.desc);
  
  // Clear buffer
  graphics.fill(handle, 0x0000);
  
  var t0 = board.millis();
  var success = false;
  
  try {
    if (test.type === 'JPEG') {
      image.drawJPEG(handle, test.file, {x: 0, y: 0});
    } else {
      image.drawBMP(handle, test.file, {x: 0, y: 0});
    }
    success = true;
  } catch (e) {
    console.log('  ERROR: ' + e.message);
  }
  
  var elapsed = board.millis() - t0;
  
  if (success) {
    console.log('  Decode: ' + elapsed + 'ms - OK');
    flush();
    passed++;
    board.delay(2000);
  } else {
    failed++;
  }
  console.log('');
}

// Summary
console.log('=== Results ===');
console.log('Passed: ' + passed + '/' + tests.length);
console.log('Failed: ' + failed + '/' + tests.length);

// Final display - show gradient
graphics.fill(handle, 0x0000);
image.drawJPEG(handle, '/test_gradient_simple.jpg', {x: 0, y: 0});
flush();
console.log('Done!');
