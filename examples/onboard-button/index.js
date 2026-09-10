// Configured input only: no GPIO, controller or board-name branches.
var devices = require('devices');
var board = require('board');
if (!devices.button) {
  console.log('Onboard button is not supported in this firmware.');
} else {
  var button = devices.button.open();
  function showState() {
    if (board.devices.led) board.led(button.pressed);
    console.log('Button: ' + (button.pressed ? 'pressed' : 'released'));
  }
  button.addEventListener('press', showState);
  button.addEventListener('release', showState);
  button.addEventListener('error', function (event) { console.error('Button failed:', event.error); });
  showState(); // Initial observation, not a synthetic press/release event.
  setTimeout(function () {
    button.close();
    if (board.devices.led) board.led(false);
    console.log('Demo complete!');
  }, 60000);
}
