'use strict';
// Private configured-tone adapter. Native owns the pin, slice and safety alarm.
var native = require('mcujs:buzzer-native');
var events = require('events');
var subscribe = events.AbortSignal.subscribe;
function error(code, message) {
  var e = new Error(message); e.code = code; e.resource = 'buzzer';
  e.name = code === 'EBUSY' ? 'ResourceBusyError' : code === 'ERR_RESOURCE_EXHAUSTED' ? 'ResourceExhaustedError' : code === 'ABORT_ERR' ? 'AbortError' : 'Error';
  return e;
}
function open() {
  if (arguments.length) throw new TypeError('Configured buzzer.open takes no arguments');
  var token = native.open(), closed = false, pending = null;
  function active() { if (closed) throw error('ENXIO', 'Buzzer handle is closed'); }
  function finish(record, failed, reason) {
    if (pending !== record) return;
    native.stop(token);
    pending = null;
    if (record.timer !== undefined) clearTimeout(record.timer);
    if (record.unsubscribe) record.unsubscribe();
    if (failed) record.reject(reason); else record.resolve(Object.freeze({frequency: record.frequency, duration: record.duration}));
  }
  function stop() { active(); if (pending) finish(pending, true, error('ABORT_ERR', 'Tone stopped')); }
  function close() { if (closed) return; stop(); native.close(token); closed = true; }
  function beep(options) {
    // All validation failures reject, including property getter failures.
    return new Promise(function (resolve, reject) {
      active();
      if (!options || typeof options !== 'object') throw new TypeError('Tone options required');
      Object.keys(options).forEach(function (key) {
        if (['frequency', 'duration', 'signal'].indexOf(key) < 0) throw new RangeError('Unsupported tone option: ' + key);
      });
      var frequency = options.frequency, duration = options.duration, signal = options.signal;
      [frequency, duration].forEach(function (v) { if (typeof v !== 'number' || !Number.isFinite(v)) throw new TypeError('Tone values must be finite numbers'); });
      if (!Number.isInteger(frequency) || frequency < 500 || frequency > 4000 ||
          !Number.isInteger(duration) || duration < 1 || duration > 1000) throw new RangeError('Tone outside capability');
      active();
      if (pending) throw error('EBUSY', 'Buzzer is already playing');
      var record = {resolve: resolve, reject: reject, frequency: frequency, duration: duration};
      pending = record;
      try {
        if (signal !== undefined) record.unsubscribe = subscribe(signal, function (reason) { finish(record, true, reason); });
        if (pending !== record) { if (record.unsubscribe) record.unsubscribe(); return; } // already aborted or closed during branding
        active();
        // Reserve the JS completion timer before energizing hardware.
        try { record.timer = setTimeout(function () { finish(record, false); }, duration); }
        catch (cause) { throw error('ERR_RESOURCE_EXHAUSTED', 'No tone completion timer available'); }
        record.frequency = native.start(token, frequency, duration);
      } catch (cause) { finish(record, true, cause); }
    });
  }
  return Object.freeze({beep: beep, stop: stop, close: close,
    get state() { return closed ? 'closed' : pending && native.playing(token) ? 'playing' : 'open'; }
  });
}
module.exports = Object.freeze({open: open, getState: function () { return native.state(); }});
