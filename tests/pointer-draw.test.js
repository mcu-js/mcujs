'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const events = require('../lib/events');
const fs = require('node:fs');
const vm = require('node:vm');
const createBrowserDisplay = require('../examples/portable/pointer-draw/browser-adapter');

// Minimal public DOM boundary fixture, not a browser or DOM implementation.
class Source extends EventTarget {
  constructor() { super(); this.listeners = []; }
  addEventListener(type, fn, options) {
    super.addEventListener(type, fn, options); this.listeners.push([type, fn, options]);
  }
  removeEventListener(type, fn, options) {
    super.removeEventListener(type, fn, options);
    this.listeners = this.listeners.filter(x => x[0] !== type || x[1] !== fn);
  }
  send(type, fields = {}) {
    const event = new Event(type);
    Object.assign(event, { pointerType: 'mouse', pointerId: 7, isPrimary: true,
      button: 0, buttons: 1, clientX: 110, clientY: 70 }, fields);
    this.dispatchEvent(event);
  }
}
function fixture() {
  const view = new Source(), element = new Source(), calls = [];
  Object.assign(view, { Event, EventTarget });
  Object.assign(element, { width: 240, height: 280,
    getBoundingClientRect: () => ({ left: 10, top: 20, width: 480, height: 140 }),
    getContext: () => ({ fillRect(...args) { calls.push(['fill', ...args]); },
      beginPath() {}, moveTo(...args) { calls.push(['from', ...args]); },
      lineTo(...args) { calls.push(['to', ...args]); }, stroke() {} }),
    setPointerCapture(id) { this.captured = id; },
    hasPointerCapture(id) { return this.captured === id; },
    releasePointerCapture(id) { this.captured = null; this.send('lostpointercapture', { pointerId: id }); }
  });
  const display = createBrowserDisplay(element, view), seen = [];
  for (const type of ['pointerdown', 'pointermove', 'pointerup', 'pointercancel']) {
    display.canvas.addEventListener(type, e => {
      assert.equal(e.target, display.canvas); assert.equal(e.currentTarget, display.canvas);
      seen.push(e);
    });
  }
  return { view, element, display, seen, calls };
}
for (const reason of ['blur', 'lostpointercapture', 'pointercancel', 'capture failure', 'buttons lost']) {
  test(reason + ' cancels once and permits a fresh stroke', () => {
    const f = fixture();
    if (reason === 'capture failure') f.element.setPointerCapture = () => { throw Error('capture unavailable'); };
    f.element.send('pointerdown');
    if (reason === 'blur') f.view.send('blur');
    if (reason === 'lostpointercapture') f.element.send('lostpointercapture');
    if (reason === 'pointercancel') f.view.send('pointercancel');
    if (reason === 'buttons lost') f.view.send('pointermove', { buttons: 0 });
    f.view.send('pointerup', { buttons: 0 });
    assert.deepEqual(f.seen.map(e => e.type), ['pointerdown', 'pointercancel']);
    assert.equal(f.seen[1].buttons, 0);
    f.element.setPointerCapture = id => { f.element.captured = id; };
    f.element.send('pointerdown');
    assert.equal(f.seen[2].type, 'pointerdown');
    f.display.close(); f.display.close();
    assert.equal(f.seen.length, 4);
    assert.equal(f.element.listeners.length + f.view.listeners.length, 0);
    f.element.send('pointerdown'); f.view.send('pointermove');
    assert.equal(f.seen.length, 4);
  });
}
test('ignores touch, pen, secondary buttons, non-primary and unrelated contacts', () => {
  const f = fixture();
  for (const fields of [{ pointerType: 'touch' }, { pointerType: 'pen' }, { button: 2 }, { isPrimary: false }])
    f.element.send('pointerdown', fields);
  assert.equal(f.seen.length, 0);
  f.element.send('pointerdown');
  f.view.send('pointermove', { pointerId: 99 });
  f.element.send('lostpointercapture', { pointerId: 99 });
  assert.equal(f.seen.length, 1);
  f.display.close();
});
test('same consumer draws separate bounded segments on MCU events and browser adapter', () => {
  const draw = require('../examples/portable/pointer-draw/draw');
  const f = fixture();
  const stop = draw(f.display);
  f.element.send('pointerdown');
  f.view.send('pointermove', { clientX: 130, clientY: 80 });
  f.view.send('blur');
  const before = f.calls.length;
  f.view.send('pointermove');
  assert.equal(f.calls.length, before);
  assert.ok(f.calls.some(c => c[0] === 'to' && c[1] === 60 && c[2] === 120));
  stop(); stop();
  f.element.send('pointerdown');
  assert.equal(f.calls.length, before);
  f.display.close();

  const canvas = new events.EventTarget();
  canvas.width = 240; canvas.height = 280; canvas.getContext = f.element.getContext;
  let count = 0;
  const add = canvas.addEventListener, remove = canvas.removeEventListener;
  canvas.addEventListener = function (...args) { count++; return add.apply(this, args); };
  canvas.removeEventListener = function (...args) { count--; return remove.apply(this, args); };
  const done = draw({ canvas, present() {} });
  function send(type, fields) {
    const event = new events.Event(type);
    Object.assign(event, { pointerId: 1, isPrimary: true, button: 0, buttons: 1, offsetX: 3, offsetY: 4 }, fields);
    canvas.dispatchEvent(event);
  }
  send('pointerdown'); send('pointermove', { offsetX: 8, offsetY: 9 });
  assert.deepEqual(f.calls.at(-1), ['to', 8, 9]);
  send('pointercancel', { buttons: 0 });
  const after = f.calls.length;
  send('pointermove'); assert.equal(f.calls.length, after);
  done(); done(); assert.equal(count, 0);
  send('pointerdown'); assert.equal(f.calls.length, after);
});
test('browser script setup runs bounded and removes pagehide and input listeners', () => {
  const f = fixture();
  f.display.close();
  f.element.ownerDocument = { defaultView: f.view };
  const status = {};
  let tick, cleared = 0;
  const context = vm.createContext({
    window: f.view, console: { log() {} },
    document: { getElementById: id => id === 'drawing' ? f.element : status },
    setTimeout(fn, ms) { assert.equal(ms, 30000); tick = fn; return 1; },
    clearTimeout(id) { assert.equal(id, 1); cleared++; }
  });
  for (const file of ['draw.js', 'browser-adapter.js', 'browser.js'])
    vm.runInContext(fs.readFileSync(require.resolve('../examples/portable/pointer-draw/' + file), 'utf8'), context);
  f.element.send('pointerdown');
  tick(); f.view.send('pagehide');
  assert.equal(status.textContent, 'Demo complete! Reload to draw again.');
  assert.equal(cleared, 1);
  assert.equal(f.element.listeners.length + f.view.listeners.length, 0);
});
test('firmware setup starts pointer once and closes after its bounded timer', () => {
  const canvas = new events.EventTarget();
  let started = 0, closed = 0, tick, cleared = 0;
  Object.assign(canvas, { width: 240, height: 280, maxTouchPoints: 1,
    getContext: () => ({ fillRect() {} }) });
  const display = { canvas, present() {}, startPointer() { started++; },
    close() { closed++; events.EventTarget.clear(canvas); } };
  vm.runInNewContext(fs.readFileSync(require.resolve('../examples/portable/pointer-draw/index'), 'utf8'), {
    require: name => name === 'devices' ? { display: { open: () => display } } : require('../examples/portable/pointer-draw/draw'),
    console: { log() {} },
    setTimeout(fn, ms) { assert.equal(ms, 30000); tick = fn; return 1; },
    clearTimeout(id) { assert.equal(id, 1); cleared++; }
  });
  assert.equal(started, 1); assert.equal(closed, 0);
  tick(); tick(); assert.equal(closed, 1); assert.equal(cleared, 1);
});
test('mouse coordinates scale CSS to bitmap, capture and release outside', () => {
  const f = fixture();
  f.element.send('pointerdown');
  assert.equal(f.element.captured, 7);
  f.view.send('pointermove', { clientX: 510, clientY: 170, button: -1 });
  f.view.send('pointerup', { clientX: 530, clientY: 180, buttons: 0 });
  assert.deepEqual(f.seen.map(e => [e.type, e.offsetX, e.offsetY, e.button, e.buttons]), [
    ['pointerdown', 50, 100, 0, 1], ['pointermove', 250, 300, -1, 1],
    ['pointerup', 260, 320, 0, 0]
  ]);
  assert.ok(f.seen.every(e => e.pointerId === 7 && e.pointerType === 'mouse' && e.isPrimary));
  assert.equal(f.element.captured, null);
  f.display.close();
});
