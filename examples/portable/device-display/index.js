var devices = require('devices');
var draw = require('./draw');

if (!devices.display) {
  console.log('No configured display is enabled in this firmware.');
} else {
  var display = devices.display.open();
  try {
    draw(display);
    display.present();
  } finally {
    display.close();
  }
}
