'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

function loadCanvas() {
  const calls = [];
  const native = {
    open(kind, options) {
      assert.equal(this, native);
      assert.equal(kind, 'default');
      assert.deepEqual(Object.keys(options), []);
      let lifecycle, closed = false;
      const backend = {
        maxTouchPoints: 0,
        setLifecycle(callback) { lifecycle = callback; },
        startPointer() { const error = new Error('Touch unavailable'); error.code = 'ENXIO'; throw error; },
        stopPointer() {},
        samplePointer() { throw new Error('Touch is stopped'); },
        getState() { return closed ? 'closed' : 'open'; },
        draw(commands, mode, rgba, lineWidth) {
          assert.equal(this, backend);
          calls.push({ commands: Array.from(commands), mode, rgba: Array.from(rgba), lineWidth });
        },
        close() { assert.equal(this, backend); closed = true; if (lifecycle) lifecycle(); },
      };
      Object.defineProperties(backend, {
        width: { value: 160 },
        height: { value: 120 },
      });
      return backend;
    },
  };
  const module = { exports: {} };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../lib/canvas.js'), 'utf8'), {
    module,
    exports: module.exports,
    require(name) {
      if (name === 'events') return require('../lib/events.js');
      assert.equal(name, 'mcujs:canvas-native');
      return native;
    },
  }, { filename: 'canvas.js' });
  return { canvas: module.exports.canvas, calls, exported: Object.keys(module.exports) };
}

test('exports a fixed hardware canvas and one opaque 2D context', () => {
  const { canvas, calls, exported } = loadCanvas();
  assert.deepEqual(exported.sort(), ['canvas', 'connect', 'display']);
  assert.equal(canvas.width, 160);
  assert.equal(canvas.height, 120);
  assert.throws(() => { canvas.width = 320; }, TypeError);
  assert.throws(() => { canvas.height = 240; }, TypeError);
  const ctx = canvas.getContext('2d');
  assert.equal(canvas.getContext('2d', { alpha: true }), ctx);
  assert.equal(canvas.getContext({ toString: () => '2d' }), ctx);
  assert.equal(canvas.getContext('webgl'), null);
  assert.equal(canvas.getContext('2D'), null);
  assert.equal(ctx.canvas, canvas);
  assert.throws(() => { ctx.canvas = {}; }, TypeError);
  assert.equal(ctx.getContextAttributes().alpha, false);
  ctx.getContextAttributes().alpha = true;
  assert.equal(ctx.getContextAttributes().alpha, false);
  assert.equal(ctx.fillStyle, '#000000');
  assert.equal(ctx.strokeStyle, '#000000');
  assert.equal(ctx.lineWidth, 1);
  assert.deepEqual(calls, []);
  for (const unsupported of ['drawImage', 'strokeText', 'translate', 'rotate', 'scale', 'present']) {
    assert.equal(ctx[unsupported], undefined);
  }
});

test('fillText draws the built-in A glyph at a left alphabetic baseline without changing the path', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  ctx.moveTo(1, 2);
  ctx.lineTo(3, 4);
  ctx.fillStyle = 'red';
  assert.equal(ctx.fillText('A', 0, 7), undefined);
  // Independent 5-by-7 bitmap specification, not production font data.
  const expected = ['.###.', '#...#', '#...#', '#...#', '#####', '#...#', '#...#'];
  const pixels = Array.from({ length: 7 }, () => Array(5).fill('.'));
  for (const call of calls) {
    assert.equal(call.mode, 'fill');
    assert.deepEqual(call.rgba, [1, 0, 0, 1]);
    for (let i = 0; i < call.commands.length; i += 5) {
      const [op, x, y, w, h] = call.commands.slice(i, i + 5);
      assert.equal(op, 4);
      for (let dy = 0; dy < h; dy++) for (let dx = 0; dx < w; dx++) pixels[y + dy][x + dx] = '#';
    }
  }
  assert.deepEqual(pixels.map(row => row.join('')), expected);
  ctx.stroke();
  assert.deepEqual(calls.at(-1).commands, [1, 1, 2, 2, 3, 4]);
});

test('text has bounded bitmap font sizes, width measurement and saved font state', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  assert.equal(ctx.font, '8px monospace');
  assert.equal(ctx.measureText('Hello 123!').width, 60);
  ctx.save();
  ctx.font = '16px monospace';
  assert.equal(ctx.measureText('A A').width, 36);
  ctx.fillText('!', 10, 20);
  assert.deepEqual(calls.map(c => c.commands), [[4, 14, 6, 2, 10, 4, 14, 18, 2, 2]]);
  for (const invalid of ['0px monospace', '9px monospace', '40px monospace', '16px serif', 'bold 16px monospace', null]) {
    ctx.font = invalid;
    assert.equal(ctx.font, '16px monospace');
  }
  ctx.font = '32px monospace';
  assert.equal(ctx.measureText('12').width, 48);
  ctx.restore();
  assert.equal(ctx.font, '8px monospace');
  assert.equal(ctx.measureText('').width, 0);
  assert.deepEqual(Object.keys(ctx.measureText('A')), ['width']);
});

test('text validates before drawing, bounds work and normalizes Canvas whitespace', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  for (const text of ['oké', 'ok\x00', 'ok\x7f', 'A'.repeat(129)]) {
    assert.throws(() => ctx.fillText(text, 0, 7), { name: 'RangeError' });
    assert.throws(() => ctx.measureText(text), { name: 'RangeError' });
  }
  assert.throws(() => ctx.fillText('A'), { name: 'TypeError' });
  assert.throws(() => ctx.measureText(), { name: 'TypeError' });
  assert.throws(() => ctx.fillText(Symbol(), 0, 7), { name: 'TypeError' });
  assert.throws(() => ctx.fillText('A', 0n, 7), { name: 'TypeError' });
  assert.throws(() => ctx.fillText('A', 0, 7, 100), { name: 'RangeError' });
  ctx.fillText('A', NaN, 7);
  ctx.fillText('A', 0, Infinity);
  assert.deepEqual(calls, []);
  assert.equal(ctx.measureText({ toString: () => '12' }).width, 12);
  assert.equal(ctx.measureText(' \t\r\n\f').width, 30);
  assert.equal(ctx.measureText('A'.repeat(128)).width, 768);
  ctx.fillText(' \t\r\n\f', 0, 7);
  assert.deepEqual(calls, []);
  ctx.fillText('A\nA', '0', '7');
  assert.equal(calls.length, 2);
  assert.equal(calls[1].commands[1], 12);
  assert.equal(calls[1].commands[2], 1);
  assert.ok(calls.every(c => c.commands.length <= 128));
  canvas.getContext('2d');
});

test('text clips scaled runs at all edges and skips fully offscreen work', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  ctx.font = '16px monospace';
  ctx.lineWidth = 1000; // Text fill must not depend on stroke width.
  ctx.fillText('!', -5, 3);
  assert.deepEqual(calls.map(c => c.commands), [[4, 0, 1, 1, 2]]);
  assert.equal(calls[0].lineWidth, 1);
  calls.length = 0;
  ctx.fillText('!', 155, 130);
  assert.deepEqual(calls.map(c => c.commands), [[4, 159, 116, 1, 4]]);
  calls.length = 0;
  ctx.fillText('A', 1e100, 0);
  ctx.fillText('A', -1e100, 0);
  ctx.fillText('A', 0, -1e100);
  ctx.fillText('A', 0, 1e100);
  assert.deepEqual(calls, []);
  ctx.font = '8px monospace';
  ctx.fillText('!', -1.5, 7.5);
  assert.deepEqual(calls.map(c => c.commands), [[4, 0.5, 0.5, 1, 5, 4, 0.5, 6.5, 1, 1]]);
});

test('retains paths across fill/stroke, with standard subpath starts and closure', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
  assert.deepEqual(calls, []);
  ctx.lineTo('2.5', -3);
  ctx.closePath(); // A one-point subpath cannot close.
  ctx.lineTo(8, 9);
  ctx.closePath();
  ctx.closePath(); // Closing an already closed subpath is also a no-op.
  ctx.lineTo(10, 11);
  ctx.moveTo(20, 21);
  ctx.rect(30, 31, -5, 6.5);
  ctx.closePath(); // rect already ends in a new one-point subpath.
  ctx.lineTo(40, 41);
  ctx.fill();
  ctx.stroke();
  const commands = [1, 2.5, -3, 2, 8, 9, 3, 2, 10, 11, 1, 20, 21,
    4, 30, 31, -5, 6.5, 2, 40, 41];
  assert.deepEqual(calls, [
    { commands, mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 1 },
    { commands, mode: 'stroke', rgba: [0, 0, 0, 1], lineWidth: 1 },
  ]);
  ctx.beginPath();
  ctx.stroke();
  assert.equal(calls.length, 2);
  ctx.lineTo(1, 2);
  ctx.stroke();
  assert.deepEqual(calls[2].commands, [1, 1, 2]);
});

test('rectangle drawing is immediate, signed and independent of the current path', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  ctx.moveTo(1, 2);
  ctx.lineTo(3, 4);
  assert.equal(ctx.fillRect('10.5', null, -8, 9), undefined);
  ctx.strokeRect(-1, -2, 3.25, -4.5);
  ctx.fillRect(1, 2, 0, 4);
  ctx.fillRect(1, 2, 4, 0);
  ctx.strokeRect(1, 2, 0, 0);
  ctx.strokeRect(1, 2, 0, 4);
  ctx.fill('nonzero');
  assert.deepEqual(calls, [
    { commands: [4, 10.5, 0, -8, 9], mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 1 },
    { commands: [4, -1, -2, 3.25, -4.5], mode: 'stroke', rgba: [0, 0, 0, 1], lineWidth: 1 },
    { commands: [4, 1, 2, 0, 4], mode: 'stroke', rgba: [0, 0, 0, 1], lineWidth: 1 },
    { commands: [1, 1, 2, 2, 3, 4], mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 1 },
  ]);
});

test('drawing converts numeric arguments, ignores nonfinite coordinates and checks arity', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  const converted = [];
  ctx.moveTo({ valueOf() { converted.push('x'); return 1.5; } }, true);
  ctx.lineTo(NaN, { valueOf() { converted.push('y'); return 2; } });
  ctx.moveTo(Infinity, 3);
  ctx.lineTo(4, -Infinity);
  ctx.rect(0, 0, 1, NaN);
  ctx.fillRect(0, 0, Infinity, 1);
  ctx.strokeRect(undefined, 0, 1, 1);
  assert.throws(() => ctx.moveTo(1), { name: 'TypeError' });
  assert.throws(() => ctx.rect(1, 2, 3), { name: 'TypeError' });
  assert.throws(() => ctx.lineTo(Symbol(), 1), { name: 'TypeError' });
  assert.throws(() => ctx.lineTo(1n, 1), { name: 'TypeError' });
  ctx.lineTo(false, '');
  ctx.stroke();
  assert.deepEqual(converted, ['x', 'y']);
  assert.deepEqual(calls, [
    { commands: [1, 1.5, 1, 2, 0, 0], mode: 'stroke', rgba: [0, 0, 0, 1], lineWidth: 1 },
  ]);
});

test('path storage is bounded, rejects atomically and recovers with beginPath', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  for (let i = 0; i < 42; i++) ctx.moveTo(1, 2);
  assert.throws(() => ctx.rect(3, 4, 5, 6), { name: 'RangeError' });
  assert.throws(() => ctx.lineTo(3, 4), { name: 'RangeError' });
  ctx.stroke();
  assert.equal(calls[0].commands.length, 126);
  assert.deepEqual(calls[0].commands.slice(-3), [1, 1, 2]);
  ctx.beginPath();
  ctx.lineTo(7, 8);
  ctx.stroke();
  assert.deepEqual(calls[1].commands, [1, 7, 8]);
  assert.throws(() => ctx.fill('evenodd'), { name: 'RangeError' });
  assert.throws(() => ctx.fill('invalid'), { name: 'TypeError' });
});

test('hex styles normalize to CSS strings and native RGBA without losing alpha', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  const cases = [
    [' #F00 ', '#ff0000', [1, 0, 0, 1]],
    ['#369C', 'rgba(51, 102, 153, 0.8)', [0.2, 0.4, 0.6, 0.8]],
    ['#336699', '#336699', [0.2, 0.4, 0.6, 1]],
    ['#336699cc', 'rgba(51, 102, 153, 0.8)', [0.2, 0.4, 0.6, 0.8]],
    ['#ff000080', 'rgba(255, 0, 0, 0.502)', [1, 0, 0, 128 / 255]],
    ['#f000', 'rgba(255, 0, 0, 0)', [1, 0, 0, 0]],
    ['transparent', 'rgba(0, 0, 0, 0)', [0, 0, 0, 0]],
  ];
  for (const [input, serialized, rgba] of cases) {
    ctx.fillStyle = input;
    ctx.strokeStyle = input;
    assert.equal(ctx.fillStyle, serialized);
    assert.equal(ctx.strokeStyle, serialized);
    ctx.fillRect(1, 2, 3, 4);
    ctx.strokeRect(5, 6, 7, 8);
    assert.deepEqual(calls.splice(0), [
      { commands: [4, 1, 2, 3, 4], mode: 'fill', rgba, lineWidth: 1 },
      { commands: [4, 5, 6, 7, 8], mode: 'stroke', rgba, lineWidth: 1 },
    ]);
  }
});

test('supports all basic sixteen CSS names and ignores invalid style assignments', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  const names = [
    ['black', '#000000'], ['silver', '#c0c0c0'], ['gray', '#808080'],
    ['white', '#ffffff'], ['maroon', '#800000'], ['red', '#ff0000'],
    ['purple', '#800080'], ['fuchsia', '#ff00ff'], ['green', '#008000'],
    ['lime', '#00ff00'], ['olive', '#808000'], ['yellow', '#ffff00'],
    ['navy', '#000080'], ['blue', '#0000ff'], ['teal', '#008080'], ['aqua', '#00ffff'],
  ];
  for (const [name, serialized] of names) {
    ctx.fillStyle = name.toUpperCase();
    assert.equal(ctx.fillStyle, serialized);
  }
  ctx.fillStyle = { toString: () => 'red' };
  ctx.strokeStyle = 'blue';
  for (const invalid of ['', 'not-a-color', '#12', '#12345', '#1234567', '#ggg',
    '#123456789', 'constructor', '__proto__', null, undefined, 12, {}, 'currentColor']) {
    ctx.fillStyle = invalid;
    ctx.strokeStyle = invalid;
    assert.equal(ctx.fillStyle, '#ff0000');
    assert.equal(ctx.strokeStyle, '#0000ff');
  }
  ctx.rect(1, 2, 3, 4);
  ctx.fill();
  ctx.stroke();
  assert.deepEqual(calls, [
    { commands: [4, 1, 2, 3, 4], mode: 'fill', rgba: [1, 0, 0, 1], lineWidth: 1 },
    { commands: [4, 1, 2, 3, 4], mode: 'stroke', rgba: [0, 0, 1, 1], lineWidth: 1 },
  ]);
});

test('legacy comma rgb/rgba colors clamp channels and allow style getter round trips', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  for (const [input, serialized, rgba] of [
    ['rgb(255, 0, 153)', '#ff0099', [1, 0, 0.6, 1]],
    ['rgba(100%, 0%, 60%, 25%)', 'rgba(255, 0, 153, 0.25)', [1, 0, 0.6, 0.25]],
    ['rgba(300, -2, 0, 2)', '#ff0000', [1, 0, 0, 1]],
    ['rgba(51, 102, 153, .8)', 'rgba(51, 102, 153, 0.8)', [0.2, 0.4, 0.6, 0.8]],
  ]) {
    ctx.fillStyle = input;
    assert.equal(ctx.fillStyle, serialized);
    ctx.strokeStyle = ctx.fillStyle;
    ctx.strokeRect(1, 2, 3, 4);
    assert.deepEqual(calls.pop(), {
      commands: [4, 1, 2, 3, 4], mode: 'stroke', rgba, lineWidth: 1,
    });
  }
  for (const invalid of ['rgb(1,2)', 'rgba(1,2,3,NaN)', 'rgb(1%,2,3)',
    'rgb(1 2 3)', 'rgba(1,2,3,4)junk', 'rgb(1,,3)', 'rgb(1,2,3,4)']) {
    ctx.fillStyle = invalid;
    assert.equal(ctx.fillStyle, 'rgba(51, 102, 153, 0.8)');
  }
});

test('lineWidth uses numeric conversion and ignores nonpositive or nonfinite values', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  ctx.lineWidth = '2.5';
  for (const invalid of [0, -1, NaN, Infinity, -Infinity, 'bad', undefined, null]) {
    ctx.lineWidth = invalid;
    assert.equal(ctx.lineWidth, 2.5);
  }
  assert.throws(() => { ctx.lineWidth = Symbol(); }, { name: 'TypeError' });
  assert.throws(() => { ctx.lineWidth = 2n; }, { name: 'TypeError' });
  ctx.strokeRect(0, 0, 1, 1);
  assert.deepEqual(calls, [
    { commands: [4, 0, 0, 1, 1], mode: 'stroke', rgba: [0, 0, 0, 1], lineWidth: 2.5 },
  ]);
});

test('clearRect paints opaque black without changing styles or the retained path', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#f008';
  ctx.strokeStyle = 'blue';
  ctx.lineWidth = 4;
  ctx.moveTo(1, 2);
  ctx.lineTo(3, 4);
  ctx.clearRect('10.5', -2, -3, 4.25);
  ctx.clearRect(1, 2, 0, 4);
  ctx.clearRect(1, 2, 3, 0);
  ctx.clearRect(Infinity, 2, 3, 4);
  assert.throws(() => ctx.clearRect(1), { name: 'TypeError' });
  assert.equal(ctx.fillStyle, 'rgba(255, 0, 0, 0.533)');
  assert.equal(ctx.strokeStyle, '#0000ff');
  assert.equal(ctx.lineWidth, 4);
  ctx.fill();
  ctx.stroke();
  assert.deepEqual(calls, [
    { commands: [4, 10.5, -2, -3, 4.25], mode: 'fill', rgba: [0, 0, 0, 1], lineWidth: 4 },
    { commands: [1, 1, 2, 2, 3, 4], mode: 'fill', rgba: [1, 0, 0, 136 / 255], lineWidth: 4 },
    { commands: [1, 1, 2, 2, 3, 4], mode: 'stroke', rgba: [0, 0, 1, 1], lineWidth: 4 },
  ]);
});

test('save/restore is LIFO drawing state only, never a saved path or draw operation', () => {
  const { canvas, calls } = loadCanvas();
  const ctx = canvas.getContext('2d');
  ctx.restore(); // Empty stack does nothing.
  ctx.save();
  ctx.fillStyle = '#f008';
  ctx.strokeStyle = 'blue';
  ctx.lineWidth = 2.5;
  ctx.moveTo(1, 2);
  ctx.save();
  ctx.fillStyle = 'lime';
  ctx.strokeStyle = 'yellow';
  ctx.lineWidth = 9;
  ctx.lineTo(3, 4);
  ctx.restore();
  assert.equal(ctx.fillStyle, 'rgba(255, 0, 0, 0.533)');
  assert.equal(ctx.strokeStyle, '#0000ff');
  assert.equal(ctx.lineWidth, 2.5);
  assert.deepEqual(calls, []);
  ctx.fill();
  ctx.stroke();
  ctx.restore();
  ctx.restore();
  assert.equal(ctx.fillStyle, '#000000');
  assert.equal(ctx.strokeStyle, '#000000');
  assert.equal(ctx.lineWidth, 1);
  ctx.stroke();
  ctx.save();
  ctx.beginPath();
  ctx.restore();
  ctx.stroke();
  assert.deepEqual(calls, [
    { commands: [1, 1, 2, 2, 3, 4], mode: 'fill', rgba: [1, 0, 0, 136 / 255], lineWidth: 2.5 },
    { commands: [1, 1, 2, 2, 3, 4], mode: 'stroke', rgba: [0, 0, 1, 1], lineWidth: 2.5 },
    { commands: [1, 1, 2, 2, 3, 4], mode: 'stroke', rgba: [0, 0, 0, 1], lineWidth: 1 },
  ]);
});
