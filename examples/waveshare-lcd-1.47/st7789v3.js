// st7789v3.js - ST7789V3 Display Driver for mcujs
// Pure JavaScript driver for the Waveshare RP2350-LCD-1.47-A
// 172x320 RGB565 rectangular IPS display

// Default pin configuration for waveshare_rp2350_lcd_1.47_a
// From Waveshare official DEV_Config.h
var DEFAULT_PINS = {
  spiBus: 0,
  sck: 18,
  mosi: 19,
  miso: 255,  // Not used
  cs: 17,
  dc: 16,
  rst: 20,
  bl: 21
};

// Display dimensions
var LCD_WIDTH = 172;
var LCD_HEIGHT = 320;

// ST7789 window offset (display RAM starts at offset 0x22 for rows)
var COL_OFFSET = 0x22;  // 34 pixels column offset in horizontal mode
var ROW_OFFSET = 0x00;

// Create a ST7789V3 display driver
// Options: { spiBus, sck, mosi, miso, cs, dc, rst, bl, baudrate, horizontal }
function createST7789V3Driver(options) {
  options = options || {};
  
  var spiBus = options.spiBus !== undefined ? options.spiBus : DEFAULT_PINS.spiBus;
  var sck = options.sck !== undefined ? options.sck : DEFAULT_PINS.sck;
  var mosi = options.mosi !== undefined ? options.mosi : DEFAULT_PINS.mosi;
  var miso = options.miso !== undefined ? options.miso : DEFAULT_PINS.miso;
  var cs = options.cs !== undefined ? options.cs : DEFAULT_PINS.cs;
  var dc = options.dc !== undefined ? options.dc : DEFAULT_PINS.dc;
  var rst = options.rst !== undefined ? options.rst : DEFAULT_PINS.rst;
  var bl = options.bl !== undefined ? options.bl : DEFAULT_PINS.bl;
  var baudrate = options.baudrate || 40000000;  // 40 MHz default
  var horizontal = options.horizontal !== undefined ? options.horizontal : true;  // Default horizontal
  
  var initialized = false;
  var width, height;
  var colOffset, rowOffset;
  
  // Set dimensions based on orientation
  if (horizontal) {
    width = LCD_HEIGHT;   // 320
    height = LCD_WIDTH;   // 172
    colOffset = 0;
    rowOffset = COL_OFFSET;
  } else {
    width = LCD_WIDTH;    // 172
    height = LCD_HEIGHT;  // 320
    colOffset = COL_OFFSET;
    rowOffset = 0;
  }
  
  // Initialize GPIO pins
  function initPins() {
    GPIO.init(cs, GPIO.OUTPUT);
    GPIO.init(dc, GPIO.OUTPUT);
    GPIO.init(rst, GPIO.OUTPUT);
    GPIO.init(bl, GPIO.OUTPUT);
    GPIO.set(cs, 1);   // CS high (inactive)
    GPIO.set(bl, 0);   // Backlight off initially
    GPIO.set(rst, 1);  // Reset high (inactive)
  }
  
  // Hardware reset
  function reset() {
    GPIO.set(rst, 1);
    board.delay(100);
    GPIO.set(rst, 0);
    board.delay(100);
    GPIO.set(rst, 1);
    board.delay(100);
  }
  
  // Send command byte (DC low)
  function command(cmd) {
    GPIO.set(cs, 0);
    GPIO.set(dc, 0);   // Command mode
    SPI.transfer(spiBus, cmd);
    GPIO.set(cs, 1);
  }
  
  // Send data byte (DC high)
  function dataByte(b) {
    GPIO.set(cs, 0);
    GPIO.set(dc, 1);   // Data mode
    SPI.transfer(spiBus, b);
    GPIO.set(cs, 1);
  }
  
  // Send data bytes (DC high)
  function data(bytes) {
    GPIO.set(cs, 0);
    GPIO.set(dc, 1);   // Data mode
    if (Array.isArray(bytes)) {
      SPI.transfer(spiBus, bytes);
    } else {
      SPI.transfer(spiBus, bytes);
    }
    GPIO.set(cs, 1);
  }
  
  // Send command followed by data
  function writeReg(cmd, dataBytes) {
    command(cmd);
    if (dataBytes !== undefined && dataBytes !== null) {
      if (Array.isArray(dataBytes)) {
        for (var i = 0; i < dataBytes.length; i++) {
          dataByte(dataBytes[i]);
        }
      } else {
        dataByte(dataBytes);
      }
    }
  }
  
  // ST7789V3 initialization sequence
  // Based on Waveshare official C and Python examples
  function init() {
    initPins();
    
    // Initialize SPI
    SPI.init(spiBus, sck, mosi, miso, baudrate);
    
    // Hardware reset
    reset();
    
    // Sleep out
    writeReg(0x11);
    board.delay(120);
    
    // Memory access control - sets rotation and color order
    if (horizontal) {
      // Horizontal mode: 0x70 = MY=0, MX=1, MV=1, ML=1, BGR=0
      writeReg(0x36, [0x70]);
    } else {
      // Vertical mode: 0x00 = normal
      writeReg(0x36, [0x00]);
    }
    
    // Pixel format: 16-bit RGB565
    writeReg(0x3A, [0x05]);
    
    // Porch control
    writeReg(0xB2, [0x0C, 0x0C, 0x00, 0x33, 0x33]);
    
    // Gate control
    writeReg(0xB7, [0x35]);
    
    // VCOM setting
    writeReg(0xBB, [0x35]);
    
    // LCM control
    writeReg(0xC0, [0x2C]);
    
    // VDV and VRH command enable
    writeReg(0xC2, [0x01]);
    
    // VRH set
    writeReg(0xC3, [0x13]);
    
    // VDV set
    writeReg(0xC4, [0x20]);
    
    // Frame rate control
    writeReg(0xC6, [0x0F]);
    
    // Power control 1
    writeReg(0xD0, [0xA4, 0xA1]);
    
    // D6 command (undocumented, from Waveshare)
    writeReg(0xD6, [0xA1]);
    
    // Positive voltage gamma control
    writeReg(0xE0, [0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30]);
    
    // Negative voltage gamma control
    writeReg(0xE1, [0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32]);
    
    // Display inversion on (required for correct colors on ST7789)
    writeReg(0x21);
    
    // Sleep out (again, to be safe)
    writeReg(0x11);
    board.delay(120);
    
    // Display on
    writeReg(0x29);
    board.delay(20);
    
    // Turn on backlight
    GPIO.set(bl, 1);
    
    initialized = true;
  }
  
  // Set the drawing window
  function setWindow(x0, y0, x1, y1) {
    // Apply offsets based on orientation
    var xs = x0 + colOffset;
    var xe = x1 + colOffset;
    var ys = y0 + rowOffset;
    var ye = y1 + rowOffset;
    
    // Column address set
    writeReg(0x2A, [(xs >> 8) & 0xFF, xs & 0xFF, (xe >> 8) & 0xFF, xe & 0xFF]);
    // Row address set
    writeReg(0x2B, [(ys >> 8) & 0xFF, ys & 0xFF, (ye >> 8) & 0xFF, ye & 0xFF]);
    // Memory write command
    command(0x2C);
  }
  
  // Flush a graphics buffer to the display using DMA
  function flush(bufferHandle, bufWidth, bufHeight) {
    if (!initialized) {
      throw new Error('Display not initialized');
    }
    
    var w = bufWidth || width;
    var h = bufHeight || height;
    
    // Set window to full screen (or buffer size)
    setWindow(0, 0, w - 1, h - 1);
    
    // Send pixel data via DMA
    GPIO.set(dc, 1);   // Data mode
    GPIO.set(cs, 0);   // Select
    SPI.writeBufferDMA(spiBus, bufferHandle, w * h * 2);
    GPIO.set(cs, 1);   // Deselect
  }
  
  // Set backlight on/off or PWM duty (0-100)
  function setBacklight(value) {
    if (typeof value === 'boolean') {
      GPIO.set(bl, value ? 1 : 0);
    } else {
      // PWM control if available
      GPIO.set(bl, value > 0 ? 1 : 0);
    }
  }
  
  // Return public interface
  return {
    init: init,
    command: command,
    data: data,
    writeReg: writeReg,
    setWindow: setWindow,
    flush: flush,
    setBacklight: setBacklight,
    width: width,
    height: height,
    reset: reset
  };
}

module.exports = {
  createST7789V3Driver: createST7789V3Driver,
  DEFAULT_PINS: DEFAULT_PINS,
  LCD_WIDTH: LCD_WIDTH,
  LCD_HEIGHT: LCD_HEIGHT
};
