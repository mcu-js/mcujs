'use strict';

// Native open validates the SPI display options and owns the hardware backend.
var canvas = require('canvas');

module.exports = {
  connect: function (options) { return canvas.connect('st7789', options); }
};
