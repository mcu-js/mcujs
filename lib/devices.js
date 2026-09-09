'use strict';

// Registry support, not physical inventory. No driver opens during discovery.
var definitions = require('board').capability('devices');
var devices = {};
if (definitions && definitions.display) {
  var display = {};
  Object.defineProperties(display, {
    capabilities: { enumerable: true, value: definitions.display },
    state: { enumerable: true, get: function () {
      return require('mcujs:canvas-native').defaultState();
    } },
    open: { enumerable: true, value: function () {
      if (arguments.length) throw new TypeError('Configured display.open takes no arguments');
      return require('canvas').connect('default');
    } }
  });
  devices.display = Object.freeze(display);
}
module.exports = Object.freeze(devices);
