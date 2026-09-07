// No wiring: the onboard button controls the onboard LED for 60 seconds.
// Run manually with require('/lib/onboard-button'); do not install as /index.js.
(function () {
  var board = require('board');
  if (!board.devices.button || typeof board.buttonPressed !== 'function' ||
      typeof board.led !== 'function') {
    console.log('Onboard button-to-LED demo is not supported on this board.');
    return;
  }

  console.log('Press and release the onboard button to control the LED for 60 seconds.');
  console.log('Do not hold BOOT/BOOTSEL during reset or power-on.');
  board.led(board.buttonPressed());

  var candidate;
  var stable;
  var samples = 0;
  var interval = setInterval(function () {
    var pressed = board.buttonPressed();
    if (pressed !== candidate) {
      candidate = pressed;
      samples = 1;
    } else if (samples < 3) {
      samples++;
    }
    // Three matching 10ms samples suppress contact bounce on both edges.
    if (samples === 3 && stable !== candidate) {
      stable = candidate;
      board.led(stable);
      console.log(stable ? 'Button: pressed' : 'Button: released');
    }
  }, 10);

  setTimeout(function () {
    clearInterval(interval);
    board.led(false);
    console.log('Demo complete!');
  }, 60000);
}());
