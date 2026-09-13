'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const events = require('../lib/events');
const root = path.resolve(__dirname, '../examples');

// .run executes a fresh CommonJS entry in a persistent realm, not a global script.
function fixture(overrides = {}) {
  const timers = new Map(), logs = [], cache = {}, loads = [];
  let next = 0;
  const board = { name: 'test', chip: 'test', devices: { led: {} },
    freeMemory: () => 1000, millis: () => 0, led() {} };
  const builtins = { board, 'mcujs:module': { has: () => true }, ...overrides };
  const context = vm.createContext({ board,
    console: { log: (...a) => logs.push(a.join(' ')), error: (...a) => logs.push(a.join(' ')) },
    fs: { writeFileSync() { assert.fail('example must not write files'); },
      mkdirSync() { assert.fail('example must not create directories'); }, existsSync: () => true },
    setInterval: (fn, ms) => { timers.set(++next, { fn, ms, interval: true }); return next; },
    setTimeout: (fn, ms) => { timers.set(++next, { fn, ms }); return next; },
    clearInterval: id => timers.delete(id), clearTimeout: id => timers.delete(id)
  });
  function load(file, entry = false) {
    if (!path.extname(file)) file += '.js';
    if (!entry && cache[file]) return cache[file].exports;
    const mod = { exports: {} };
    cache[file] = mod;
    if (file.endsWith('.json')) mod.exports = JSON.parse(fs.readFileSync(file, 'utf8'));
    else {
      const localRequire = name => {
        loads.push(name);
        if (Object.hasOwn(builtins, name)) return builtins[name];
        assert.ok(name.startsWith('.'), 'unexpected builtin: ' + name);
        return load(path.resolve(path.dirname(file), name));
      };
      localRequire.cache = cache;
      vm.runInContext('(function(require,module,exports){\n' + fs.readFileSync(file, 'utf8') + '\n})', context, { filename: file })(localRequire, mod, mod.exports);
    }
    return mod.exports;
  }
  return { context, timers, logs, loads, board,
    run: file => load(path.join(root, file), true),
    expire(ms) { for (const [id, timer] of [...timers]) if (!timer.interval && timer.ms === ms) { timers.delete(id); timer.fn(); } }
  };
}

test('storage ownership checks the canonical startup path without writing', () => {
  const paths = [];
  const f = fixture({
    board: { storageReady: () => true },
    fs: { existsSync(p) { paths.push(p); return p === '/app/index.js'; } }
  });
  f.run('storage-ownership/index.js');
  f.run('storage-ownership/index.js');
  assert.deepEqual(paths, ['/app/index.js', '/app/index.js']);
  assert.ok(f.logs.every(line => line.endsWith('true')));
});

function input() {
  const target = new events.EventTarget();
  target.listeners = new Set();
  const add = target.addEventListener, remove = target.removeEventListener;
  target.addEventListener = function (type, fn) { this.listeners.add(fn); add.call(this, type, fn); };
  target.removeEventListener = function (type, fn) { this.listeners.delete(fn); remove.call(this, type, fn); };
  return target;
}

test('onboard button rerun closes only the old handle, clears listeners and timer, and ends LED off', () => {
  const handles = [], leds = [];
  const f = fixture({ devices: { button: { open() {
    const handle = input(); handle.pressed = true; handle.closed = 0;
    handle.close = () => { handle.closed++; events.EventTarget.clear(handle); };
    handles.push(handle); return handle;
  } } } });
  f.board.led = on => leds.push(on);
  f.run('onboard-button/index.js');
  const oldTimer = [...f.timers.values()][0].fn;
  f.run('onboard-button/index.js');
  assert.equal(handles[0].closed, 1, 'rerun must close previous handle');
  assert.equal(handles[0].listeners.size, 0);
  assert.equal(handles[1].closed, 0);
  assert.equal(f.timers.size, 1);
  oldTimer();
  assert.equal(handles[1].closed, 0, 'stale callback must not close new handle');
  f.expire(60000);
  assert.equal(f.timers.size, 0);
  assert.equal(handles[1].closed, 1);
  assert.equal(handles[1].listeners.size, 0);
  assert.equal(leds.at(-1), false);
  f.context.onboardButtonStop();
  assert.equal(handles[1].closed, 1);
});

for (const file of ['onboard-button/index.js', 'portable/device-display/index.js', 'portable/pointer-draw/index.js', 'portable/app-draw/index.js']) {
  test(file + ' skips before requiring unavailable devices or sibling helpers', () => {
    const f = fixture({ 'mcujs:module': { has: () => false } });
    f.run(file); f.run(file);
    assert.ok(!f.loads.includes('devices'));
    assert.equal(f.timers.size, 0);
    assert.ok(f.logs.some(s => /not supported|No configured|unavailable/.test(s)));
  });
}

for (const [file, duration, stopName] of [
  ['portable/pointer-draw/index.js', 30000, 'pointerDrawStop'],
  ['portable/app-draw/index.js', 60000, 'appDrawStop']
]) {
  test(file + ' replaces live pointer listeners, handle and timer on persistent rerun', () => {
    const handles = [];
    const f = fixture({
      // app-draw intentionally persists validated settings; config/modules do not.
      fs: { existsSync: () => false, writeFileSync() {} },
      './draw': require('../examples/portable/pointer-draw/draw'),
      devices: { display: { open() {
        const canvas = input();
        Object.assign(canvas, { width: 240, height: 280, maxTouchPoints: 1,
          getContext: () => ({ fillRect() {}, beginPath() {}, moveTo() {}, lineTo() {}, stroke() {} }) });
        const handle = { canvas, closed: 0, present() {}, startPointer() {},
          close() { this.closed++; events.EventTarget.clear(canvas); } };
        handles.push(handle); return handle;
      } } }
    });
    f.run(file);
    const oldTimer = [...f.timers.values()][0].fn;
    f.run(file);
    assert.equal(handles[0].closed, 1, 'rerun must close previous display');
    assert.equal(handles[0].canvas.listeners.size, 0);
    assert.equal(handles[1].closed, 0);
    assert.equal(f.timers.size, 1);
    oldTimer();
    assert.equal(handles[1].closed, 0);
    f.expire(duration);
    assert.equal(handles[1].closed, 1);
    assert.equal(handles[1].canvas.listeners.size, 0);
    assert.equal(f.timers.size, 0);
    f.context[stopName]();
    assert.equal(handles[1].closed, 1);
  });
}

for (const [file, message] of [['config/index.js', 'App: My mcujs App v1.0.0'], ['modules/index.js', 'Module caching: true']]) {
  test(file + ' reads bundled siblings without filesystem writes on rerun', () => {
    const f = fixture();
    f.run(file); f.run(file);
    assert.equal(f.timers.size, 0);
    assert.equal(f.logs.filter(s => s === message).length, 2);
    assert.ok(f.loads.some(name => name.startsWith('./')));
  });
}

for (const file of ['hello/index.js', 'blink/index.js', 'blink/blink.js', 'pwm-fade/index.js']) {
  test(file + ' releases its interval when the stop timer cannot be allocated', () => {
    const f = fixture({ pwm: { init() {}, setDuty() {}, stop() {} } });
    f.board.devices.led = { type: 'managed' };
    f.board.capability = () => ({ pins: [5] });
    f.context.pwmFadePin = 5;
    f.context.setTimeout = () => { throw new Error('No free timer slots'); };
    assert.throws(() => f.run(file), /No free timer slots/);
    assert.equal(f.timers.size, 0, 'failed setup must leave no interval');
  });
}

test('hello replaces its heartbeat on two persistent reruns and finishes at 30s', () => {
  const f = fixture();
  f.run('hello/index.js');
  const old = [...f.timers.keys()];
  f.run('hello/index.js');
  assert.ok(old.every(id => !f.timers.has(id)), 'rerun must cancel old timers');
  assert.equal(f.timers.size, 2);
  const heartbeat = [...f.timers.values()].find(t => t.interval);
  assert.equal(heartbeat.ms, 5000);
  heartbeat.fn();
  assert.ok(f.logs.some(s => s.includes('Heartbeat #1')));
  f.expire(30000);
  assert.equal(f.timers.size, 0);
  assert.ok(f.logs.includes('Demo complete!'));
  f.context.helloStop();
});
