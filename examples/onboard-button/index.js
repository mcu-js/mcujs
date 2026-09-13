// Configured input only; stop after 60 seconds or call onboardButtonStop().
(function () {
  if (typeof globalThis.onboardButtonStop === 'function') globalThis.onboardButtonStop();
  if (!require('mcujs:module').has('devices')) {
    console.log('Onboard button is not supported in this firmware.');
    return;
  }
  var devices = require('devices');
  var board = require('board');
  if (!devices.button) {
    console.log('Onboard button is not supported in this firmware.');
    return;
  }
  var button = devices.button.open(), timer, done = false;
  function showState() {
    if (board.devices.led) board.led(button.pressed);
    console.log('Button: ' + (button.pressed ? 'pressed' : 'released'));
  }
  function failed(event) { console.error('Button failed:', event.error); finish(); }
  function finish() {
    if (done) return;
    done = true;
    if (timer !== undefined) clearTimeout(timer);
    button.removeEventListener('press', showState);
    button.removeEventListener('release', showState);
    button.removeEventListener('error', failed);
    try { button.close(); } finally { if (board.devices.led) board.led(false); }
    console.log('Demo complete!');
  }
  globalThis.onboardButtonStop = finish;
  try {
    button.addEventListener('press', showState);
    button.addEventListener('release', showState);
    button.addEventListener('error', failed);
    showState(); // Initial observation, not a synthetic press/release event.
    timer = setTimeout(finish, 60000);
  } catch (error) { finish(); throw error; }
}());
