/*
 * ST7789 Display Driver for mcujs Screen API
 * 
 * Common LCD controller used in many displays (240x240, 240x320, etc.)
 * 
 * Usage:
 *   var screen = require('screen');
 *   var st7789 = require('drivers/st7789');
 *   screen.init(st7789, { pins: { cs: 9, dc: 8, rst: 12, bl: 13 } });
 *   screen.fill(screen.RED);
 *   screen.show();
 */

var driver = {
  /* Default configuration */
  name: 'st7789',
  width: 240,
  height: 320,
  colorFormat: 'rgb565',
  byteOrder: 'swapped',  /* SPI displays need byte-swapped colors */
  
  /* SPI configuration */
  spi: 1,
  baudrate: 62500000,  /* 62.5 MHz */
  
  /* Pin configuration (override in screen.init) */
  pins: {
    cs: 9,    /* Chip select */
    dc: 8,    /* Data/Command */
    rst: 12,  /* Reset */
    bl: 13    /* Backlight */
  },
  
  /* Internal state */
  _initialized: false,
  _gpio: null,
  
  /*
   * Initialize the display
   */
  init: function(config) {
    var self = this;
    
    /* Merge config */
    if (config.width) this.width = config.width;
    if (config.height) this.height = config.height;
    if (config.spi !== undefined) this.spi = config.spi;
    if (config.baudrate) this.baudrate = config.baudrate;
    if (config.pins) {
      if (config.pins.cs !== undefined) this.pins.cs = config.pins.cs;
      if (config.pins.dc !== undefined) this.pins.dc = config.pins.dc;
      if (config.pins.rst !== undefined) this.pins.rst = config.pins.rst;
      if (config.pins.bl !== undefined) this.pins.bl = config.pins.bl;
    }
    
    /* Initialize GPIO pins */
    GPIO.init(this.pins.cs, 'out');
    GPIO.init(this.pins.dc, 'out');
    GPIO.init(this.pins.rst, 'out');
    GPIO.init(this.pins.bl, 'out');
    
    /* Initialize SPI */
    SPI.init(this.spi, {
      baudrate: this.baudrate,
      sck: this.spi === 0 ? 18 : 10,
      mosi: this.spi === 0 ? 19 : 11,
      miso: this.spi === 0 ? 16 : 12
    });
    
    /* Hardware reset */
    GPIO.set(this.pins.rst, 1);
    board.delay(50);
    GPIO.set(this.pins.rst, 0);
    board.delay(50);
    GPIO.set(this.pins.rst, 1);
    board.delay(150);
    
    /* Send init commands */
    this._initCommands();
    
    /* Turn on backlight */
    GPIO.set(this.pins.bl, 1);
    
    this._initialized = true;
  },
  
  /*
   * Send initialization commands to the display
   */
  _initCommands: function() {
    /* Software reset */
    this._writeCommand(0x01);
    board.delay(150);
    
    /* Sleep out */
    this._writeCommand(0x11);
    board.delay(120);
    
    /* Memory data access control */
    this._writeCommand(0x36);
    this._writeData([0x00]);  /* RGB order, top-to-bottom, left-to-right */
    
    /* Interface pixel format - 16bit/pixel */
    this._writeCommand(0x3A);
    this._writeData([0x55]);  /* RGB565 */
    
    /* Porch control */
    this._writeCommand(0xB2);
    this._writeData([0x0C, 0x0C, 0x00, 0x33, 0x33]);
    
    /* Gate control */
    this._writeCommand(0xB7);
    this._writeData([0x35]);
    
    /* VCOM setting */
    this._writeCommand(0xBB);
    this._writeData([0x1F]);
    
    /* LCM control */
    this._writeCommand(0xC0);
    this._writeData([0x2C]);
    
    /* VDV and VRH command enable */
    this._writeCommand(0xC2);
    this._writeData([0x01]);
    
    /* VRH set */
    this._writeCommand(0xC3);
    this._writeData([0x12]);
    
    /* VDV set */
    this._writeCommand(0xC4);
    this._writeData([0x20]);
    
    /* Frame rate control */
    this._writeCommand(0xC6);
    this._writeData([0x0F]);  /* 60Hz */
    
    /* Power control 1 */
    this._writeCommand(0xD0);
    this._writeData([0xA4, 0xA1]);
    
    /* Positive voltage gamma control */
    this._writeCommand(0xE0);
    this._writeData([0xD0, 0x08, 0x11, 0x08, 0x0C, 0x15, 0x39, 0x33, 0x50, 0x36, 0x13, 0x14, 0x29, 0x2D]);
    
    /* Negative voltage gamma control */
    this._writeCommand(0xE1);
    this._writeData([0xD0, 0x08, 0x10, 0x08, 0x06, 0x06, 0x39, 0x44, 0x51, 0x0B, 0x16, 0x14, 0x2F, 0x31]);
    
    /* Display inversion on (for proper colors on most ST7789 displays) */
    this._writeCommand(0x21);
    
    /* Display on */
    this._writeCommand(0x29);
    board.delay(100);
  },
  
  /*
   * Write a command to the display
   */
  _writeCommand: function(cmd) {
    GPIO.set(this.pins.cs, 0);
    GPIO.set(this.pins.dc, 0);  /* Command mode */
    SPI.write(this.spi, [cmd]);
    GPIO.set(this.pins.cs, 1);
  },
  
  /*
   * Write data to the display
   */
  _writeData: function(data) {
    GPIO.set(this.pins.cs, 0);
    GPIO.set(this.pins.dc, 1);  /* Data mode */
    SPI.write(this.spi, data);
    GPIO.set(this.pins.cs, 1);
  },
  
  /*
   * Set the drawing window
   */
  _setWindow: function(x0, y0, x1, y1) {
    /* Column address set */
    this._writeCommand(0x2A);
    this._writeData([x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF]);
    
    /* Row address set */
    this._writeCommand(0x2B);
    this._writeData([y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF]);
    
    /* Memory write */
    this._writeCommand(0x2C);
  },
  
  /*
   * Flush the framebuffer to the display
   * Called by screen.show()
   * 
   * @param {number} bufferPtr - Pointer to the framebuffer (native address)
   * @param {number} byteLength - Length of the buffer in bytes
   */
  show: function(bufferPtr, byteLength) {
    /* Set window to full screen */
    this._setWindow(0, 0, this.width - 1, this.height - 1);
    
    /* Send pixel data via DMA */
    GPIO.set(this.pins.cs, 0);
    GPIO.set(this.pins.dc, 1);  /* Data mode */
    SPI.writeBufferDMA(this.spi, bufferPtr, byteLength);
    GPIO.set(this.pins.cs, 1);
  },
  
  /*
   * Set backlight level (0-100)
   */
  setBacklight: function(level) {
    /* Simple on/off for now - could use PWM for dimming */
    GPIO.set(this.pins.bl, level > 0 ? 1 : 0);
  }
};

module.exports = driver;
