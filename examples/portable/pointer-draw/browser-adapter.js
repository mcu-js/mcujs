'use strict';

// Explicit mouse-only DOM adapter. No touch, pen, gestures or DOM emulation.
// Use a border/padding-free canvas; coordinates may lie outside its bitmap.
function createBrowserDisplay(element, view) {
  view = view || element.ownerDocument.defaultView;
  var target = new view.EventTarget();
  var active = null, x = 0, y = 0, closed = false;
  Object.defineProperties(target, {
    width: { get: function () { return element.width; } },
    height: { get: function () { return element.height; } }
  });
  target.getContext = function (type) { return element.getContext(type); };
  function position(event) {
    var rect = element.getBoundingClientRect();
    if (!(rect.width > 0 && rect.height > 0)) return false;
    x = (event.clientX - rect.left) * element.width / rect.width;
    y = (event.clientY - rect.top) * element.height / rect.height;
    return isFinite(x) && isFinite(y);
  }
  function emit(type, id) {
    var event = new view.Event(type);
    var fields = { pointerId: id, pointerType: 'mouse', isPrimary: true,
      button: type === 'pointerdown' || type === 'pointerup' ? 0 : -1,
      buttons: type === 'pointerdown' || type === 'pointermove' ? 1 : 0,
      offsetX: x, offsetY: y, clientX: x, clientY: y };
    Object.keys(fields).forEach(function (key) {
      Object.defineProperty(event, key, { value: fields[key], enumerable: true });
    });
    target.dispatchEvent(event);
  }
  function finish(type) {
    if (active === null) return;
    var id = active;
    active = null; // release may synchronously trigger lostpointercapture.
    try { emit(type, id); } finally {
      try {
        if (element.hasPointerCapture(id)) element.releasePointerCapture(id);
      } catch (ignored) { /* Element may have left the document. */ }
    }
  }
  function cancel() { finish('pointercancel'); }
  function matches(event) { return active !== null && event.pointerType === 'mouse' && event.pointerId === active; }
  function down(event) {
    if (closed || active !== null || event.pointerType !== 'mouse' ||
        !event.isPrimary || event.button !== 0 || !(event.buttons & 1) || !position(event)) return;
    active = event.pointerId;
    emit('pointerdown', active);
    if (closed || active === null) return;
    try { element.setPointerCapture(active); } catch (error) { cancel(); }
  }
  function move(event) {
    if (!matches(event)) return;
    if (!(event.buttons & 1) || !position(event)) { cancel(); return; }
    emit('pointermove', active);
  }
  function up(event) {
    if (!matches(event) || event.button !== 0) return;
    if (!position(event)) { cancel(); return; }
    finish('pointerup');
  }
  function interrupted(event) { if (matches(event)) cancel(); }
  var listeners = [
    [element, 'pointerdown', down, false],
    [view, 'pointermove', move, true], [view, 'pointerup', up, true],
    [view, 'pointercancel', interrupted, true],
    [element, 'lostpointercapture', interrupted, false], [view, 'blur', cancel, false]
  ];
  listeners.forEach(function (entry) { entry[0].addEventListener(entry[1], entry[2], entry[3]); });
  return {
    canvas: target,
    present: function () {}, // Browser Canvas presents its drawing automatically.
    close: function () {
      if (closed) return;
      closed = true;
      try { cancel(); } finally {
        listeners.forEach(function (entry) { entry[0].removeEventListener(entry[1], entry[2], entry[3]); });
      }
    }
  };
}
if (typeof module !== 'undefined') module.exports = createBrowserDisplay;
