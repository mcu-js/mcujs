// test-screen-api.js - Test the high-level screen API
// Demonstrates: circles, lines, rectangles, colors

console.log('=== Screen API Test ===');

// Load screen.js inline (since require not fully working)
// In production, you'd use: var screen = require('./screen').createScreen();

// For now, we paste the createScreen function...
// (In a real setup, this would be loaded from screen.js)

function createScreen(options) {
  options = options || {};
  var width = options.width || 240;
  var height = options.height || 240;
  
  var pins = {
    spiBus: 1, sck: 10, mosi: 11, miso: 12,
    cs: 9, dc: 8, rst: 13, bl: 25
  };
  
  var buffer = graphics.createBuffer({ width: width, height: height });
  
  function cmd(c) {
    GPIO.set(pins.dc, 0);
    SPI.transfer(pins.spiBus, c);
  }
  
  function dat(d) {
    GPIO.set(pins.dc, 1);
    SPI.transfer(pins.spiBus, d);
  }
  
  function init() {
    GPIO.init(pins.cs, GPIO.OUTPUT);
    GPIO.init(pins.dc, GPIO.OUTPUT);
    GPIO.init(pins.rst, GPIO.OUTPUT);
    GPIO.init(pins.bl, GPIO.OUTPUT);
    GPIO.set(pins.cs, 1);
    GPIO.set(pins.bl, 0);
    GPIO.set(pins.rst, 1);
    
    SPI.init(pins.spiBus, pins.sck, pins.mosi, pins.miso, 40000000);
    
    GPIO.set(pins.rst, 1);
    board.delay(100);
    GPIO.set(pins.rst, 0);
    board.delay(100);
    GPIO.set(pins.rst, 1);
    GPIO.set(pins.cs, 0);
    board.delay(100);
    
    // Init sequence
    cmd(0xEF); cmd(0xEB); dat(0x14); cmd(0xFE); cmd(0xEF);
    cmd(0xEB); dat(0x14); cmd(0x84); dat(0x40); cmd(0x85); dat(0xFF);
    cmd(0x86); dat(0xFF); cmd(0x87); dat(0xFF); cmd(0x88); dat(0x0A);
    cmd(0x89); dat(0x21); cmd(0x8A); dat(0x00); cmd(0x8B); dat(0x80);
    cmd(0x8C); dat(0x01); cmd(0x8D); dat(0x01); cmd(0x8E); dat(0xFF);
    cmd(0x8F); dat(0xFF); cmd(0xB6); dat(0x00); dat(0x20);
    cmd(0x36); dat(0x08); cmd(0x3A); dat(0x05);
    cmd(0x90); dat(0x08); dat(0x08); dat(0x08); dat(0x08);
    cmd(0xBD); dat(0x06); cmd(0xBC); dat(0x00);
    cmd(0xFF); dat(0x60); dat(0x01); dat(0x04);
    cmd(0xC3); dat(0x13); cmd(0xC4); dat(0x13); cmd(0xC9); dat(0x22);
    cmd(0xBE); dat(0x11); cmd(0xE1); dat(0x10); dat(0x0E);
    cmd(0xDF); dat(0x21); dat(0x0c); dat(0x02);
    cmd(0xF0); dat(0x45); dat(0x09); dat(0x08); dat(0x08); dat(0x26); dat(0x2A);
    cmd(0xF1); dat(0x43); dat(0x70); dat(0x72); dat(0x36); dat(0x37); dat(0x6F);
    cmd(0xF2); dat(0x45); dat(0x09); dat(0x08); dat(0x08); dat(0x26); dat(0x2A);
    cmd(0xF3); dat(0x43); dat(0x70); dat(0x72); dat(0x36); dat(0x37); dat(0x6F);
    cmd(0xED); dat(0x1B); dat(0x0B); cmd(0xAE); dat(0x77); cmd(0xCD); dat(0x63);
    cmd(0x70); dat(0x07); dat(0x07); dat(0x04); dat(0x0E); dat(0x0F);
              dat(0x09); dat(0x07); dat(0x08); dat(0x03);
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
    cmd(0x35); cmd(0x21); cmd(0x11);
    board.delay(120);
    cmd(0x29);
    board.delay(20);
    GPIO.set(pins.bl, 1);
    return screen;
  }
  
  function flush() {
    cmd(0x2A); dat(0x00); dat(0x00); dat(0x00); dat(width - 1);
    cmd(0x2B); dat(0x00); dat(0x00); dat(0x00); dat(height - 1);
    cmd(0x2C);
    GPIO.set(pins.dc, 1);
    SPI.writeBufferDMA(pins.spiBus, buffer, width * height * 2);
    return screen;
  }
  
  function fill(color) {
    graphics.fill(buffer, color);
    return screen;
  }
  
  function fillRect(x, y, w, h, color) {
    graphics.fillRect(buffer, x, y, w, h, color);
    return screen;
  }
  
  function drawHLine(x, y, w, color) {
    graphics.fillRect(buffer, x, y, w, 1, color);
    return screen;
  }
  
  function drawVLine(x, y, h, color) {
    graphics.fillRect(buffer, x, y, 1, h, color);
    return screen;
  }
  
  function drawRect(x, y, w, h, color) {
    drawHLine(x, y, w, color);
    drawHLine(x, y + h - 1, w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w - 1, y, h, color);
    return screen;
  }
  
  function drawLine(x0, y0, x1, y1, color) {
    var dx = Math.abs(x1 - x0);
    var dy = Math.abs(y1 - y0);
    var sx = x0 < x1 ? 1 : -1;
    var sy = y0 < y1 ? 1 : -1;
    var err = dx - dy;
    graphics.setPixel(buffer, x0, y0, color);
    while (x0 !== x1 || y0 !== y1) {
      var e2 = 2 * err;
      if (e2 > -dy) { err -= dy; x0 += sx; }
      if (e2 < dx) { err += dx; y0 += sy; }
      graphics.setPixel(buffer, x0, y0, color);
    }
    return screen;
  }
  
  function drawCircle(cx, cy, r, color) {
    var x = r, y = 0, err = 0;
    while (x >= y) {
      graphics.setPixel(buffer, cx + x, cy + y, color);
      graphics.setPixel(buffer, cx + y, cy + x, color);
      graphics.setPixel(buffer, cx - y, cy + x, color);
      graphics.setPixel(buffer, cx - x, cy + y, color);
      graphics.setPixel(buffer, cx - x, cy - y, color);
      graphics.setPixel(buffer, cx - y, cy - x, color);
      graphics.setPixel(buffer, cx + y, cy - x, color);
      graphics.setPixel(buffer, cx + x, cy - y, color);
      y++;
      if (err <= 0) err += 2 * y + 1;
      if (err > 0) { x--; err -= 2 * x + 1; }
    }
    return screen;
  }
  
  function fillCircle(cx, cy, r, color) {
    var x = r, y = 0, err = 0;
    while (x >= y) {
      drawHLine(cx - x, cy + y, 2 * x + 1, color);
      drawHLine(cx - x, cy - y, 2 * x + 1, color);
      drawHLine(cx - y, cy + x, 2 * y + 1, color);
      drawHLine(cx - y, cy - x, 2 * y + 1, color);
      y++;
      if (err <= 0) err += 2 * y + 1;
      if (err > 0) { x--; err -= 2 * x + 1; }
    }
    return screen;
  }
  
  function rgb(r, g, b) { return graphics.color565(r, g, b); }
  
  var screen = {
    width: width, height: height,
    init: init, flush: flush, fill: fill,
    fillRect: fillRect, drawRect: drawRect,
    drawLine: drawLine, drawHLine: drawHLine, drawVLine: drawVLine,
    drawCircle: drawCircle, fillCircle: fillCircle,
    rgb: rgb,
    BLACK: graphics.color565(0, 0, 0),
    WHITE: graphics.color565(255, 255, 255),
    RED: graphics.color565(255, 0, 0),
    GREEN: graphics.color565(0, 255, 0),
    BLUE: graphics.color565(0, 0, 255),
    CYAN: graphics.color565(0, 255, 255),
    MAGENTA: graphics.color565(255, 0, 255),
    YELLOW: graphics.color565(255, 255, 0),
    ORANGE: graphics.color565(255, 128, 0)
  };
  return screen;
}

// Create and initialize screen
var screen = createScreen();
screen.init();
console.log('Screen initialized');

// Draw demo graphics
console.log('Drawing demo...');

// Dark blue background
screen.fill(screen.rgb(0, 0, 40));

// Filled circles
screen.fillCircle(120, 120, 100, screen.BLUE);
screen.fillCircle(120, 120, 70, screen.CYAN);
screen.fillCircle(120, 120, 40, screen.WHITE);

// Circle outlines
screen.drawCircle(120, 120, 110, screen.YELLOW);

// Diagonal lines from corners
screen.drawLine(20, 20, 100, 100, screen.RED);
screen.drawLine(220, 20, 140, 100, screen.RED);
screen.drawLine(20, 220, 100, 140, screen.GREEN);
screen.drawLine(220, 220, 140, 140, screen.GREEN);

// Rectangle outlines
screen.drawRect(50, 50, 140, 140, screen.MAGENTA);

// Flush to display
screen.flush();

console.log('=== Demo Complete ===');
console.log('You should see concentric circles with lines and rectangles!');
