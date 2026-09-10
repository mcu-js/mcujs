// Bounded firmware setup. Application drawing is shared with browser.html.
var devices = require('devices');
var draw = require('./draw');

if (!devices.display) {
  console.log('No configured display is enabled in this firmware.');
} else {
  var display = devices.display.open();
  var stop, timer, finished = false;
  function finish() {
    if (finished) return;
    finished = true;
    if (timer !== undefined) clearTimeout(timer);
    try { if (stop) stop(); } finally { display.close(); }
    console.log('Demo complete!');
  }
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
