// test-image.js - Image module test for Waveshare RP2040-Touch-LCD-1.28
// 240x240 circular display with GC9A01A controller

var image = require('image');

var WIDTH = 240, HEIGHT = 240;

// Pin configuration for this board
var pins = {
  spiBus: 1,
  sck: 10,
  mosi: 11,
  miso: 12,
  cs: 9,
  dc: 8,
  rst: 13,
  bl: 25
};

function cmd(c) { GPIO.set(pins.dc, 0); SPI.transfer(pins.spiBus, c); }
function dat(d) { GPIO.set(pins.dc, 1); SPI.transfer(pins.spiBus, d); }

console.log('=== Image Test for 1.28" Touch LCD ===');
console.log('Display: 240x240 GC9A01A');

// Initialize GPIO
GPIO.init(pins.cs, GPIO.OUTPUT);
GPIO.init(pins.dc, GPIO.OUTPUT);
GPIO.init(pins.rst, GPIO.OUTPUT);
GPIO.init(pins.bl, GPIO.OUTPUT);
GPIO.set(pins.cs, 1);
GPIO.set(pins.bl, 0);

// Hardware reset
GPIO.set(pins.rst, 1); board.delay(100);
GPIO.set(pins.rst, 0); board.delay(100);
GPIO.set(pins.rst, 1); 
GPIO.set(pins.cs, 0);  // CS stays low
board.delay(100);

// Initialize SPI
SPI.init(pins.spiBus, pins.sck, pins.mosi, pins.miso, 40000000);

// GC9A01A init sequence
cmd(0xEF);
cmd(0xEB); dat(0x14);
cmd(0xFE);
cmd(0xEF);
cmd(0xEB); dat(0x14);
cmd(0x84); dat(0x40);
cmd(0x85); dat(0xFF);
cmd(0x86); dat(0xFF);
cmd(0x87); dat(0xFF);
cmd(0x88); dat(0x0A);
cmd(0x89); dat(0x21);
cmd(0x8A); dat(0x00);
cmd(0x8B); dat(0x80);
cmd(0x8C); dat(0x01);
cmd(0x8D); dat(0x01);
cmd(0x8E); dat(0xFF);
cmd(0x8F); dat(0xFF);
cmd(0xB6); dat(0x00); dat(0x20);
cmd(0x36); dat(0x08);
cmd(0x3A); dat(0x05);
cmd(0x90); dat(0x08); dat(0x08); dat(0x08); dat(0x08);
cmd(0xBD); dat(0x06);
cmd(0xBC); dat(0x00);
cmd(0xFF); dat(0x60); dat(0x01); dat(0x04);
cmd(0xC3); dat(0x13);
cmd(0xC4); dat(0x13);
cmd(0xC9); dat(0x22);
cmd(0xBE); dat(0x11);
cmd(0xE1); dat(0x10); dat(0x0E);
cmd(0xDF); dat(0x21); dat(0x0c); dat(0x02);
cmd(0xF0); dat(0x45); dat(0x09); dat(0x08); dat(0x08); dat(0x26); dat(0x2A);
cmd(0xF1); dat(0x43); dat(0x70); dat(0x72); dat(0x36); dat(0x37); dat(0x6F);
cmd(0xF2); dat(0x45); dat(0x09); dat(0x08); dat(0x08); dat(0x26); dat(0x2A);
cmd(0xF3); dat(0x43); dat(0x70); dat(0x72); dat(0x36); dat(0x37); dat(0x6F);
cmd(0xED); dat(0x1B); dat(0x0B);
cmd(0xAE); dat(0x77);
cmd(0xCD); dat(0x63);
cmd(0x70); dat(0x07); dat(0x07); dat(0x04); dat(0x0E); dat(0x0F); dat(0x09); dat(0x07); dat(0x08); dat(0x03);
cmd(0xE8); dat(0x34);
cmd(0x62); dat(0x18); dat(0x0D); dat(0x71); dat(0xED); dat(0x70); dat(0x70);
          dat(0x18); dat(0x0F); dat(0x71); dat(0xEF); dat(0x70); dat(0x70);
cmd(0x63); dat(0x18); dat(0x11); dat(0x71); dat(0xF1); dat(0x70); dat(0x70);
          dat(0x18); dat(0x13); dat(0x71); dat(0xF3); dat(0x70); dat(0x70);
cmd(0x64); dat(0x28); dat(0x29); dat(0xF1); dat(0x01); dat(0xF1); dat(0x00); dat(0x07);
cmd(0x66); dat(0x3C); dat(0x00); dat(0xCD); dat(0x67); dat(0x45); dat(0x45);
          dat(0x10); dat(0x00); dat(0x00); dat(0x00);
cmd(0x67); dat(0x00); dat(0x3C); dat(0x00); dat(0x00); dat(0x00); dat(0x01);
          dat(0x54); dat(0x10); dat(0x32); dat(0x98);
cmd(0x74); dat(0x10); dat(0x85); dat(0x80); dat(0x00); dat(0x00); dat(0x4E); dat(0x00);
cmd(0x98); dat(0x3e); dat(0x07);
cmd(0x35);
cmd(0x21);
cmd(0x11);
board.delay(120);
cmd(0x29);
board.delay(20);

// Create buffer and clear
var handle = graphics.createBuffer({width: WIDTH, height: HEIGHT});
graphics.fill(handle, 0x0000);

function flush() {
  cmd(0x2A); dat(0x00); dat(0x00); dat(0x00); dat(WIDTH - 1);
  cmd(0x2B); dat(0x00); dat(0x00); dat(0x00); dat(HEIGHT - 1);
  cmd(0x2C);
  GPIO.set(pins.dc, 1);
  SPI.writeBufferDMA(pins.spiBus, handle, WIDTH * HEIGHT * 2);
}

flush();
GPIO.set(pins.bl, 1);
console.log('Display initialized');
console.log('');

// Run tests
var tests = [
  { file: '/test_240x240.jpg', type: 'JPEG', desc: '240x240 full screen' },
  { file: '/icon_32x32.bmp', type: 'BMP', desc: '32x32 icon' },
  { file: '/test_gradient_simple.jpg', type: 'JPEG', desc: 'Gradient (172x320, clipped)' }
];

var passed = 0;
var failed = 0;

for (var i = 0; i < tests.length; i++) {
  var test = tests[i];
  console.log('Test ' + (i + 1) + '/' + tests.length + ': ' + test.file);
  console.log('  Type: ' + test.type + ' - ' + test.desc);
  
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

// Test positioning - icons in corners
console.log('Test: Icon positioning');
graphics.fill(handle, 0x001F); // Blue background
image.drawBMP(handle, '/icon_32x32.bmp', {x: 0, y: 0});
image.drawBMP(handle, '/icon_32x32.bmp', {x: WIDTH - 32, y: 0});
image.drawBMP(handle, '/icon_32x32.bmp', {x: 0, y: HEIGHT - 32});
image.drawBMP(handle, '/icon_32x32.bmp', {x: WIDTH - 32, y: HEIGHT - 32});
// Center icon
image.drawBMP(handle, '/icon_32x32.bmp', {x: (WIDTH - 32) / 2, y: (HEIGHT - 32) / 2});
flush();
console.log('  5 icons on blue background');
board.delay(2000);

// Final - show the 240x240 JPEG
console.log('');
console.log('=== Results ===');
console.log('Passed: ' + passed + '/' + tests.length);
console.log('Failed: ' + failed + '/' + tests.length);

image.drawJPEG(handle, '/test_240x240.jpg', {x: 0, y: 0});
flush();
console.log('Done!');
