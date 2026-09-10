'use strict';

// Identical application consumer in firmware and the browser. No hardware or DOM.
function pointerDraw(display) {
  var canvas = display.canvas, ctx = canvas.getContext('2d');
  var active = null, x = 0, y = 0, stopped = false;
  ctx.fillStyle = 'black';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = 'white'; ctx.strokeStyle = 'white'; ctx.lineWidth = 2;
  display.present();
  function down(event) {
    if (active !== null || !event.isPrimary || event.button !== 0) return;
    active = event.pointerId; x = event.offsetX; y = event.offsetY;
    ctx.fillRect(x - 1, y - 1, 2, 2);
    display.present();
  }
  function segment(event) {
    // Each segment has its own bounded path, never an ever-growing stroke.
    ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(event.offsetX, event.offsetY);
    ctx.stroke(); x = event.offsetX; y = event.offsetY;
    display.present();
  }
  function move(event) {
    if (active === null || event.pointerId !== active) return;
    if (!(event.buttons & 1)) { active = null; return; }
    segment(event);
  }
  function up(event) {
    if (active === null || event.pointerId !== active) return;
    segment(event); active = null;
  }
  function cancel(event) { if (event.pointerId === active) active = null; }
  var handlers = { pointerdown: down, pointermove: move, pointerup: up, pointercancel: cancel };
  Object.keys(handlers).forEach(function (type) { canvas.addEventListener(type, handlers[type]); });
  return function stop() {
    if (stopped) return;
    stopped = true; active = null;
    Object.keys(handlers).forEach(function (type) { canvas.removeEventListener(type, handlers[type]); });
  };
}
// CommonJS firmware entry; the same file is a plain script in browser.html.
if (typeof module !== 'undefined') module.exports = pointerDraw;
