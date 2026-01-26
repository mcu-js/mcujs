/*
 * DVI Display Driver for mcujs Screen API
 * 
 * Bitbanged DVI output using PIO for RP2040-based boards with HDMI connectors.
 * Uses PicoDVI library for TMDS encoding and signal generation.
 * 
 * NOTE: This is a placeholder - full DVI support requires native implementation.
 * See board/waveshare_rp2040_pizero/waveshare_rp2040_pizero_phases.md for roadmap.
 * 
 * Usage:
 *   var screen = require('screen');
 *   var dvi = require('drivers/dvi');
 *   screen.init(dvi);
 *   screen.fill(screen.RED);
 *   screen.show();
 */

var driver = {
  /* Default configuration */
  name: 'dvi',
  width: 160,   /* Conservative default - 160x120 = 38KB */
  height: 120,  /* Use screen.init(dvi, {width: 320, height: 240}) for larger */
  colorFormat: 'rgb565',
  byteOrder: 'native',  /* DVI uses native byte order */
  
  /* DVI pin configuration (RP2040-PiZero defaults) */
  pins: {
    d0_p: 12, d0_n: 13,   /* TMDS Data 0 (Blue + Sync) */
    d1_p: 14, d1_n: 15,   /* TMDS Data 1 (Green) */
    d2_p: 16, d2_n: 17,   /* TMDS Data 2 (Red) */
    clk_p: 18, clk_n: 19  /* TMDS Clock */
  },
  
  /* Internal state */
  _initialized: false,
  
  /*
   * Initialize the DVI output
   */
  init: function(config) {
    /* Merge config */
    if (config.width) this.width = config.width;
    if (config.height) this.height = config.height;
    if (config.pins) {
      for (var key in config.pins) {
        this.pins[key] = config.pins[key];
      }
    }
    
    /* Check if native DVI module is available */
    if (typeof DVI === 'undefined') {
      console.log('DVI: Native module not available (Phase 2 feature)');
      console.log('DVI: Using framebuffer-only mode for testing');
      this._initialized = true;
      return;
    }
    
    /* Initialize native DVI */
    DVI.init({
      width: this.width,
      height: this.height,
      pins: this.pins
    });
    
    this._initialized = true;
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
      /* Framebuffer-only mode - no output */
      return;
    }
    
    /* Send framebuffer to DVI output */
    DVI.flush(bufferPtr, byteLength);
  },
  
  /*
   * Deinitialize DVI output
   */
  deinit: function() {
    if (typeof DVI !== 'undefined' && DVI.deinit) {
      DVI.deinit();
    }
    this._initialized = false;
  }
};

module.exports = driver;
