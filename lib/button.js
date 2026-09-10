'use strict';
// Private implementation; application discovery lives in require('devices').
var owner = null;
var unavailable = false;
function error(code, message, cause) {
  var e = new Error(message);
  e.code = code;
  if (cause !== undefined) e.cause = cause;
  e.resource = 'button';
  e.name = code === 'EBUSY' ? 'ResourceBusyError' : code === 'ERR_RESOURCE_EXHAUSTED' ? 'ResourceExhaustedError' : 'Error';
  return e;
}
function open() {
  if (arguments.length) throw new TypeError('Configured button.open takes no arguments');
  if (owner) throw error('EBUSY', 'Configured button is owned');
  var board = require('board');
  var events = require('events');
  var handle = new events.EventTarget();
  var state = 'open', pressed, timer, candidate, since;
  var dispatch = events.EventTarget.prototype.dispatchEvent;
  function active() { if (state !== 'open') throw error('ENXIO', 'Button handle is closed or failed'); }
  function read() {
    try {
      var value = board.buttonPressed();
      if (typeof value !== 'boolean') throw error('EIO', 'Invalid button sample');
      return value;
    } catch (cause) { throw error('EIO', 'Unable to sample button', cause); }
  }
  var add = events.EventTarget.prototype.addEventListener;
  var remove = events.EventTarget.prototype.removeEventListener;
  var clear = events.EventTarget.clear;
  function addListener() {
    active();
    add.apply(handle, arguments);
    // Options getters may close the handle before the underlying add finishes.
    if (state !== 'open') { clear(handle); active(); }
  }
  function removeListener() { remove.apply(handle, arguments); }
  function stop() {
    if (timer !== undefined) { clearInterval(timer); timer = undefined; }
    if (owner === handle) owner = null;
  }
  function close() {
    if (state === 'open') state = 'closed';
    stop();
    clear(handle);
  }
  function poll() {
    if (state !== 'open') return;
    var value, now;
    try { value = read(); now = board.millis() >>> 0; }
    catch (cause) {
      state = 'error'; unavailable = true; stop();
      var event = new events.Event('error');
      Object.defineProperty(event, 'error', { value: cause, enumerable: true });
      try { dispatch.call(handle, event); } finally { clear(handle); }
      return;
    }
    if (value !== candidate) { candidate = value; since = now; }
    if (candidate !== pressed && ((now - since) >>> 0) >= 30) {
      pressed = candidate;
      dispatch.call(handle, new events.Event(pressed ? 'press' : 'release'));
    }
  }
  Object.defineProperties(handle, {
    state: { enumerable: true, get: function () { return state; } },
    pressed: { enumerable: true, get: function () { active(); return pressed; } },
    close: { value: close },
    addEventListener: { value: addListener },
    removeEventListener: { value: removeListener },
    dispatchEvent: { value: function (event) { active(); return dispatch.call(handle, event); } }
  });
  owner = handle;
  try {
    pressed = read();
    candidate = pressed;
    since = board.millis() >>> 0;
    try { timer = setInterval(poll, 10); }
    catch (cause) { throw error('ERR_RESOURCE_EXHAUSTED', 'No button polling timer available', cause); }
  } catch (cause) { unavailable = true; close(); throw cause; }
  owner = handle;
  unavailable = false;
  return Object.freeze(handle);
}
module.exports = Object.freeze({ open: open, getState: function () { return owner ? 'busy' : unavailable ? 'unavailable' : 'idle'; } });
