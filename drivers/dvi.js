/*
 * DVI Display Driver for mcujs Screen API
 * 
 * Bitbanged DVI output using PIO for RP2040-based boards with HDMI connectors.
 * Uses PicoDVI library for TMDS encoding and signal generation.
 * 
 * Supported boards:
 * - Waveshare RP2040-PiZero (16MB flash, HDMI output)
 * 
 * Usage:
 *   var screen = require('screen');
 *   var dvi = require('drivers/dvi');
 *   screen.init(dvi);
 *   screen.fill(screen.RED);
 *   screen.show();
 */

var driver = {
  /* Configuration - resolution from native DVI module
   * Default 160x120 is 4x scaled to 640x480 output
   */
  name: 'dvi',
  width: 160,
  height: 120,
  colorFormat: 'rgb565',
  byteOrder: 'native',  /* DVI uses native byte order */
  
  /* Internal state */
  _initialized: false,
  
  /*
   * Initialize the DVI output
   * Called by screen.init()
   */
  init: function(config) {
    /* Check if native DVI module is available */
    if (typeof DVI === 'undefined') {
      console.log('DVI: Native module not available');
      console.log('DVI: Build with MCUJS_HAS_DVI=1 for DVI support');
      this._initialized = false;
      return;
    }
    
    /* Get dimensions from native module */
    this.width = DVI.width;
    this.height = DVI.height;
    
    /* Override from config if provided (but native may limit) */
    if (config && config.width) this.width = Math.min(config.width, DVI.width);
    if (config && config.height) this.height = Math.min(config.height, DVI.height);
    
    /* Initialize native DVI
     * This sets up PIO, DMA, and prepares for output
     */
    try {
      DVI.init(this.width, this.height);
      DVI.start();  /* Launch core 1 for DVI signal generation */
      this._initialized = true;
      console.log('DVI: Initialized ' + this.width + 'x' + this.height + ' (640x480 output)');
    } catch (e) {
      console.log('DVI: Init failed - ' + e.message);
      this._initialized = false;
    }
  },
  
  /*
   * Flush the framebuffer to the DVI output
   * Called by screen.show()
   * 
   * @param {number} bufferPtr - Pointer to the framebuffer (native address)
   * @param {number} byteLength - Length of the buffer in bytes
   */
  show: function(bufferPtr, byteLength) {
    if (!this._initialized) {
      return;
    }
    
    /* Check if native DVI module is available */
    if (typeof DVI === 'undefined') {
      return;
    }
    
    /* Send framebuffer to DVI output */
    DVI.show(bufferPtr, byteLength);
  },
  
  /*
   * Stop DVI output
   */
  stop: function() {
    if (typeof DVI !== 'undefined' && DVI.stop) {
      DVI.stop();
    }
    this._initialized = false;
  }
};

module.exports = driver;
