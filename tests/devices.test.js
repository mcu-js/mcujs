'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

function fixture(supported = true) {
  const calls = [];
  let active = false;
  const capability = Object.freeze({ interface: 'canvas-2d-subset', maxOpenHandles: 1 });
  const native = {
    defaultState() { return active ? 'busy' : 'idle'; },
    open(kind, options) {
      calls.push(['open', kind, options]);
      if (active) { const e = new Error('Display busy'); e.code = 'EBUSY'; throw e; }
      active = true;
      let state = 'open';
      return {
        width: 200, height: 200,
        getState() { return state; },
        draw() { calls.push(['draw']); },
        present() { calls.push(['present']); },
        close() { calls.push(['close']); active = false; state = 'closed'; },
      };
    },
  };
  const modules = {
    board: { capability(name) {
      calls.push(['capability', name]);
      assert.equal(name, 'devices');
      return supported ? Object.freeze({ display: capability }) : undefined;
    } },
    'mcujs:canvas-native': native,
  };
  function requireModule(name) {
    if (!Object.hasOwn(modules, name)) {
      calls.push(['load', name]);
      const module = { exports: {} };
      vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../lib', name + '.js'), 'utf8'),
        { module, require: requireModule }, { filename: name + '.js' });
      modules[name] = module.exports;
    }
    return modules[name];
  }
  return { devices: requireModule('devices'), calls, native, capability };
}

test('unsupported device handles are absent, even if a physical board has a display', () => {
  const { devices, calls } = fixture(false);
  assert.equal(devices.display, undefined);
  assert.equal(Object.keys(devices).length, 0);
  assert.equal(Object.isFrozen(devices), true);
  assert.deepEqual(calls, [['load', 'devices'], ['capability', 'devices']]);
});

test('discovery is stable and frozen; capabilities and state do not open or load Canvas', () => {
  const { devices, calls, capability } = fixture();
  assert.equal(devices.display, devices.display);
  assert.equal(Object.isFrozen(devices.display), true);
  assert.equal(devices.display.capabilities, capability);
  assert.equal(devices.display.state, 'idle');
  assert.deepEqual(calls, [['load', 'devices'], ['capability', 'devices']]);
});

test('configured display opens lazily without exposing wiring options', () => {
  const { devices, calls } = fixture();
  assert.throws(() => devices.display.open({ sck: 18 }), { name: 'TypeError' });
  assert.equal(calls.some(c => c[0] === 'open'), false);
  const handle = devices.display.open();
  assert.equal(handle.canvas.width, 200);
  assert.equal(handle.canvas, handle.canvas);
  assert.equal(devices.display.state, 'busy');
  assert.throws(() => devices.display.open(), { code: 'EBUSY' });
  handle.close();
  handle.close();
  assert.equal(calls.filter(c => c[0] === 'close').length, 1);
  assert.equal(devices.display.state, 'idle');
  const reopened = devices.display.open();
  assert.notEqual(reopened, handle);
  handle.close();
  assert.equal(devices.display.state, 'busy');
  reopened.close();
});

test('owned display explicitly presents and rejects use after close', () => {
  const { devices, calls } = fixture();
  const handle = devices.display.open();
  assert.equal(handle.state, 'open');
  const ctx = handle.canvas.getContext('2d');
  ctx.fillRect(0, 0, 8, 8);
  handle.present();
  assert.deepEqual(calls.filter(c => c[0] === 'draw' || c[0] === 'present'), [['draw'], ['present']]);
  handle.close();
  assert.equal(handle.state, 'closed');
  assert.throws(() => handle.present(), { code: 'ENXIO' });
  assert.throws(() => ctx.fillRect(0, 0, 1, 1), { code: 'ENXIO' });
});

module.exports = { fixture };
