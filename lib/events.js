'use strict';

// A flat, synchronous event surface. No DOM tree or device event queue.
// JerryScript 3.0's weak containers retain empty backing slots indefinitely.
// Keep state with each instance so normal GC reclaims it, including cycles.
// This is encapsulation for an application API, not a hostile-realm boundary.
var stateKey = Symbol('mcujs.events.state');
function set(value, data) {
  data.self = value;
  Object.defineProperty(value, stateKey, { value: data });
}
var limits = Object.freeze({ listeners: 16, subscriptions: 16, depth: 4, callbacks: 32 });
var empty = Object.freeze({});
var depth = 0;
var budget = 0;
function dictionary(value, keys) {
  if (value == null) return empty;
  if (typeof value !== 'object') throw new TypeError('Options must be an object');
  Object.keys(value).forEach(function (key) {
    if (keys.indexOf(key) < 0) throw new RangeError('Unsupported option: ' + key);
  });
  return value;
}
function listenerOptions(value, keys) {
  if (value === true) throw new RangeError('Capture is not supported');
  var options = dictionary(value === false ? null : value, keys);
  if (options.capture || options.passive) throw new RangeError('Capture/passive are not supported');
  return options;
}
function get(value, kind) {
  var property = Object.getOwnPropertyDescriptor(value, stateKey);
  var data = property && property.value;
  if (!data || data.self !== value || data.kind !== kind) throw new TypeError('Invalid ' + kind + ' receiver');
  return data;
}
function remove(record) {
  record.removed = true;
  var list = record.owner.listeners;
  var index = list.indexOf(record);
  if (index >= 0) list.splice(index, 1);
  if (record.signal) {
    var subscriptions = record.signal.subscriptions;
    index = subscriptions.indexOf(record);
    if (index >= 0) subscriptions.splice(index, 1);
  }
}
function signalData(value) {
  var data = get(value, 'target').abort;
  if (!data) throw new TypeError('Expected an MCU.js AbortSignal');
  return data;
}
class Event {
  constructor(type, options) {
    if (arguments.length === 0) throw new TypeError('Event type is required');
    type = '' + type;
    options = dictionary(options, ['cancelable', 'bubbles', 'composed']);
    if (options.bubbles || options.composed) throw new RangeError('Event propagation is not supported');
    set(this, { kind: 'event', type: type, cancelable: !!options.cancelable,
      defaultPrevented: false, target: null, currentTarget: null });
  }
  get type() { return get(this, 'event').type; }
  get cancelable() { return get(this, 'event').cancelable; }
  get defaultPrevented() { return get(this, 'event').defaultPrevented; }
  get target() { return get(this, 'event').target; }
  get currentTarget() { return get(this, 'event').currentTarget; }
  stopImmediatePropagation() { get(this, 'event').stopped = true; }
  preventDefault() {
    var data = get(this, 'event');
    if (data.cancelable) data.defaultPrevented = true;
  }
}
class EventTarget {
  // MCU.js lifecycle extension: clear registrations, including signal links.
  static clear(target) { get(target, 'target').listeners.slice().forEach(remove); }
  constructor() { set(this, { kind: 'target', listeners: [] }); }
  addEventListener(type, callback, options) {
    var data = get(this, 'target');
    type = '' + type;
    options = listenerOptions(options, ['capture', 'passive', 'once', 'signal']);
    var once = !!options.once;
    var signal = options.signal;
    var cancellation = signal === undefined ? null : signalData(signal);
    if (callback == null) return;
    if (typeof callback !== 'function' && typeof callback !== 'object') throw new TypeError('Invalid listener');
    // Options getters may abort or mutate the target: check after reading them.
    if (cancellation && cancellation.aborted) return;
    if (data.listeners.some(function (r) { return r.type === type && r.callback === callback; })) return;
    if (data.listeners.length >= limits.listeners) throw new RangeError('Listener limit exceeded');
    if (cancellation && cancellation.subscriptions.length >= limits.subscriptions)
      throw new RangeError('Abort subscription limit exceeded');
    var record = { owner: data, type: type, callback: callback, once: once, signal: cancellation };
    data.listeners.push(record);
    if (cancellation) cancellation.subscriptions.push(record);
  }
  removeEventListener(type, callback, options) {
    var data = get(this, 'target');
    type = '' + type;
    listenerOptions(options, ['capture']);
    var record = data.listeners.find(function (r) { return r.type === type && r.callback === callback; });
    if (record) remove(record);
  }
  dispatchEvent(event) {
    var data = get(this, 'target');
    var e = get(event, 'event');
    if (e.currentTarget !== null) {
      var invalid = new Error('Event is already being dispatched');
      invalid.name = 'InvalidStateError';
      throw invalid;
    }
    if (depth >= limits.depth) throw new RangeError('Event dispatch depth exceeded');
    var snapshot = data.listeners.slice();
    if (depth === 0) budget = limits.callbacks;
    depth++;
    e.target = this;
    e.currentTarget = this;
    e.stopped = false;
    try {
      for (var i = 0; i < snapshot.length && !e.stopped; i++) {
        var record = snapshot[i];
        if (record.removed || record.type !== e.type) continue;
        if (budget === 0) throw new RangeError('Event callback budget exceeded');
        budget--;
        if (record.once) remove(record);
        try {
          if (typeof record.callback === 'function') record.callback.call(this, event);
          else record.callback.handleEvent(event);
        } catch (error) {
          // Reporting must not prevent later listeners or cleanup.
          try { console.error('Event listener error:', error); } catch (ignored) {}
        }
      }
    } finally { e.currentTarget = null; e.stopped = false; depth--; }
    return !e.defaultPrevented;
  }
}
var signalToken = {};
var dispatch = EventTarget.prototype.dispatchEvent;
class AbortSignal extends EventTarget {
  constructor(token) {
    if (token !== signalToken) throw new TypeError('Use AbortController to create a signal');
    super();
    get(this, 'target').abort = { aborted: false, reason: undefined, subscriptions: [] };
  }
  get aborted() { return signalData(this).aborted; }
  get reason() { return signalData(this).reason; }
  throwIfAborted() {
    var data = signalData(this);
    if (data.aborted) throw data.reason;
  }
}
class AbortController {
  constructor() { set(this, { kind: 'controller', signal: new AbortSignal(signalToken) }); }
  get signal() { return get(this, 'controller').signal; }
  abort(reason) {
    var signal = get(this, 'controller').signal;
    var data = signalData(signal);
    if (data.aborted) return;
    if (reason === undefined) {
      reason = new Error('The operation was aborted');
      reason.name = 'AbortError';
    }
    data.aborted = true;
    data.reason = reason;
    // Cancellation algorithms are private, not forgeable 'abort' listeners.
    // Remove subscriptions before user listeners can stop or throw in dispatch.
    data.subscriptions.slice().forEach(remove);
    dispatch.call(signal, new Event('abort'));
  }
}
module.exports = { Event: Event, EventTarget: EventTarget,
  AbortController: AbortController, AbortSignal: AbortSignal, limits: limits };
