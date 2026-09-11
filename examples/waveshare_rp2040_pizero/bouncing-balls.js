// Five colored balls on the configured Canvas display; no DVI or pin setup.
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
    var w = canvas.width, h = canvas.height, count = 0;
    if (w < 20 || h < 20) throw new Error('Bouncing balls need a canvas of at least 20 by 20');
    var balls = [
      {x: 40, y: 30, vx: 2, vy: 1.5, r: 8, c: 'red'},
      {x: 80, y: 60, vx: -1.5, vy: 2, r: 10, c: 'lime'},
      {x: 120, y: 40, vx: 1.8, vy: -1.2, r: 6, c: 'blue'},
      {x: 60, y: 80, vx: -2, vy: -1.5, r: 9, c: 'yellow'},
      {x: 100, y: 50, vx: 1.5, vy: 1.8, r: 7, c: 'aqua'}
    ];
    for (var i = 0; i < balls.length; i++) {
      var b = balls[i];
      b.x = Math.max(b.r, Math.min(w - b.r, b.x * w / 160));
      b.y = Math.max(b.r, Math.min(h - b.r, b.y * h / 120));
    }
    function draw() {
      try {
        ctx.fillStyle = 'black';
        ctx.fillRect(0, 0, w, h);
        for (var i = 0; i < balls.length; i++) {
          var b = balls[i];
          b.x += b.vx; b.y += b.vy;
          if (b.x < b.r) { b.x = b.r; b.vx = Math.abs(b.vx); }
          else if (b.x > w - b.r) { b.x = w - b.r; b.vx = -Math.abs(b.vx); }
          if (b.y < b.r) { b.y = b.r; b.vy = Math.abs(b.vy); }
          else if (b.y > h - b.r) { b.y = h - b.r; b.vy = -Math.abs(b.vy); }
          ctx.beginPath();
          ctx.arc(b.x, b.y, b.r, 0, 2 * Math.PI);
          ctx.fillStyle = b.c;
          ctx.fill();
        }
        ctx.fillStyle = 'white';
        ctx.fillText('F:' + count, 2, 9);
        count++;
        // The runtime presents when this JavaScript task returns.
      } catch (error) { stop(); throw error; }
    }
    draw();
    interval = setInterval(draw, 33); // Requested cadence, not a measured FPS.
    deadline = setTimeout(function () {
      stop();
      console.log('Demo complete!');
    }, 30000);
    console.log('Bouncing Balls Demo: five balls for 30 seconds.');
  } catch (error) { stop(); throw error; }
}());
