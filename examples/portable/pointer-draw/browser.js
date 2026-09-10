// Explicit browser-only setup: no firmware modules or globals are emulated.
(function () {
  var display = createBrowserDisplay(document.getElementById('drawing'));
  var stop = pointerDraw(display);
  var status = document.getElementById('status');
  var timer, finished = false;
  function finish() {
    if (finished) return;
    finished = true;
    clearTimeout(timer);
    window.removeEventListener('pagehide', finish);
    try { display.close(); } finally { stop(); }
    status.textContent = 'Demo complete! Reload to draw again.';
    console.log('Demo complete!');
  }
  window.addEventListener('pagehide', finish);
  timer = setTimeout(finish, 30000);
}());
