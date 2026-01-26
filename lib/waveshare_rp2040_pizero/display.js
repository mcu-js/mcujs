/*
 * Waveshare RP2040-PiZero Default Display Driver
 * 
 * Re-exports the DVI driver as the default display for this board.
 * 
 * Usage:
 *   var screen = require('screen');
 *   var display = require('lib/waveshare_rp2040_pizero/display');
 *   screen.init(display);
 */

var dvi = require('drivers/dvi');

module.exports = dvi;
