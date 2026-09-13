// Bounded firmware setup. Application drawing is shared with browser.html.
(function () {
if (typeof globalThis.pointerDrawStop === 'function') globalThis.pointerDrawStop();
var devices = require('mcujs:module').has('devices') ? require('devices') : {};

if (!devices.display) {
  console.log('No configured display is enabled in this firmware.');
} else {
  var draw = require('./draw');
  var display = devices.display.open();
  var stop, timer, finished = false;
  function finish() {
    if (finished) return;
    finished = true;
    if (timer !== undefined) clearTimeout(timer);
    display.canvas.removeEventListener('error', finish);
    try { if (stop) stop(); } finally { display.close(); }
    console.log('Demo complete!');
  }
  globalThis.pointerDrawStop = finish;
  try {
    if (!display.canvas.maxTouchPoints) {
      console.log('This configured display has no pointer input.');
      finish();
    } else {
      stop = draw(display);
      display.canvas.addEventListener('error', finish);
      display.startPointer();
      console.log('Draw with one contact for 30 seconds.');
      timer = setTimeout(finish, 30000);
    }
  } catch (error) { finish(); throw error; }
}
}());
