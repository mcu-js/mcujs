// screen.js - High-level Screen API for mcujs
// Composes a graphics buffer with a display driver
// Provides a clean, chainable API for drawing
// For Waveshare RP2350-LCD-1.47-A (ST7789V3 172x320)

// Create a screen instance
// Options: { width, height, horizontal }
function createScreen(options) {
  options = options || {};
  
  // Default to horizontal orientation (320x172)
  var horizontal = options.horizontal !== undefined ? options.horizontal : true;
  var width = options.width || (horizontal ? 320 : 172);
  var height = options.height || (horizontal ? 172 : 320);
  
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
  
  // Window offsets for ST7789 (34 pixels offset in row direction)
  var colOffset = horizontal ? 0 : 0x22;
  var rowOffset = horizontal ? 0x22 : 0;
  
  // Create the framebuffer
  var buffer = graphics.createBuffer({ width: width, height: height });
  var initialized = false;
  
  // SPI command helper
  function cmd(c) {
    GPIO.set(pins.dc, 0);
    GPIO.set(pins.cs, 0);
    SPI.transfer(pins.spiBus, c);
    GPIO.set(pins.cs, 1);
  }
  
  // SPI data helper
  function dat(d) {
    GPIO.set(pins.dc, 1);
    GPIO.set(pins.cs, 0);
    SPI.transfer(pins.spiBus, d);
    GPIO.set(pins.cs, 1);
  }
  
  // Initialize the display
  function init() {
    if (initialized) return screen;
    
    // Initialize GPIO pins
    GPIO.init(pins.cs, GPIO.OUTPUT);
    GPIO.init(pins.dc, GPIO.OUTPUT);
    GPIO.init(pins.rst, GPIO.OUTPUT);
    GPIO.init(pins.bl, GPIO.OUTPUT);
    GPIO.set(pins.cs, 1);
    GPIO.set(pins.bl, 0);
    GPIO.set(pins.rst, 1);
    
    // Initialize SPI at 40MHz
    SPI.init(pins.spiBus, pins.sck, pins.mosi, pins.miso, 40000000);
    
    // Hardware reset
    GPIO.set(pins.rst, 1);
    board.delay(100);
    GPIO.set(pins.rst, 0);
    board.delay(100);
    GPIO.set(pins.rst, 1);
    board.delay(100);
    
    // Sleep out
    cmd(0x11);
    board.delay(120);
    
    // Memory access control - sets rotation and color order
    cmd(0x36);
    if (horizontal) {
      dat(0x70);  // Horizontal mode
    } else {
      dat(0x00);  // Vertical mode
    }
    
    // Pixel format: 16-bit RGB565
    cmd(0x3A); dat(0x05);
    
    // Porch control
    cmd(0xB2); dat(0x0C); dat(0x0C); dat(0x00); dat(0x33); dat(0x33);
    
    // Gate control
    cmd(0xB7); dat(0x35);
    
    // VCOM setting
    cmd(0xBB); dat(0x35);
    
    // LCM control
    cmd(0xC0); dat(0x2C);
    
    // VDV and VRH command enable
    cmd(0xC2); dat(0x01);
    
    // VRH set
    cmd(0xC3); dat(0x13);
    
    // VDV set
    cmd(0xC4); dat(0x20);
    
    // Frame rate control
    cmd(0xC6); dat(0x0F);
    
    // Power control 1
    cmd(0xD0); dat(0xA4); dat(0xA1);
    
    // D6 command
    cmd(0xD6); dat(0xA1);
    
    // Positive voltage gamma control
    cmd(0xE0);
    dat(0xF0); dat(0x00); dat(0x04); dat(0x04); dat(0x04); dat(0x05);
    dat(0x29); dat(0x33); dat(0x3E); dat(0x38); dat(0x12); dat(0x12);
    dat(0x28); dat(0x30);
    
    // Negative voltage gamma control
    cmd(0xE1);
    dat(0xF0); dat(0x07); dat(0x0A); dat(0x0D); dat(0x0B); dat(0x07);
    dat(0x28); dat(0x33); dat(0x3E); dat(0x36); dat(0x14); dat(0x14);
    dat(0x29); dat(0x32);
    
    // Display inversion on (required for correct colors on ST7789)
    cmd(0x21);
    
    // Sleep out
    cmd(0x11);
    board.delay(120);
    
    // Clear display RAM BEFORE Display ON to avoid garbage flash
    graphics.fill(buffer, 0x0000);
    setWindowAndWrite(0, 0, width, height);
    GPIO.set(pins.dc, 1);
    GPIO.set(pins.cs, 0);
    SPI.writeBufferDMA(pins.spiBus, buffer, width * height * 2);
    GPIO.set(pins.cs, 1);
    
    // NOW turn on display and backlight
    cmd(0x29);
    board.delay(20);
    GPIO.set(pins.bl, 1);
    
    initialized = true;
    return screen;
  }
  
  // Set window and prepare for memory write
  function setWindowAndWrite(x, y, w, h) {
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
  
  // Flush buffer to display via DMA
  function flush() {
    setWindowAndWrite(0, 0, width, height);
    GPIO.set(pins.dc, 1);
    GPIO.set(pins.cs, 0);
    SPI.writeBufferDMA(pins.spiBus, buffer, width * height * 2);
    GPIO.set(pins.cs, 1);
    return screen;
  }
  
  // Drawing methods (all chainable)
  function setPixel(x, y, color) {
    graphics.setPixel(buffer, x, y, color);
    return screen;
  }
  
  function fillRect(x, y, w, h, color) {
    graphics.fillRect(buffer, x, y, w, h, color);
    return screen;
  }
  
  function fill(color) {
    graphics.fill(buffer, color);
    return screen;
  }
  
  function clear() {
    graphics.fill(buffer, graphics.color565(0, 0, 0));
    return screen;
  }
  
  // Draw a horizontal line
  function drawHLine(x, y, w, color) {
    graphics.fillRect(buffer, x, y, w, 1, color);
    return screen;
  }
  
  // Draw a vertical line
  function drawVLine(x, y, h, color) {
    graphics.fillRect(buffer, x, y, 1, h, color);
    return screen;
  }
  
  // Draw a rectangle outline
  function drawRect(x, y, w, h, color) {
    drawHLine(x, y, w, color);
    drawHLine(x, y + h - 1, w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w - 1, y, h, color);
    return screen;
  }
  
  // Draw a line (Bresenham's algorithm)
  function drawLine(x0, y0, x1, y1, color) {
    var dx = Math.abs(x1 - x0);
    var dy = Math.abs(y1 - y0);
    var sx = x0 < x1 ? 1 : -1;
    var sy = y0 < y1 ? 1 : -1;
    var err = dx - dy;
    
    while (true) {
      graphics.setPixel(buffer, x0, y0, color);
      if (x0 === x1 && y0 === y1) break;
      var e2 = 2 * err;
      if (e2 > -dy) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y0 += sy;
      }
    }
    return screen;
  }
  
  // Draw a circle outline (Midpoint algorithm)
  function drawCircle(cx, cy, r, color) {
    var x = r;
    var y = 0;
    var err = 0;
    
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
      if (err <= 0) {
        err += 2 * y + 1;
      }
      if (err > 0) {
        x--;
        err -= 2 * x + 1;
      }
    }
    return screen;
  }
  
  // Fill a circle
  function fillCircle(cx, cy, r, color) {
    var x = r;
    var y = 0;
    var err = 0;
    
    while (x >= y) {
      // Draw horizontal lines for each y
      drawHLine(cx - x, cy + y, 2 * x + 1, color);
      drawHLine(cx - x, cy - y, 2 * x + 1, color);
      drawHLine(cx - y, cy + x, 2 * y + 1, color);
      drawHLine(cx - y, cy - x, 2 * y + 1, color);
      
      y++;
      if (err <= 0) {
        err += 2 * y + 1;
      }
      if (err > 0) {
        x--;
        err -= 2 * x + 1;
      }
    }
    return screen;
  }
  
  // Color helpers
  function rgb(r, g, b) {
    return graphics.color565(r, g, b);
  }
  
  // Backlight control
  function setBacklight(on) {
    GPIO.set(pins.bl, on ? 1 : 0);
    return screen;
  }
  
  // Cleanup
  function destroy() {
    graphics.freeBuffer(buffer);
    buffer = null;
  }
  
  // Build screen object
  var screen = {
    // Properties
    width: width,
    height: height,
    
    // Lifecycle
    init: init,
    flush: flush,
    destroy: destroy,
    
    // Drawing
    setPixel: setPixel,
    fill: fill,
    clear: clear,
    fillRect: fillRect,
    drawRect: drawRect,
    drawLine: drawLine,
    drawHLine: drawHLine,
    drawVLine: drawVLine,
    drawCircle: drawCircle,
    fillCircle: fillCircle,
    
    // Color
    rgb: rgb,
    color565: rgb,
    
    // Hardware
    setBacklight: setBacklight,
    
    // Pre-computed colors (already byte-swapped)
    BLACK:   graphics.color565(0, 0, 0),
    WHITE:   graphics.color565(255, 255, 255),
    RED:     graphics.color565(255, 0, 0),
    GREEN:   graphics.color565(0, 255, 0),
    BLUE:    graphics.color565(0, 0, 255),
    CYAN:    graphics.color565(0, 255, 255),
    MAGENTA: graphics.color565(255, 0, 255),
    YELLOW:  graphics.color565(255, 255, 0),
    ORANGE:  graphics.color565(255, 128, 0),
    GRAY:    graphics.color565(128, 128, 128)
  };
  
  return screen;
}

// Export for require()
if (typeof module !== 'undefined') {
  module.exports = { createScreen: createScreen };
}
