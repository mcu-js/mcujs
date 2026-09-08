/* Standard Canvas calls; the same function runs in MCU.js and a browser. */
function startCanvasAnimation(canvas, onDone) {
  var ctx = canvas.getContext('2d');
  var start = Date.now();
  var timer;
  var state = { frames: 0, running: true };
  state.stop = function () {
    if (!state.running) return;
    state.running = false;
    clearTimeout(timer);
    if (onDone) onDone(state.frames, Date.now() - start);
  };
  function frame() {
    var elapsed = Date.now() - start;
    if (elapsed >= 60000) { state.stop(); return; }
    var phase = (elapsed % 4000) / 4000;
    var x = 12 + Math.floor((phase < 0.5 ? phase * 2 : (1 - phase) * 2) * 116);
    ctx.fillStyle = '#08182e';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = '#00ffff';
    ctx.fillRect(x, 38, 16, 16);
    ctx.beginPath();
    ctx.moveTo(12, 104);
    ctx.lineTo(x + 8, 64);
    ctx.strokeStyle = '#ffd040';
    ctx.lineWidth = 2;
    ctx.stroke();
    state.frames++;
    timer = setTimeout(frame, 50);
  }
  frame();
  return state;
}
if (typeof module !== 'undefined') module.exports = startCanvasAnimation;
