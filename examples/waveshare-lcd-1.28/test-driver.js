// test-driver.js - Test the gc9a01a driver with graphics buffer

console.log('=== GC9A01A Driver Test ===');

// Load driver (inline since we don't have require yet)
var DEFAULT_PINS = {
  spiBus: 1, sck: 10, mosi: 11, miso: 12,
  cs: 9, dc: 8, rst: 13, bl: 25
};

var spiBus = DEFAULT_PINS.spiBus;
var cs = DEFAULT_PINS.cs;
var dc = DEFAULT_PINS.dc;
var rst = DEFAULT_PINS.rst;
var bl = DEFAULT_PINS.bl;

// Initialize pins
GPIO.init(cs, GPIO.OUTPUT);
GPIO.init(dc, GPIO.OUTPUT);
GPIO.init(rst, GPIO.OUTPUT);
GPIO.init(bl, GPIO.OUTPUT);
GPIO.set(cs, 1);
GPIO.set(bl, 0);
GPIO.set(rst, 1);

// Initialize SPI
SPI.init(spiBus, DEFAULT_PINS.sck, DEFAULT_PINS.mosi, DEFAULT_PINS.miso, 40000000);

// Hardware reset - CS goes LOW after reset
GPIO.set(rst, 1);
board.delay(100);
GPIO.set(rst, 0);
board.delay(100);
GPIO.set(rst, 1);
GPIO.set(cs, 0);  // CS LOW - stays low
board.delay(100);

function cmd(c) {
  GPIO.set(dc, 0);
  SPI.transfer(spiBus, c);
}

function dat(d) {
  GPIO.set(dc, 1);
  SPI.transfer(spiBus, d);
}

// Full init sequence
console.log('Initializing display...');
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
GPIO.set(bl, 1);
console.log('Display initialized');

// Create a graphics buffer
console.log('Creating graphics buffer...');
var buf = graphics.createBuffer({ width: 240, height: 240 });
console.log('Buffer created:', buf);

// Fill with blue
console.log('Filling with blue...');
var blue = graphics.color565(0, 0, 255);
graphics.fill(buf, blue);

// Draw a red rectangle in center
console.log('Drawing red rectangle...');
var red = graphics.color565(255, 0, 0);
graphics.fillRect(buf, 70, 70, 100, 100, red);

// Draw a green rectangle
console.log('Drawing green rectangle...');
var green = graphics.color565(0, 255, 0);
graphics.fillRect(buf, 90, 90, 60, 60, green);

// Flush to display using DMA
console.log('Flushing to display via DMA...');
cmd(0x2A); dat(0x00); dat(0x00); dat(0x00); dat(0xEF);
cmd(0x2B); dat(0x00); dat(0x00); dat(0x00); dat(0xEF);
cmd(0x2C);
GPIO.set(dc, 1);
SPI.writeBufferDMA(spiBus, buf, 240 * 240 * 2);

console.log('=== Test Complete ===');
console.log('You should see: blue background, red square, green square in center');
