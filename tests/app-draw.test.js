'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const draw = require('../examples/portable/pointer-draw/draw.js');
const source = fs.readFileSync(path.join(__dirname, '../examples/portable/app-draw/index.js'), 'utf8');
function run(files, failWrite = false) {
  const handlers = {}, context = {}, state = { closed: 0, started: 0, frames: 0, logs: [] };
  ['fillRect', 'beginPath', 'moveTo', 'lineTo', 'stroke'].forEach(k => { context[k] = () => {}; });
  const display = { canvas: { width: 240, height: 280, maxTouchPoints: 1,
    getContext: () => context, addEventListener: (k, v) => { handlers[k] = v; },
    removeEventListener: k => { delete handlers[k]; } },
    present: () => { state.frames++; }, startPointer: () => { state.started++; },
    close: () => { state.closed++; } };
  const fakeFS = { existsSync: p => files.has(p), readFileSync: p => files.get(p),
    writeFileSync: (p, s) => { if (failWrite) throw Object.assign(new Error('USB host owns storage'), { code: 'EBUSY' }); files.set(p, s); } };
  const sandbox = { require: n => ({ fs: fakeFS, devices: { display: { open: () => display } }, './draw': draw })[n],
    console: { log: s => state.logs.push(s), error: () => {} },
    setTimeout: (f, ms) => { assert.equal(ms, 60000); state.finish = f; return 1; }, clearTimeout: () => {} };
  try { vm.runInNewContext(source, sandbox); } catch (error) { state.error = error; }
  return { state, context, handlers };
}
test('application entry persists settings across fresh runs and applies saved ink', () => {
  const files = new Map();
  const first = run(files);
  assert.equal(first.state.error, undefined);
  assert.equal(first.context.strokeStyle, 'lime');
  assert.equal(JSON.parse(files.get('/app/settings.json')).starts, 1);
  first.state.finish(); assert.equal(first.state.closed, 1);
  files.set('/app/settings.json', JSON.stringify({ color: 'aqua', lineWidth: 5, starts: 1 }));
  const second = run(files);
  assert.equal(second.context.strokeStyle, 'aqua');
  assert.equal(second.context.lineWidth, 5);
  assert.equal(second.state.started, 1);
  assert.equal(JSON.parse(files.get('/app/settings.json')).starts, 2);
  second.state.finish(); assert.equal(second.state.closed, 1);
});
test('malformed settings are not silently replaced', () => {
  const files = new Map([['/app/settings.json', '{broken']]);
  const result = run(files);
  assert.ok(result.state.error);
  assert.equal(files.get('/app/settings.json'), '{broken');
  assert.equal(result.state.started, 0);
});
test('write failure closes the display and never pretends settings were saved', () => {
  const files = new Map(); const result = run(files, true);
  assert.equal(result.state.error.code, 'EBUSY');
  assert.equal(files.size, 0); assert.equal(result.state.closed, 1); assert.equal(result.state.started, 0);
});
