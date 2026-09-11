// Scrolling rainbow bars and a frame counter on the configured Canvas display.
// Historical PiZero folder; no DVI, controller or pin setup in this consumer.
// Runs for 30 seconds, then closes. Run again after "Demo complete!".
(function () {
  var devices = require('devices');
  if (!devices.display) throw new Error('No configured display');
  var display = devices.display.open();
  var interval, deadline;
  function stop() {
    clearInterval(interval);
    clearTimeout(deadline);
    display.close();
  }
  try {
    var canvas = display.canvas, ctx = canvas.getContext('2d');
    var w = canvas.width, h = canvas.height, offset = 0, count = 0;
    function hue2rgb(hue) {
      hue = hue & 255;
      var r, g, b;
      if (hue < 85) { r = hue * 3; g = 255 - hue * 3; b = 0; }
      else if (hue < 170) { hue -= 85; r = 255 - hue * 3; g = 0; b = hue * 3; }
      else { hue -= 170; r = 0; g = hue * 3; b = 255 - hue * 3; }
      return 'rgb(' + r + ',' + g + ',' + b + ')';
    }
    function draw() {
      try {
        for (var y = 0; y < h; y += 10) {
          ctx.fillStyle = hue2rgb((y + offset) * 2);
          ctx.fillRect(0, y, w, Math.min(10, h - y));
        }
        ctx.fillStyle = '#ffffff';
        ctx.fillText('F:' + count, 2, 9); // 8px font, baseline = top + 7.
        offset = (offset + 4) & 255;
        count++;
        // Automatic task-end presentation; do not flush or access buffers.
      } catch (error) { stop(); throw error; }
    }
    draw();
    interval = setInterval(draw, 50); // Target 20 updates/s, not an FPS promise.
    deadline = setTimeout(function () {
      stop();
      console.log('Demo complete!');
    }, 30000);
    console.log('Rainbow Demo: scrolling bars and frame counter for 30 seconds.');
  } catch (error) { stop(); throw error; }
}());
