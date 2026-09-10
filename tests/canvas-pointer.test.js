'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const events = require('../lib/events');
function fixture() {
  let tick, sample = [false, 0, 0], lifecycle, state = 'open', reads = 0, stops = 0;
  const backend = { width: 240, height: 280, maxTouchPoints: 1,
    getState: () => state, draw() {}, present() {},
    setLifecycle(fn) { lifecycle = fn; },
    close() { state = 'closed'; lifecycle(); },
    startPointer() {}, stopPointer() { stops++; },
    samplePointer() { reads++; if (sample instanceof Error) throw sample; return sample; }
  };
  const module = {};
  vm.runInNewContext(fs.readFileSync(require.resolve('../lib/canvas'), 'utf8'), {
    module, require: n => n === 'events' ? events : { open: () => backend },
    setInterval(fn, ms) { assert.equal(ms, 16); tick = fn; return 1; },
    clearInterval() { tick = undefined; }
  });
  const display = module.exports.display;
  return { display, canvas: display.canvas, backend,
    sample(value) { sample = value; if (tick) tick(); },
    fail() { state = 'error'; lifecycle(); },
    get reads() { return reads; }, get stops() { return stops; }, get timer() { return tick; } };
}
function observe(canvas) {
  const seen = [];
  for (const type of ['pointerdown', 'pointermove', 'pointerup', 'pointercancel'])
    canvas.addEventListener(type, e => {
      assert.equal(e.target, canvas); assert.equal(e.currentTarget, canvas);
      seen.push(e);
    });
  return seen;
}
test('Canvas touch drag uses one stable contact and bitmap-local fields', () => {
  const f = fixture(), seen = observe(f.canvas);
  assert.equal(f.canvas.maxTouchPoints, 1);
  f.display.startPointer();
  f.sample([true, 10, 20]); f.sample([true, 10, 20]);
  f.sample([true, 30, 40]); f.sample([false, 0, 0]);
  assert.deepEqual(seen.map(e => [e.type, e.button, e.buttons, e.offsetX, e.offsetY]), [
    ['pointerdown', 0, 1, 10, 20], ['pointermove', -1, 1, 30, 40], ['pointerup', 0, 0, 30, 40]
  ]);
  for (const e of seen) {
    assert.equal(e.pointerType, 'touch'); assert.equal(e.isPrimary, true);
    assert.equal(e.pointerId, seen[0].pointerId);
    assert.equal(e.clientX, e.offsetX); assert.equal(e.clientY, e.offsetY);
  }
  f.sample([true, 50, 60]);
  assert.notEqual(seen[3].pointerId, seen[0].pointerId);
  f.display.close();
  assert.equal(seen[4].type, 'pointercancel');
  assert.equal(seen[4].buttons, 0); assert.equal(seen[4].button, -1);
  assert.equal(f.timer, undefined);
  f.sample([true, 70, 80]); f.display.close();
  assert.equal(seen.length, 5);
});
test('sample failure cancels instead of releasing and stops polling', () => {
  const f = fixture(), seen = observe(f.canvas), errors = [];
  f.canvas.addEventListener('error', e => errors.push(e.error));
  f.display.startPointer(); f.sample([true, 10, 20]);
  const failure = new Error('I2C read failed'); f.sample(failure);
  assert.deepEqual(seen.map(e => e.type), ['pointerdown', 'pointercancel']);
  assert.deepEqual(errors, [failure]); assert.equal(f.timer, undefined);
});
test('native lifecycle failure cancels a held contact and clears timer', () => {
  const f = fixture(), seen = observe(f.canvas);
  f.display.startPointer(); f.sample([true, 10, 20]); f.fail();
  assert.deepEqual(seen.map(e => e.type), ['pointerdown', 'pointercancel']);
  assert.equal(f.timer, undefined); assert.throws(() => f.display.startPointer());
});
