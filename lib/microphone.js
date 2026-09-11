'use strict';
// Native owns bounded PCM capture and shuts the input off independently of JS.
var native = require('mcujs:microphone-native');
var subscribe = require('events').AbortSignal.subscribe;
function error(code, message) {
  var e = new Error(message); e.code = code; e.resource = 'microphone';
  e.name = code === 'ABORT_ERR' ? 'AbortError' : code === 'EBUSY' ? 'ResourceBusyError' : 'Error';
  return e;
}
function open() {
  if (arguments.length) throw new TypeError('Configured microphone.open takes no arguments');
  var token = native.open(), closed = false, pending = null;
  function active() { if (closed) throw error('ENXIO', 'Microphone handle is closed'); }
  function finish(record, failed, reason, data) {
    if (pending !== record) return;
    // Settle even when native fail-closed cleanup reports an error.
    try { native.stop(token); }
    catch (cause) { failed = true; reason = cause; data = undefined; }
    pending = null;
    if (record.timer !== undefined) clearTimeout(record.timer);
    if (record.unsubscribe) record.unsubscribe();
    if (failed) record.reject(reason); else record.resolve(data);
  }
  function stop() { active(); if (pending) finish(pending, true, error('ABORT_ERR', 'Recording stopped')); }
  function close() { if (closed) return; stop(); native.close(token); closed = true; }
  function recordAudio(options) {
    return new Promise(function (resolve, reject) {
      active();
      if (options === undefined) options = {};
      if (!options || typeof options !== 'object') throw new TypeError('Recording options must be an object');
      Object.keys(options).forEach(function (key) {
        if (['signal', 'duration'].indexOf(key) < 0) throw new RangeError('Unsupported recording option: ' + key);
      });
      var duration = options.duration, signal = options.signal;
      if (typeof duration !== 'number' || !Number.isInteger(duration)) throw new TypeError('Duration must be integer milliseconds');
      if (duration < 20 || duration > 1000) throw new RangeError('Duration must be 20..1000 milliseconds');
      active();
      if (pending) throw error('EBUSY', 'Microphone is already recording');
      var record = {resolve: resolve, reject: reject}; pending = record;
      function schedule() {
        try { record.timer = setTimeout(poll, 4); }
        catch (cause) { throw error('ERR_RESOURCE_EXHAUSTED', 'No microphone service timer available'); }
      }
      function poll() {
        if (pending !== record) return;
        try {
          if (native.poll(token) === 2) finish(record, false, undefined, native.result(token));
          else schedule();
        } catch (cause) { finish(record, true, cause); }
      }
      try {
        if (signal !== undefined) record.unsubscribe = subscribe(signal, function (reason) { finish(record, true, reason); });
        if (pending !== record) { if (record.unsubscribe) record.unsubscribe(); return; }
        active(); schedule(); // reserve service timer before capture
        native.start(token, duration);
      } catch (cause) { finish(record, true, cause); }
    });
  }
  return Object.freeze({record: recordAudio, stop: stop, close: close,
    get state() { return closed ? 'closed' : pending ? 'recording' : 'open'; }
  });
}
module.exports = Object.freeze({open: open, getState: function () { return native.state(); }});
