'use strict';
// Install as /app/index.js, with pointer-draw/draw.js beside it as /app/draw.js.
var fs = require('fs');
var devices = require('devices');
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
if (!devices.display) throw new Error('This app needs a configured Canvas display.');
var display = devices.display.open();
var stopDraw, timer, done = false;
function finish() {
  if (done) return;
  done = true;
  if (timer !== undefined) clearTimeout(timer);
  try { if (stopDraw) stopDraw(); } finally { display.close(); }
  console.log('App drawing demo complete. Reset to run again.');
}
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
  display.canvas.addEventListener('error', function (event) {
    console.error('Pointer input failed:', event.error);
    finish();
  });
  display.startPointer();
  console.log('APP_READY ' + JSON.stringify(settings));
  timer = setTimeout(finish, 60000);
} catch (error) { finish(); throw error; }
