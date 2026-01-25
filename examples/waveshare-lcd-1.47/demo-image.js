// demo-image.js - Image loading demo for Waveshare RP2350-LCD-1.47-A
// Demonstrates the image module for decoding JPEG and BMP files

var fs = require('fs');
var image = require('image');

// Display configuration (172x320 in vertical mode)
var WIDTH = 172;
var HEIGHT = 320;

// Pin configuration (Waveshare RP2350-LCD-1.47-A)
var pins = {
  spiBus: 0,
  sck: 18,
  mosi: 19,
  miso: 255,
  cs: 17,
  dc: 16,
  rst: 20,
  bl: 21
};

// Window offsets for ST7789
var colOffset = 0x22;
var rowOffset = 0;

// Create framebuffer
var buffer = graphics.createBuffer({ width: WIDTH, height: HEIGHT });

// SPI helpers
function cmd(c) {
  GPIO.set(pins.dc, 0);
  GPIO.set(pins.cs, 0);
  SPI.transfer(pins.spiBus, c);
  GPIO.set(pins.cs, 1);
}

function dat(d) {
  GPIO.set(pins.dc, 1);
  GPIO.set(pins.cs, 0);
  SPI.transfer(pins.spiBus, d);
  GPIO.set(pins.cs, 1);
}

// Initialize display
function initDisplay() {
  GPIO.init(pins.cs, GPIO.OUTPUT);
  GPIO.init(pins.dc, GPIO.OUTPUT);
  GPIO.init(pins.rst, GPIO.OUTPUT);
  GPIO.init(pins.bl, GPIO.OUTPUT);
  GPIO.set(pins.cs, 1);
  GPIO.set(pins.bl, 0);
  
  // Hardware reset
  GPIO.set(pins.rst, 1);
  board.delay(100);
  GPIO.set(pins.rst, 0);
  board.delay(100);
  GPIO.set(pins.rst, 1);
  board.delay(100);
  
  // Initialize SPI
  SPI.init(pins.spiBus, pins.sck, pins.mosi, pins.miso, 40000000);
  
  // Sleep out
  cmd(0x11);
  board.delay(120);
  
  // Memory access control - vertical mode
  cmd(0x36); dat(0x00);
  
  // Pixel format: 16-bit RGB565
  cmd(0x3A); dat(0x05);
  
  // Basic init sequence
  cmd(0xB2); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);
  cmd(0xB7); dat(0x35);
  cmd(0xBB); dat(0x35);
  cmd(0xC0); dat(0x2C);
  cmd(0xC2); dat(0x01);
  cmd(0xC3); dat(0x13);
  cmd(0xC4); dat(0x20);
  cmd(0xC6); dat(0x0F);
  cmd(0xD0); dat(0xA4); dat(0xA1);
  cmd(0xD6); dat(0xA1);
  
  // Display inversion on
  cmd(0x21);
  
  // Sleep out & display on
  cmd(0x11);
  board.delay(120);
  cmd(0x29);
  board.delay(20);
  
  // Backlight on
  GPIO.set(pins.bl, 1);
}

// Set display window
function setWindow(x, y, w, h) {
  var x0 = x + colOffset;
  var x1 = x + w - 1 + colOffset;
  var y0 = y + rowOffset;
  var y1 = y + h - 1 + rowOffset;
  
  cmd(0x2A);
  dat((x0 >> 8) & 0xFF); dat(x0 & 0xFF);
  dat((x1 >> 8) & 0xFF); dat(x1 & 0xFF);
  
  cmd(0x2B);
  dat((y0 >> 8) & 0xFF); dat(y0 & 0xFF);
  dat((y1 >> 8) & 0xFF); dat(y1 & 0xFF);
  
  cmd(0x2C);
}

// Flush buffer to display
function flush() {
  setWindow(0, 0, WIDTH, HEIGHT);
  GPIO.set(pins.dc, 1);
  GPIO.set(pins.cs, 0);
  SPI.writeBufferDMA(pins.spiBus, buffer, WIDTH * HEIGHT * 2);
  GPIO.set(pins.cs, 1);
}

// Main demo
console.log('Image Module Demo');
console.log('=================');

// Initialize display
console.log('Initializing display...');
initDisplay();

// Clear to black
graphics.fill(buffer, 0x0000);
flush();

// Try to load JPEG
console.log('Loading JPEG...');
try {
  var jpgData = fs.readFileSync('/test_172x320.jpg');
  console.log('JPEG file size: ' + jpgData.length + ' bytes');
  
  // Get image info
  var info = image.info(jpgData);
  if (info) {
    console.log('Image: ' + info.width + 'x' + info.height + ' ' + info.format);
  }
  
  // Decode JPEG to buffer
  var start = board.millis();
  image.decodeJPEG(buffer, jpgData, { x: 0, y: 0 });
  var elapsed = board.millis() - start;
  console.log('JPEG decode time: ' + elapsed + 'ms');
  
  // Show on display
  flush();
  console.log('JPEG displayed!');
  
  board.delay(3000);
} catch (e) {
  console.log('JPEG error: ' + e.message);
  console.log('Make sure /test_172x320.jpg exists on the filesystem');
}

// Try to load BMP
console.log('Loading BMP...');
try {
  var bmpData = fs.readFileSync('/test_172x320_24bit.bmp');
  console.log('BMP file size: ' + bmpData.length + ' bytes');
  
  // Get image info
  var info = image.info(bmpData);
  if (info) {
    console.log('Image: ' + info.width + 'x' + info.height + ' ' + info.format + ' ' + info.bpp + 'bpp');
  }
  
  // Decode BMP to buffer
  var start = board.millis();
  image.decodeBMP(buffer, bmpData, { x: 0, y: 0 });
  var elapsed = board.millis() - start;
  console.log('BMP decode time: ' + elapsed + 'ms');
  
  // Show on display
  flush();
  console.log('BMP displayed!');
  
  board.delay(3000);
} catch (e) {
  console.log('BMP error: ' + e.message);
  console.log('Make sure /test_172x320_24bit.bmp exists on the filesystem');
}

// Done
console.log('Demo complete!');
console.log('Copy test images to the device filesystem to see them.');
