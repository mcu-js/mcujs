// A static Canvas drawing for 20 seconds, then release the display.
// Uses the configured display dimensions; no pins or controller setup.
// Run this file again after "Demo complete!" (no reset is needed).
(function () {
  var devices = require('devices');
  if (!devices.display) throw new Error('No configured display');
  var display = devices.display.open();
  try {
    var canvas = display.canvas;
    var ctx = canvas.getContext('2d'), w = canvas.width, h = canvas.height;
    ctx.fillStyle = '#08182e';
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = '#00eeee';
    ctx.lineWidth = 2;
    ctx.strokeRect(4, 4, w - 8, h - 8);
    var colors = ['#ff3040', '#30ff70', '#3070ff', '#ffffff'];
    for (var i = 0; i < 4; i++) {
      ctx.fillStyle = colors[i];
      ctx.fillRect(12 + i * (w - 24) / 4, 12, (w - 40) / 4, 18);
    }
    ctx.beginPath();
    ctx.moveTo(18, h - 18);
    ctx.lineTo(w - 18, 44);
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 3;
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(18, 44);
    ctx.lineTo(w - 18, h - 18);
    ctx.strokeStyle = '#ffd040';
    ctx.stroke();
    ctx.fillStyle = '#00eeee';
    ctx.fillRect(w / 2 - 7, h / 2 - 7, 14, 14);
    // The runtime presents after this callback returns. Do not flush the ctx.
    console.log('Canvas demo: color bars, crossed lines and a cyan square.');
    setTimeout(function () {
      display.close();
      console.log('Demo complete!');
    }, 20000);
  } catch (error) {
    display.close();
    throw error;
  }
}());
