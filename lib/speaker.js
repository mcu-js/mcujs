'use strict';
// Native owns file, fixed buffers, DMA and independent fail-silent output.
var native = require('mcujs:speaker-native');
var subscribe = require('events').AbortSignal.subscribe;
function error(code, message) {
  var e = new Error(message); e.code = code; e.resource = 'speaker';
  e.name = code === 'ABORT_ERR' ? 'AbortError' : code === 'EBUSY' ? 'ResourceBusyError' : 'Error';
  return e;
}
function open() {
  if (arguments.length) throw new TypeError('Configured speaker.open takes no arguments');
  var token = native.open(), closed = false, pending = null;
  function active() { if (closed) throw error('ENXIO', 'Speaker handle is closed'); }
  function finish(record, failed, reason) {
    if (pending !== record) return;
    native.stop(token);
    pending = null;
    if (record.timer !== undefined) clearTimeout(record.timer);
    if (record.unsubscribe) record.unsubscribe();
    if (failed) record.reject(reason); else record.resolve();
  }
  function stop() { active(); if (pending) finish(pending, true, error('ABORT_ERR', 'Playback stopped')); }
  function close() { if (closed) return; stop(); native.close(token); closed = true; }
  function play(path, options) {
    return new Promise(function (resolve, reject) {
      active();
      if (typeof path !== 'string' || !path.length || path.indexOf('\0') !== -1) throw new TypeError('WAV path required');
      if (options === undefined) options = {};
      if (!options || typeof options !== 'object') throw new TypeError('Playback options must be an object');
      Object.keys(options).forEach(function (key) {
        if (['signal', 'volume'].indexOf(key) < 0) throw new RangeError('Unsupported playback option: ' + key);
      });
      var volume = options.volume, signal = options.signal;
      if (volume === undefined) volume = 0.25;
      if (typeof volume !== 'number' || !Number.isFinite(volume)) throw new TypeError('Volume must be finite');
      if (volume < 0 || volume > 1) throw new RangeError('Volume must be between 0 and 1');
      active();
      if (pending) throw error('EBUSY', 'Speaker is already playing');
      var record = {resolve: resolve, reject: reject}; pending = record;
      function schedule() {
        try { record.timer = setTimeout(poll, 4); }
        catch (cause) { throw error('ERR_RESOURCE_EXHAUSTED', 'No speaker service timer available'); }
      }
      function poll() {
        if (pending !== record) return;
        try {
          if (native.poll(token) === 2) finish(record, false);
          else schedule();
        } catch (cause) { finish(record, true, cause); }
      }
      try {
        if (signal !== undefined) record.unsubscribe = subscribe(signal, function (reason) { finish(record, true, reason); });
        if (pending !== record) { if (record.unsubscribe) record.unsubscribe(); return; }
        active(); schedule(); // reserve service timer before output
        native.start(token, path, volume);
      } catch (cause) { finish(record, true, cause); }
    });
  }
  return Object.freeze({play: play, stop: stop, close: close,
    get state() { return closed ? 'closed' : pending ? 'playing' : 'open'; }
  });
}
module.exports = Object.freeze({open: open, getState: function () { return native.state(); }});
