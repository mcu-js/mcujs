// gc9a01a.js - GC9A01A Display Driver for mcujs
// Pure JavaScript driver for the Waveshare RP2040 Touch LCD 1.28"
// 240x240 RGB565 round IPS display

// Default pin configuration for waveshare_rp2040_touch_lcd_1.28
// From Waveshare official DEV_Config.h
var DEFAULT_PINS = {
  spiBus: 1,
  sck: 10,
  mosi: 11,
  miso: 12,
  cs: 9,
  dc: 8,
  rst: 13,
  bl: 25
};

// Create a GC9A01A display driver
// Options: { spiBus, sck, mosi, miso, cs, dc, rst, bl, baudrate }
function createGC9A01ADriver(options) {
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
  
  var initialized = false;
  var width = 240;
  var height = 240;
  
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
  
  // Hardware reset - CS goes LOW after reset and stays LOW
  function reset() {
    GPIO.set(rst, 1);
    board.delay(100);
    GPIO.set(rst, 0);
    board.delay(100);
    GPIO.set(rst, 1);
    GPIO.set(cs, 0);   // CS LOW - stays low for all communication!
    board.delay(100);
  }
  
  // Send command byte (DC low) - CS stays LOW
  function command(cmd) {
    GPIO.set(dc, 0);   // Command mode
    SPI.transfer(spiBus, cmd);
  }
  
  // Send data byte (DC high) - CS stays LOW
  function dataByte(b) {
    GPIO.set(dc, 1);   // Data mode
    SPI.transfer(spiBus, b);
  }
  
  // Send data bytes (DC high) - CS stays LOW
  function data(bytes) {
    GPIO.set(dc, 1);   // Data mode
    if (Array.isArray(bytes)) {
      SPI.transfer(spiBus, bytes);
    } else {
      SPI.transfer(spiBus, bytes);
    }
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
  
  // GC9A01A initialization sequence
  // Based on manufacturer reference and Waveshare examples
  function init() {
    initPins();
    
    // Initialize SPI
    SPI.init(spiBus, sck, mosi, miso, baudrate);
    
    // Hardware reset
    reset();
    
    // Initialization sequence
    writeReg(0xEF);
    writeReg(0xEB, [0x14]);
    writeReg(0xFE);
    writeReg(0xEF);
    writeReg(0xEB, [0x14]);
    writeReg(0x84, [0x40]);
    writeReg(0x85, [0xFF]);
    writeReg(0x86, [0xFF]);
    writeReg(0x87, [0xFF]);
    writeReg(0x88, [0x0A]);
    writeReg(0x89, [0x21]);
    writeReg(0x8A, [0x00]);
    writeReg(0x8B, [0x80]);
    writeReg(0x8C, [0x01]);
    writeReg(0x8D, [0x01]);
    writeReg(0x8E, [0xFF]);
    writeReg(0x8F, [0xFF]);
    
    // Display function control
    writeReg(0xB6, [0x00, 0x00]);
    
    // Memory access control - sets rotation and color order
    // 0x48 = MY=0, MX=1, MV=0, ML=0, BGR=1, MH=0
    writeReg(0x36, [0x48]);
    
    // Pixel format: 16-bit RGB565
    writeReg(0x3A, [0x05]);
    
    writeReg(0x90, [0x08, 0x08, 0x08, 0x08]);
    writeReg(0xBD, [0x06]);
    writeReg(0xBC, [0x00]);
    writeReg(0xFF, [0x60, 0x01, 0x04]);
    writeReg(0xC3, [0x13]);
    writeReg(0xC4, [0x13]);
    writeReg(0xC9, [0x22]);
    writeReg(0xBE, [0x11]);
    writeReg(0xE1, [0x10, 0x0E]);
    writeReg(0xDF, [0x21, 0x0C, 0x02]);
    
    // Gamma settings
    writeReg(0xF0, [0x45, 0x09, 0x08, 0x08, 0x26, 0x2A]);
    writeReg(0xF1, [0x43, 0x70, 0x72, 0x36, 0x37, 0x6F]);
    writeReg(0xF2, [0x45, 0x09, 0x08, 0x08, 0x26, 0x2A]);
    writeReg(0xF3, [0x43, 0x70, 0x72, 0x36, 0x37, 0x6F]);
    
    writeReg(0xED, [0x1B, 0x0B]);
    writeReg(0xAE, [0x77]);
    writeReg(0xCD, [0x63]);
    writeReg(0x70, [0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03]);
    writeReg(0xE8, [0x34]);
    writeReg(0x62, [0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70]);
    writeReg(0x63, [0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70]);
    writeReg(0x64, [0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07]);
    writeReg(0x66, [0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00]);
    writeReg(0x67, [0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98]);
    writeReg(0x74, [0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00]);
    writeReg(0x98, [0x3E, 0x07]);
    
    // Tearing effect line on
    writeReg(0x35);
    
    // Display inversion on (required for correct colors on GC9A01A)
    writeReg(0x21);
    
    // Sleep out
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
    // Column address set
    writeReg(0x2A, [0x00, x0, 0x00, x1]);
    // Row address set
    writeReg(0x2B, [0x00, y0, 0x00, y1]);
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
  
  // Set backlight on/off
  function setBacklight(on) {
    GPIO.set(bl, on ? 1 : 0);
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
  createGC9A01ADriver: createGC9A01ADriver,
  DEFAULT_PINS: DEFAULT_PINS
};
