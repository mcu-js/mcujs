var devices = require('mcujs:module').has('devices') ? require('devices') : {};

if (!devices.display) {
  console.log('No configured display is enabled in this firmware.');
} else {
  var draw = require('./draw');
  var display = devices.display.open();
  try {
    draw(display);
    display.present();
  } finally {
    display.close();
  }
}
