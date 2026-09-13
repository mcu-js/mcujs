'use strict';
// Install as /app/index.js, with pointer-draw/draw.js beside it as /app/draw.js.
(function () {
if (typeof globalThis.appDrawStop === 'function') globalThis.appDrawStop();
var devices = require('mcujs:module').has('devices') ? require('devices') : {};
if (!devices.display) { console.log('No configured display is enabled in this firmware.'); return; }
var fs = require('fs');
var draw = require('./draw');
var settingsPath = '/app/settings.json';
var settings = { color: 'lime', lineWidth: 3, starts: 0 };
if (fs.existsSync(settingsPath)) {
  settings = JSON.parse(fs.readFileSync(settingsPath, 'utf8'));
}
if (!settings || ['white', 'lime', 'aqua', 'yellow'].indexOf(settings.color) < 0 ||
    typeof settings.lineWidth !== 'number' || settings.lineWidth < 1 || settings.lineWidth > 8 ||
    settings.lineWidth % 1 !== 0 || typeof settings.starts !== 'number' ||
    settings.starts < 0 || settings.starts >= 1000000 || settings.starts % 1 !== 0) {
  throw new Error('Invalid /app/settings.json; refusing to overwrite it.');
}
var display = devices.display.open();
var stopDraw, timer, done = false;
function failed(event) { console.error('Pointer input failed:', event.error); finish(); }
function finish() {
  if (done) return;
  done = true;
  if (timer !== undefined) clearTimeout(timer);
  display.canvas.removeEventListener('error', failed);
  try { if (stopDraw) stopDraw(); } finally { display.close(); }
  console.log('App drawing demo complete.');
}
globalThis.appDrawStop = finish;
try {
  if (!display.canvas.maxTouchPoints) throw new Error('This app needs pointer input.');
  // Small persistent configuration write; errors such as USB ownership remain visible.
  settings.starts++;
  fs.writeFileSync(settingsPath, JSON.stringify(settings));
  stopDraw = draw(display);
  var ctx = display.canvas.getContext('2d');
  ctx.fillStyle = settings.color;
  ctx.strokeStyle = settings.color;
  ctx.lineWidth = settings.lineWidth;
  ctx.fillRect(0, 0, display.canvas.width, 4); // Visible ready cue in the saved color.
  display.present();
  display.canvas.addEventListener('error', failed);
  display.startPointer();
  console.log('APP_READY ' + JSON.stringify(settings));
  timer = setTimeout(finish, 60000);
} catch (error) { finish(); throw error; }
}());
