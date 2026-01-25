// test-blue.js - Solid blue JPEG test
var fs = require('fs');
var image = require('image');

var WIDTH = 172, HEIGHT = 320;
var pins = { spiBus: 0, sck: 18, mosi: 19, cs: 17, dc: 16, rst: 20, bl: 21 };

function cmd(c) { GPIO.set(pins.dc, 0); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, c); GPIO.set(pins.cs, 1); }
function dat(d) { GPIO.set(pins.dc, 1); GPIO.set(pins.cs, 0); SPI.transfer(pins.spiBus, d); GPIO.set(pins.cs, 1); }

console.log('Init...');
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
  var x0 = 0x22;
  var x1 = WIDTH - 1 + 0x22;
  var y0 = 0;
  var y1 = HEIGHT - 1;
  cmd(0x2A);
  dat(x0 >> 8); dat(x0 & 0xFF);
  dat(x1 >> 8); dat(x1 & 0xFF);
  cmd(0x2B);
  dat(y0 >> 8); dat(y0 & 0xFF);
  dat(y1 >> 8); dat(y1 & 0xFF);
  cmd(0x2C);
  GPIO.set(pins.dc, 1);
  GPIO.set(pins.cs, 0);
  SPI.writeBufferDMA(pins.spiBus, handle, WIDTH * HEIGHT * 2);
  GPIO.set(pins.cs, 1);
}

graphics.fill(handle, 0);
flush();
GPIO.set(pins.bl, 1);
console.log('Display ready');

console.log('Drawing JPEG from file...');
var t0 = board.millis();
image.drawJPEG(handle, '/test_gradient_simple.jpg', {x: 0, y: 0});
console.log('Decode: ' + (board.millis() - t0) + 'ms');

flush();
console.log('Should be red-blue gradient');
