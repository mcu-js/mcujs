const test = require('node:test');
const assert = require('node:assert/strict');

test('EventTarget dispatches synchronously with target, currentTarget and cancellation', () => {
  const { Event, EventTarget } = require('../lib/events.js');
  const target = new EventTarget();
  const event = new Event('press', { cancelable: true });
  let calls = 0;
  target.addEventListener('press', function (received) {
    assert.equal(this, target);
    assert.equal(received, event);
    assert.equal(event.target, target);
    assert.equal(event.currentTarget, target);
    event.preventDefault();
    calls++;
  });
  assert.equal(target.dispatchEvent(event), false);
  assert.equal(calls, 1);
  assert.equal(event.currentTarget, null);
  assert.equal(event.defaultPrevented, true);
});

test('listener identity, once and mutation remain safe under reentrant dispatch', () => {
  const { Event, EventTarget } = require('../lib/events.js');
  const target = new EventTarget();
  const seen = [];
  function late() { seen.push('late'); }
  function removed() { seen.push('removed'); }
  function first() {
    seen.push('first');
    target.removeEventListener('go', removed);
    target.addEventListener('go', late);
    target.dispatchEvent(new Event('go'));
  }
  target.addEventListener('go', first, { once: true });
  target.addEventListener('go', first); // Duplicate cannot change once.
  target.addEventListener('go', removed);
  target.dispatchEvent(new Event('go'));
  assert.deepEqual(seen, ['first', 'late']);
  target.removeEventListener('go', late);
  target.dispatchEvent(new Event('go'));
  assert.deepEqual(seen, ['first', 'late']);
});

test('listener exceptions are reported without skipping later listeners or leaking dispatch state', () => {
  const { Event, EventTarget } = require('../lib/events.js');
  const target = new EventTarget();
  const event = new Event('go');
  const seen = [];
  const errors = [];
  const previous = console.error;
  console.error = (...args) => errors.push(args);
  try {
    target.addEventListener('go', () => { throw new Error('listener failed'); }, { once: true });
    target.addEventListener('go', { handleEvent(received) { seen.push(received.type); } });
    assert.equal(target.dispatchEvent(event), true);
    assert.equal(target.dispatchEvent(event), true);
  } finally { console.error = previous; }
  assert.deepEqual(seen, ['go', 'go']);
  assert.equal(errors.length, 1);
  assert.equal(event.currentTarget, null);
});

test('same-event redispatch is rejected and stopImmediatePropagation resets after dispatch', () => {
  const { Event, EventTarget } = require('../lib/events.js');
  const target = new EventTarget();
  const event = new Event('go');
  let rejected;
  let calls = 0;
  target.addEventListener('go', () => {
    try { target.dispatchEvent(event); } catch (error) { rejected = error.name; }
    event.stopImmediatePropagation();
  }, { once: true });
  target.addEventListener('go', () => calls++);
  target.dispatchEvent(event);
  assert.equal(rejected, 'InvalidStateError');
  assert.equal(calls, 0);
  target.dispatchEvent(event);
  assert.equal(calls, 1);
});

test('abort cleans signal-bound listeners before notifying, keeps the first reason and is idempotent', () => {
  const { Event, EventTarget, AbortController } = require('../lib/events.js');
  const controller = new AbortController();
  const target = new EventTarget();
  const signal = controller.signal;
  const reason = { why: 'closed' };
  let calls = 0;
  let notifications = 0;
  target.addEventListener('go', () => calls++, { signal });
  signal.addEventListener('abort', event => {
    notifications++;
    target.dispatchEvent(new Event('go'));
    event.stopImmediatePropagation();
  });
  controller.abort(reason);
  controller.abort('ignored');
  target.addEventListener('go', () => calls++, { signal });
  target.dispatchEvent(new Event('go'));
  assert.equal(calls, 0);
  assert.equal(notifications, 1);
  assert.equal(signal.aborted, true);
  assert.equal(signal.reason, reason);
  assert.throws(() => signal.throwIfAborted(), error => error === reason);
});

test('synthetic abort events cannot cancel work; real abort during dispatch removes later listeners', () => {
  const { Event, EventTarget, AbortController } = require('../lib/events.js');
  const controller = new AbortController();
  const target = new EventTarget();
  let calls = 0;
  target.addEventListener('go', () => calls++, { signal: controller.signal });
  controller.signal.dispatchEvent(new Event('abort'));
  assert.equal(controller.signal.aborted, false);
  controller.signal.throwIfAborted();
  target.dispatchEvent(new Event('go'));
  assert.equal(calls, 1);
  const other = new EventTarget();
  other.addEventListener('go', () => controller.abort(), { once: true });
  other.addEventListener('go', () => calls++, { signal: controller.signal });
  other.dispatchEvent(new Event('go'));
  assert.equal(calls, 1);
  assert.equal(controller.signal.reason.name, 'AbortError');
});

test('only genuine signals are accepted and an aborting options getter cannot leave a listener attached', () => {
  const { Event, EventTarget, AbortSignal, AbortController } = require('../lib/events.js');
  const controller = new AbortController();
  const target = new EventTarget();
  let calls = 0;
  assert.throws(() => new AbortSignal(), TypeError);
  assert.throws(() => target.addEventListener('go', () => {}, { signal: {} }), TypeError);
  target.addEventListener('go', () => calls++, {
    signal: controller.signal,
    get once() { controller.abort(); return true; }
  });
  target.dispatchEvent(new Event('go'));
  assert.equal(calls, 0);
});

test('listener and signal-subscription limits reject overflow without losing existing registrations', () => {
  const { Event, EventTarget, AbortController, limits } = require('../lib/events.js');
  assert.deepEqual(limits, { listeners: 16, subscriptions: 16, depth: 4, callbacks: 32 });
  const target = new EventTarget();
  const controller = new AbortController();
  let calls = 0;
  const callbacks = Array.from({ length: 16 }, () => () => calls++);
  callbacks.forEach(callback => target.addEventListener('go', callback, { signal: controller.signal }));
  target.addEventListener('go', callbacks[0]); // A duplicate doesn't consume capacity.
  assert.throws(() => target.addEventListener('go', () => {}), RangeError);
  const other = new EventTarget();
  assert.throws(() => other.addEventListener('go', () => {}, { signal: controller.signal }), RangeError);
  target.dispatchEvent(new Event('go'));
  assert.equal(calls, 16);
  target.removeEventListener('go', callbacks[0]);
  other.addEventListener('go', () => calls++, { signal: controller.signal });
  controller.abort();
  other.dispatchEvent(new Event('go'));
  assert.equal(calls, 16);
});

test('once and explicit removal release cancellation subscriptions across repeated workloads', () => {
  const { Event, EventTarget, AbortController } = require('../lib/events.js');
  const controller = new AbortController();
  const target = new EventTarget();
  const callback = () => {};
  for (let i = 0; i < 256; i++) {
    target.addEventListener('go', callback, { signal: controller.signal, once: true });
    target.dispatchEvent(new Event('go'));
    target.addEventListener('go', callback, { signal: controller.signal });
    target.removeEventListener('go', callback);
  }
  for (let i = 0; i < 16; i++) target.addEventListener('go', () => {}, { signal: controller.signal });
  assert.throws(() => new EventTarget().addEventListener('go', () => {}, { signal: controller.signal }), RangeError);
});

test('nested dispatch has a shared callback budget and always restores state after overflow', () => {
  const { Event, EventTarget } = require('../lib/events.js');
  const outer = new EventTarget();
  const inner = new EventTarget();
  const event = new Event('go');
  let calls = 0;
  for (let i = 0; i < 16; i++) {
    inner.addEventListener('go', () => calls++);
    outer.addEventListener('go', () => { calls++; inner.dispatchEvent(new Event('go')); });
  }
  const previous = console.error;
  console.error = () => {};
  try { assert.throws(() => outer.dispatchEvent(event), RangeError); }
  finally { console.error = previous; }
  assert.equal(calls, 32);
  assert.equal(event.currentTarget, null);
  inner.dispatchEvent(new Event('go'));
  assert.equal(calls, 48);
});

test('dispatch nesting stops at four and failed error reporting cannot corrupt cleanup', () => {
  const { Event, EventTarget } = require('../lib/events.js');
  const targets = Array.from({ length: 5 }, () => new EventTarget());
  let calls = 0;
  targets.forEach((target, i) => target.addEventListener('go', () => {
    calls++;
    if (i < 4) targets[i + 1].dispatchEvent(new Event('go'));
  }));
  const previous = console.error;
  console.error = () => { throw new Error('reporting failed'); };
  try { targets[0].dispatchEvent(new Event('go')); }
  finally { console.error = previous; }
  assert.equal(calls, 4);
  targets[4].dispatchEvent(new Event('go'));
  assert.equal(calls, 5);
});

test('unsupported options and invalid receivers fail explicitly; null listeners are harmless', () => {
  const { Event, EventTarget } = require('../lib/events.js');
  const target = new EventTarget();
  target.addEventListener('go', null, false);
  target.removeEventListener('go', null, { capture: false });
  assert.equal(target.dispatchEvent(new Event('go')), true);
  for (const options of [true, { capture: true }, { passive: true }, { singal: {} }]) {
    assert.throws(() => target.addEventListener('go', () => {}, options), RangeError);
  }
  assert.throws(() => target.removeEventListener('go', () => {}, true), RangeError);
  assert.throws(() => target.addEventListener('go', 42), TypeError);
  assert.throws(() => target.dispatchEvent({ type: 'go' }), TypeError);
  assert.throws(() => EventTarget.prototype.dispatchEvent.call({}, new Event('go')), TypeError);
  assert.throws(() => new Event('go', { bubbles: true }), RangeError);
  assert.throws(() => new Event('go', { composed: true }), RangeError);
  assert.throws(() => new Event(Symbol('go')), TypeError);
  const event = new Event('go');
  event.preventDefault();
  assert.equal(event.defaultPrevented, false);
});
