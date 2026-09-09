'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

function loadModule(filename, requireModule) {
  const module = { exports: {} };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../lib', filename), 'utf8'), {
    module,
    exports: module.exports,
    require: requireModule,
  }, { filename });
  return module.exports;
}

function loadCanvas() {
  const opens = [];
  const native = {
    open(kind, options) {
      assert.equal(this, native);
      const record = { kind, options, calls: [], closes: 0 };
      const backend = {
        draw(commands, mode, rgba, lineWidth) {
          assert.equal(this, backend);
          assert.equal(record.closes, 0, 'drawing must not reach a closed backend');
          record.calls.push({ commands: Array.from(commands), mode, rgba: Array.from(rgba), lineWidth });
        },
        close() {
          assert.equal(this, backend);
          record.closes++;
        },
      };
      Object.defineProperties(backend, {
        width: { value: kind === 'default' ? 160 : 240 },
        height: { value: kind === 'default' ? 120 : 320 },
      });
      opens.push(record);
      return backend;
    },
    stats() { throw new Error('stats are private diagnostics'); },
  };
  const api = loadModule('canvas.js', name => {
    assert.equal(name, 'mcujs:canvas-native');
    return native;
  });
  return { api, opens };
}

test('connections isolate drawing destinations, styles, paths and save stacks', () => {
  const { api, opens } = loadCanvas();
  const a = api.connect('st7789');
  const x = a.canvas.getContext('2d');
  x.fillStyle = '#f008';
  x.strokeStyle = 'blue';
  x.lineWidth = 2.5;
  x.save();
  x.moveTo(1, 2);
  x.lineTo(3, 4);
  const b = api.connect('st7789');
  const y = b.canvas.getContext('2d');
  assert.notEqual(a, b);
  assert.notEqual(x, y);
  assert.equal(a.canvas.getContext('2d'), x);
  assert.equal(b.canvas.getContext('2d'), y);
  assert.equal(x.canvas, a.canvas);
  assert.equal(y.canvas, b.canvas);
  assert.equal(y.fillStyle, '#000000');
  assert.equal(y.strokeStyle, '#000000');
  assert.equal(y.lineWidth, 1);
  y.restore(); // Must not pop x's saved state.
  y.save();
  y.fillStyle = 'lime';
  y.strokeStyle = 'yellow';
  y.lineWidth = 7;
  y.lineTo(10, 11); // x's subpath must not turn this into a line opcode.
  y.closePath();
  y.lineTo(12, 13);
  x.fillStyle = 'red';
  x.strokeStyle = 'white';
  x.lineWidth = 9;
  x.restore(); // Must pop x's state, not y's.
  x.fill();
  x.stroke();
  y.stroke();
  x.beginPath();
  y.fill(); // x.beginPath must not clear y's path.
  y.restore();
  y.fillRect(20, 21, 22, 23);
  x.clearRect(5, 6, 7, 8);
  assert.deepEqual(opens[0].calls, [
    { commands: [1, 1, 2, 2, 3, 4], mode: 'fill', rgba: [1, 0, 0, 136 / 255], lineWidth: 2.5 },
    { commands: [1, 1, 2, 2, 3, 4], mode: 'stroke', rgba: [0, 0, 1, 1], lineWidth: 2.5 },
    { commands: [4, 5, 6, 7, 8], mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 2.5 },
  ]);
  assert.deepEqual(opens[1].calls, [
    { commands: [1, 10, 11, 2, 12, 13], mode: 'stroke', rgba: [1, 1, 0, 1], lineWidth: 7 },
    { commands: [1, 10, 11, 2, 12, 13], mode: 'fill', rgba: [0, 1, 0, 1], lineWidth: 7 },
    { commands: [4, 20, 21, 22, 23], mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 1 },
  ]);
  for (let i = 0; i < 42; i++) x.moveTo(1, 2);
  assert.throws(() => x.lineTo(3, 4), { name: 'RangeError' });
  y.lineTo(14, 15);
  y.stroke();
  assert.deepEqual(opens[1].calls[3].commands, [1, 10, 11, 2, 12, 13, 2, 14, 15]);
  assert.equal(opens.length, 2); // No default hardware opened.
});

test('close is idempotent, retains identity and prevents all drawing on only that display', () => {
  const { api, opens } = loadCanvas();
  const a = api.display;
  const b = api.connect('st7789');
  const x = a.canvas.getContext('2d');
  const y = b.canvas.getContext('2d');
  x.rect(1, 2, 3, 4);
  y.rect(5, 6, 7, 8);
  x.fill();
  assert.equal(a.close(), undefined);
  assert.equal(a.close(), undefined);
  assert.equal(opens[0].closes, 1);
  assert.equal(opens[1].closes, 0);
  assert.equal(api.display, a);
  assert.equal(api.canvas, a.canvas);
  assert.equal(a.canvas.getContext('2d'), x);
  assert.equal(a.canvas.width, 160);
  assert.equal(a.canvas.height, 120);
  for (const draw of [
    () => x.fill(), () => x.stroke(),
    () => x.fillRect(1, 2, 3, 4), () => x.strokeRect(1, 2, 3, 4),
    () => x.clearRect(1, 2, 3, 4),
  ]) assert.throws(draw, /closed/i);
  x.beginPath();
  assert.throws(() => x.fill(), /closed/i);
  assert.throws(() => x.stroke(), /closed/i);
  assert.throws(() => x.fillRect(1, 2, 0, 4), /closed/i);
  assert.equal(opens[0].calls.length, 1);
  y.fill();
  assert.deepEqual(opens[1].calls, [
    { commands: [4, 5, 6, 7, 8], mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 1 },
  ]);
  b.close();
  b.close();
  assert.equal(opens[1].closes, 1);
  assert.equal(opens.length, 2);
});

test('ST7789 connector forwards options and returns per-display canvases without opening default', () => {
  const { api, opens } = loadCanvas();
  const driver = loadModule('displays/st7789.js', name => {
    assert.equal(name, 'canvas');
    return api;
  });
  assert.deepEqual(Object.keys(driver), ['connect']);
  assert.equal(opens.length, 0);
  const options = { width: 240, height: 320, spi: 0, cs: 17, dc: 16 };
  const a = driver.connect(options);
  const b = driver.connect();
  assert.notEqual(a.canvas, b.canvas);
  assert.equal(a.canvas.width, 240);
  assert.equal(a.canvas.height, 320);
  assert.equal(opens.length, 2);
  assert.equal(opens[0].kind, 'st7789');
  assert.equal(opens[0].options, options);
  assert.equal(opens[1].kind, 'st7789');
  assert.deepEqual(Object.keys(opens[1].options), []);
  a.canvas.getContext('2d').fillRect(1, 2, 3, 4);
  assert.deepEqual(opens[0].calls, [
    { commands: [4, 1, 2, 3, 4], mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 1 },
  ]);
  assert.deepEqual(opens[1].calls, []);
  a.close();
  b.close();
  assert.equal(opens[0].closes, 1);
  assert.equal(opens[1].closes, 1);
});

test('native driver and option errors propagate unchanged and failed default opens are not cached', () => {
  const calls = [];
  const failure = new Error('native driver/options unavailable');
  const native = {
    open(kind, options) {
      assert.equal(this, native);
      calls.push({ kind, options });
      throw failure;
    },
  };
  const api = loadModule('canvas.js', name => {
    assert.equal(name, 'mcujs:canvas-native');
    return native;
  });
  assert.deepEqual(calls, []);
  const invalid = { bus: 'i2c' };
  for (const [kind, options] of [['unsupported', invalid], ['st7789', null], ['default', undefined]]) {
    assert.throws(() => api.connect(kind, options), error => error === failure);
    const call = calls[calls.length - 1];
    assert.equal(call.kind, kind);
    if (options === undefined) assert.deepEqual(Object.keys(call.options), []);
    else assert.equal(call.options, options);
  }
  assert.throws(() => api.canvas, error => error === failure);
  assert.throws(() => api.display, error => error === failure);
  assert.equal(calls.length, 5);
  assert.equal(calls[3].kind, 'default');
  assert.equal(calls[4].kind, 'default');
  assert.notEqual(calls[3].options, calls[4].options);
  const driver = loadModule('displays/st7789.js', name => {
    assert.equal(name, 'canvas');
    return api;
  });
  assert.throws(() => driver.connect(invalid), error => error === failure);
  assert.equal(calls[5].kind, 'st7789');
  assert.equal(calls[5].options, invalid);
});

for (const first of ['canvas', 'display']) {
  test(`default display is lazy and cached when accessing ${first} first`, () => {
    const { api, opens } = loadCanvas();
    assert.equal(opens.length, 0);
    assert.deepEqual(Object.keys(api).sort(), ['canvas', 'connect', 'display']);
    const options = { width: 240, height: 320 };
    const explicit = api.connect('st7789', options);
    assert.equal(opens.length, 1);
    assert.equal(opens[0].kind, 'st7789');
    assert.equal(opens[0].options, options);
    const initial = api[first];
    const display = api.display;
    assert.equal(initial, first === 'canvas' ? display.canvas : display);
    assert.equal(api.display, display);
    assert.equal(api.canvas, display.canvas);
    assert.equal(api.canvas, api.canvas);
    assert.equal(opens.length, 2);
    assert.equal(opens[1].kind, 'default');
    assert.deepEqual(Object.keys(opens[1].options), []);
    assert.notEqual(explicit, display);
    assert.notEqual(explicit.canvas, display.canvas);
    assert.notEqual(explicit.canvas.getContext('2d'), display.canvas.getContext('2d'));
    assert.equal(explicit.canvas.width, 240);
    assert.equal(explicit.canvas.height, 320);
    assert.equal(display.canvas.width, 160);
    assert.equal(display.canvas.height, 120);
    assert.throws(() => { display.canvas = explicit.canvas; }, TypeError);
    assert.throws(() => { explicit.canvas.width = 1; }, TypeError);
    assert.throws(() => { explicit.canvas.height = 1; }, TypeError);
    assert.throws(() => { api.canvas = explicit.canvas; }, TypeError);
    assert.throws(() => { api.display = explicit; }, TypeError);
    for (const object of [api, display, display.canvas, display.canvas.getContext('2d')]) {
      for (const unsupported of ['stats', 'framebuffer', 'pointer', 'present', 'flush']) {
        assert.equal(object[unsupported], undefined);
      }
    }
  });
}
