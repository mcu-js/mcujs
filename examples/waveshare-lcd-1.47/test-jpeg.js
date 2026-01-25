// test-jpeg.js - Simple JPEG display test
var fs = require('fs');
var image = require('image');

var WIDTH = 172, HEIGHT = 320;
var pins = { spiBus: 0, sck: 18, mosi: 19, cs: 17, dc: 16, rst: 20, bl: 21 };

function cmd(c) { GPIO.set(pins.dc, 0); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, c); GPIO.set(pins.cs, 1); }
function dat(d) { GPIO.set(pins.dc, 1); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, d); GPIO.set(pins.cs, 1); }

console.log('Initializing GPIO...');
// Init GPIO and SPI
GPIO.init(pins.cs, GPIO.OUTPUT);
GPIO.init(pins.dc, GPIO.OUTPUT);
GPIO.init(pins.rst, GPIO.OUTPUT);
GPIO.init(pins.bl, GPIO.OUTPUT);
GPIO.set(pins.cs, 1);
GPIO.set(pins.bl, 0);

console.log('Hardware reset...');
// Hardware reset
GPIO.set(pins.rst, 1); board.delay(100);
GPIO.set(pins.rst, 0); board.delay(100);
GPIO.set(pins.rst, 1); board.delay(150);

console.log('Init SPI...');
SPI.init(pins.spiBus, pins.sck, pins.mosi, 255, 40000000);

console.log('Init ST7789...');
// ST7789 init
cmd(0x11); board.delay(120);
cmd(0x36); dat(0x00);
cmd(0x3A); dat(0x05);
cmd(0xB2); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);
cmd(0xB7); dat(0x35);
cmd(0xBB); dat(0x35);
cmd(0xC0); dat(0x2C);
cmd(0xC2); dat(0x01);
cmd(0xC3); dat(0x13);
cmd(0xC4); dat(0x20);
cmd(0xC6); dat(0x0F);
cmd(0xD0); dat(0xA4); dat(0xA1);
cmd(0x21);
cmd(0x11); board.delay(120);
cmd(0x29); board.delay(50);

console.log('Creating buffer...');
// Create buffer
var handle = graphics.createBuffer({width: WIDTH, height: HEIGHT});
console.log('Buffer handle: ' + handle);
graphics.fill(handle, 0);

// Flush function
function flush() {
  var colOffset = 0x22;
  var rowOffset = 0;
  var x0 = 0 + colOffset;
  var x1 = WIDTH - 1 + colOffset;
  var y0 = 0 + rowOffset;
  var y1 = HEIGHT - 1 + rowOffset;
  
  cmd(0x2A);
  dat((x0 >> 8) & 0xFF); dat(x0 & 0xFF);
  dat((x1 >> 8) & 0xFF); dat(x1 & 0xFF);
  
  cmd(0x2B);
  dat((y0 >> 8) & 0xFF); dat(y0 & 0xFF);
  dat((y1 >> 8) & 0xFF); dat(y1 & 0xFF);
  
  cmd(0x2C);
  GPIO.set(pins.dc, 1);
  GPIO.set(pins.cs, 0);
  SPI.writeBufferDMA(pins.spiBus, handle, WIDTH * HEIGHT * 2);
  GPIO.set(pins.cs, 1);
}

console.log('Clearing screen...');
// Clear screen first
flush();
board.delay(50);
GPIO.set(pins.bl, 1);
console.log('Display ready');

console.log('Loading JPEG...');
// Load and decode JPEG
var jpg = fs.readFileSync('/test_172x320.jpg');
console.log('JPEG size: ' + jpg.length + ' bytes');

console.log('Decoding...');
var t0 = board.millis();
image.decodeJPEG(handle, jpg, {x: 0, y: 0});
var decodeTime = board.millis() - t0;
console.log('Decode: ' + decodeTime + 'ms');

console.log('Flushing to display...');
board.delay(20);
flush();
console.log('Done!');
