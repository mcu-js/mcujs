const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const { join } = require("node:path");
const test = require("node:test");
const vm = require("node:vm");
const { boardDescriptors } = require("../runtime/board-registry.js");

// Real example JS with simulated input/time, not firmware or electrical proof.
function demo(boardId) {
  const timers = new Map();
  const writes = [];
  const logs = [];
  let now = 0;
  let pressed = false;
  let nextId = 1;
  const board = { devices: boardDescriptors[boardId].board.devices,
    capability: name => boardDescriptors[boardId].capabilities[name], millis: () => now >>> 0 };
  const modules = {};
  function requireModule(name) {
    if (name === 'board') return board;
    if (!modules[name]) {
      const module = {};
      const filename = name === 'mcujs:button' ? 'button' : name;
      vm.runInNewContext(readFileSync(join(__dirname, '../lib/' + filename + '.js'), 'utf8'), {
        module, require: requireModule, console: { error(...parts) { logs.push(parts.join(' ')); } },
        setInterval: (fn, delay) => timer(fn, delay, true), clearInterval: id => timers.delete(id),
      });
      modules[name] = module.exports;
    }
    return modules[name];
  }
  if (board.devices.button) board.buttonPressed = function () {
    assert.equal(arguments.length, 0);
    return pressed;
  };
  if (board.devices.led) board.led = (value) => {
    assert.equal(typeof value, "boolean");
    writes.push([now, value]);
  };
  function timer(callback, delay, repeat) {
    assert.ok(delay > 0 && delay <= 60000);
    const id = nextId++;
    timers.set(id, { callback, delay, repeat, due: now + delay });
    return id;
  }
  return {
    timers, writes, logs,
    press(value) { pressed = value; },
    run() {
      vm.runInNewContext(readFileSync(join(__dirname, "../examples/onboard-button/index.js"), "utf8"), {
        require: requireModule,
        console: { log(...parts) { logs.push(parts.join(" ")); } },
        setInterval: (fn, delay) => timer(fn, delay, true),
        setTimeout: (fn, delay) => timer(fn, delay, false),
        clearInterval: (id) => timers.delete(id),
        clearTimeout: (id) => timers.delete(id),
      });
    },
    advance(ms) {
      const end = now + ms;
      for (;;) {
        const next = [...timers].sort((a, b) => a[1].due - b[1].due || a[0] - b[0])[0];
        if (!next || next[1].due > end) break;
        const [id, entry] = next;
        now = entry.due;
        if (entry.repeat) entry.due += entry.delay;
        else timers.delete(id);
        entry.callback();
      }
      now = end;
    },
  };
}

for (const boardId of ["pico", "seeed_xiao_esp32s3"]) {
  test(`${boardId}: same no-wiring demo debounces both edges and stops at 60s`, () => {
    const runtime = demo(boardId);
    runtime.run();
    assert.deepEqual(runtime.writes[0], [0, false]);
    runtime.advance(100);
    runtime.press(true);
    runtime.advance(10);
    runtime.press(false);
    runtime.advance(10);
    assert.equal(runtime.writes.some(([, value]) => value), false, "bounce must not light LED");
    runtime.press(true);
    runtime.advance(40);
    assert.equal(runtime.writes.at(-1)[1], true);
    const writeCount = runtime.writes.length;
    runtime.advance(100);
    assert.equal(runtime.writes.length, writeCount, "no duplicate writes while held");
    runtime.press(false);
    runtime.advance(10);
    runtime.press(true);
    runtime.advance(10);
    assert.equal(runtime.writes.at(-1)[1], true, "release bounce ignored");
    runtime.press(false);
    runtime.advance(40);
    assert.equal(runtime.writes.at(-1)[1], false);
    assert.deepEqual(runtime.logs.filter((line) => /^Button:/.test(line)), [
      "Button: released", "Button: pressed", "Button: released",
    ]);
    runtime.press(true);
    runtime.advance(59679); // 59999ms elapsed: demo still running.
    assert.ok(runtime.timers.size > 0);
    runtime.advance(1);
    assert.deepEqual(runtime.writes.at(-1), [60000, false]);
    assert.equal(runtime.timers.size, 0);
    assert.equal(runtime.logs.at(-1), "Demo complete!");
    const count = runtime.writes.length;
    runtime.advance(60000);
    assert.equal(runtime.writes.length, count);
  });
}

test("unsupported board exits without timers or writes", () => {
  const runtime = demo("pico2");
  runtime.run();
  assert.equal(runtime.timers.size, 0);
  assert.deepEqual(runtime.writes, []);
  assert.match(runtime.logs.join("\n"), /not supported/i);
});
