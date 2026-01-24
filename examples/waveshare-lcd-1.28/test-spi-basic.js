// test-spi-basic.js - Test SPI with Waveshare's exact init sequence
// Key insight: CS stays LOW during entire communication (not toggled per-command)

console.log('=== SPI Basic Test (Waveshare-style) ===');

// Pin definitions (from Waveshare official DEV_Config.h)
var CS = 9;
var DC = 8;
var RST = 13;   // Was 12 - WRONG! Official is 13
var BL = 25;
var SCK = 10;
var MOSI = 11;
var MISO = 12;  // Was 2 - official is 12

// Initialize pins
console.log('Initializing pins...');
GPIO.init(CS, GPIO.OUTPUT);
GPIO.init(DC, GPIO.OUTPUT);
GPIO.init(RST, GPIO.OUTPUT);
GPIO.init(BL, GPIO.OUTPUT);

GPIO.set(CS, 1);   // Deselect initially
GPIO.set(BL, 0);   // Backlight off
GPIO.set(RST, 1);  // Reset inactive

// Initialize SPI
console.log('Initializing SPI1...');
SPI.init(1, SCK, MOSI, MISO, 40000000);  // 40 MHz
console.log('SPI initialized');

// Hardware reset - IMPORTANT: CS goes LOW after reset and STAYS LOW
console.log('Hardware reset...');
GPIO.set(RST, 1);
board.delay(100);
GPIO.set(RST, 0);
board.delay(100);
GPIO.set(RST, 1);
GPIO.set(CS, 0);  // CS LOW - stays low for all communication!
board.delay(100);
console.log('Reset complete, CS is LOW');

// Helper: send command (CS stays LOW)
function cmd(c) {
  GPIO.set(DC, 0);  // Command mode
  SPI.transfer(1, c);
}

// Helper: send data byte (CS stays LOW)
function dat(d) {
  GPIO.set(DC, 1);  // Data mode
  SPI.transfer(1, d);
}

// Helper: send data array
function datArr(arr) {
  GPIO.set(DC, 1);  // Data mode
  SPI.transfer(1, arr);
}

// Full Waveshare init sequence
console.log('Sending full init sequence...');

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
cmd(0x36); dat(0x08);  // Vertical screen
cmd(0x3A); dat(0x05);  // 16-bit color

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

cmd(0x35);  // Tearing effect line ON
cmd(0x21);  // Display inversion ON

cmd(0x11);  // Sleep out
console.log('Sleep out, waiting 120ms...');
board.delay(120);

cmd(0x29);  // Display ON
console.log('Display ON, waiting 20ms...');
board.delay(20);

// Turn on backlight
GPIO.set(BL, 1);
console.log('Backlight ON');

// Fill screen with RED
console.log('Filling screen with RED...');

// Set window (full screen)
cmd(0x2A);  // Column address
dat(0x00); dat(0x00);  // Start = 0
dat(0x00); dat(0xEF);  // End = 239

cmd(0x2B);  // Row address
dat(0x00); dat(0x00);  // Start = 0
dat(0x00); dat(0xEF);  // End = 239

cmd(0x2C);  // Memory write

// Send RED pixels (RGB565: 0xF800)
// High byte = 0xF8, Low byte = 0x00
GPIO.set(DC, 1);  // Data mode

// Build a chunk of red pixels
var chunk = [];
for (var i = 0; i < 120; i++) {  // 60 pixels = 120 bytes
  chunk.push(0xF8);
  chunk.push(0x00);
}

// 240x240 = 57600 pixels, 60 pixels per chunk = 960 chunks
console.log('Sending pixels...');
for (var c = 0; c < 960; c++) {
  SPI.transfer(1, chunk);
}

console.log('=== Test Complete ===');
console.log('Display should show RED!');
