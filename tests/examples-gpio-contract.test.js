const { readdirSync, readFileSync } = require("node:fs");
const { join, relative } = require("node:path");
const test = require("node:test");
const assert = require("node:assert/strict");
const { boardDescriptors } = require("../runtime/board-registry.js");
const vm = require("node:vm");

// Host-only execution: real example JS, simulated clock and GPIO/board SDK.
// This is not JerryScript, native binding, electrical, or device evidence.
function blinkRuntime(boardId, { initError } = {}) {
  const descriptor = boardDescriptors[boardId];
  const timers = new Map();
  const writes = [];
  const logs = [];
  const imports = [];
  let now = 0;
  let nextId = 1;
  let initialized = false;
  function timer(callback, delay, repeat) {
    const id = nextId++;
    timers.set(id, { callback, due: now + delay, delay, repeat });
    return id;
  }
  const gpio = {
    OUTPUT: 0,
    init(pin, mode) {
      assert.equal(mode, 0);
      assert.ok(descriptor.capabilities.gpio.outputPins.includes(pin));
      if (initError) throw initError;
      initialized = true;
    },
    set(pin, value) {
      assert.ok(initialized, "GPIO must be initialized before writing");
      assert.equal(pin, descriptor.board.devices.led.pin);
      assert.equal(typeof value, "boolean");
      writes.push([now, pin, value]);
    },
  };
  const context = vm.createContext({
    require(name) {
      imports.push(name);
      if (name === "board") return {
        devices: descriptor.board.devices,
        led(value) {
          assert.equal(descriptor.board.devices.led.type, "managed");
          assert.equal(typeof value, "boolean");
          writes.push([now, "managed", value]);
        },
      };
      if (name === "gpio") return gpio;
      throw new Error(`unexpected module: ${name}`);
    },
    console: { log(...parts) { logs.push(parts.join(" ")); } },
    setInterval: (callback, delay) => timer(callback, delay, true),
    setTimeout: (callback, delay) => timer(callback, delay, false),
    clearInterval: (id) => timers.delete(id),
    clearTimeout: (id) => timers.delete(id),
  });
  return {
    writes, logs, imports, timers, context,
    run(filename, edit = (source) => source) {
      const path = join(__dirname, "../examples/blink", filename);
      vm.runInContext(edit(readFileSync(path, "utf8")), context, { filename: path });
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

for (const filename of ["index.js", "blink.js"]) {
  for (const [boardId, pin, off, on] of [
    ["pico", 25, false, true],
    ["seeed_xiao_esp32s3", 21, true, false],
  ]) {
    test(`simulated blink ${filename} on ${boardId}: transitions and 30s stop`, () => {
      const runtime = blinkRuntime(boardId);
      runtime.run(filename);
      assert.deepEqual(runtime.writes, [[0, pin, off]]);
      runtime.advance(499);
      assert.equal(runtime.writes.length, 1);
      runtime.advance(1);
      runtime.advance(500);
      assert.deepEqual(runtime.writes, [[0, pin, off], [500, pin, on], [1000, pin, off]]);
      runtime.advance(28999);
      assert.equal(runtime.logs.includes("Demo complete!"), false);
      runtime.advance(1);
      assert.deepEqual(runtime.writes.at(-1), [30000, pin, off]);
      assert.equal(runtime.logs.at(-1), "Demo complete!");
      assert.equal(runtime.timers.size, 0);
      const count = runtime.writes.length;
      runtime.advance(1000);
      assert.equal(runtime.writes.length, count);
      runtime.run(filename);
      runtime.advance(500);
      assert.deepEqual(runtime.writes.at(-1), [31500, pin, on]);
    });

    test(`simulated blink ${filename} on ${boardId}: one timing edit updates output and message`, () => {
      const runtime = blinkRuntime(boardId);
      runtime.run(filename, (source) => source.replace("var blinkPeriodMs = 500;", "var blinkPeriodMs = 250;"));
      runtime.advance(249);
      assert.deepEqual(runtime.writes, [[0, pin, off]]);
      runtime.advance(1);
      assert.deepEqual(runtime.writes.at(-1), [250, pin, on]);
      runtime.advance(250);
      assert.deepEqual(runtime.writes.at(-1), [500, pin, off]);
      assert.equal(runtime.logs[0], "Blinking onboard LED every 250 ms.");
      runtime.advance(29500);
      assert.equal(runtime.timers.size, 0);
      assert.deepEqual(runtime.writes.at(-1), [30000, pin, off]);
    });

    test(`simulated blink ${filename} on ${boardId}: rerun replaces both timers`, () => {
      const runtime = blinkRuntime(boardId);
      runtime.run(filename);
      runtime.advance(750);
      const oldTimers = [...runtime.timers.keys()];
      runtime.run(filename);
      assert.equal(runtime.timers.size, 2);
      for (const id of oldTimers) assert.equal(runtime.timers.has(id), false);
      assert.deepEqual(runtime.writes.at(-1), [750, pin, off]);
      runtime.advance(500);
      assert.deepEqual(runtime.writes.slice(-2), [[750, pin, off], [1250, pin, on]]);
      runtime.advance(28750);
      assert.equal(runtime.logs.includes("Demo complete!"), false, "old deadline must not stop the new run");
      runtime.advance(750);
      assert.equal(runtime.logs.filter((line) => line === "Demo complete!").length, 1);
      assert.equal(runtime.timers.size, 0);
      assert.deepEqual(runtime.writes.at(-1), [30750, pin, off]);
    });
  }

  test(`simulated blink ${filename}: managed LED and absent onboard LED`, () => {
    const managed = blinkRuntime("pico2_w");
    managed.run(filename);
    managed.advance(1000);
    assert.deepEqual(managed.writes, [[0, "managed", false], [500, "managed", true], [1000, "managed", false]]);
    assert.deepEqual(managed.imports, ["board"]);
    managed.advance(29000);
    assert.equal(managed.timers.size, 0);
    assert.deepEqual(managed.writes.at(-1), [30000, "managed", false]);
    const absent = blinkRuntime("waveshare_rp2040_pizero");
    absent.run(filename);
    absent.advance(30000);
    assert.deepEqual(absent.logs, ["This board has no onboard LED."]);
    assert.deepEqual(absent.imports, ["board"]);
    assert.deepEqual(absent.writes, []);
    assert.equal(absent.timers.size, 0);
  });

  test(`simulated blink ${filename}: GPIO ownership error propagates before timers or writes`, () => {
    // Inject the public error shape; do not claim to test native pin arbitration.
    const error = Object.assign(new Error("GPIO pin is owned by another peripheral"), {
      name: "ResourceBusyError", code: "EBUSY", resource: "gpio", pin: 21,
    });
    const runtime = blinkRuntime("seeed_xiao_esp32s3", { initError: error });
    assert.throws(() => runtime.run(filename), (thrown) => thrown === error);
    assert.deepEqual(runtime.writes, []);
    assert.deepEqual(runtime.logs, []);
    assert.equal(runtime.timers.size, 0);
  });
}

const root = join(__dirname, "..");
const examplesRoot = join(root, "examples");
const rp2040PiZeroRoadmap = join(
  root,
  "board/waveshare_rp2040_pizero/waveshare_rp2040_pizero_phases.md",
);

function javascriptFiles(directory) {
  return readdirSync(directory, { withFileTypes: true })
    .flatMap((entry) => {
      const path = join(directory, entry.name);
      if (entry.isDirectory()) return javascriptFiles(path);
      return entry.isFile() && entry.name.endsWith(".js") ? [path] : [];
    })
    .sort();
}

function lineFor(source, index) {
  return source.slice(0, index).split("\n").length;
}

function collectMatches(path, source, expression, message) {
  const matches = [];
  for (const match of source.matchAll(expression)) {
    matches.push(
      `${relative(root, path)}:${lineFor(source, match.index)}: ${message}: ${match[0]}`,
    );
  }
  return matches;
}

test("examples use the strict boolean GPIO contract", () => {
  const failures = [];
  for (const path of javascriptFiles(examplesRoot)) {
    const source = readFileSync(path, "utf8");
    failures.push(
      ...collectMatches(
        path,
        source,
        /GPIO\.set\s*\([^,\n]+,\s*[-+]?(?:\d+(?:\.\d+)?|\.\d+)\s*\)/g,
        "GPIO.set numeric literal must be boolean",
      ),
      ...collectMatches(
        path,
        source,
        /GPIO\.set\s*\([^,\n]+,\s*[^\n?]+\?\s*[01]\s*:\s*[01]\s*\)/g,
        "GPIO.set numeric ternary must produce boolean",
      ),
      ...collectMatches(
        path,
        source,
        /GPIO\.get\s*\([^\n)]*\)\s*(?:===?|!==?)\s*[01]\b/g,
        "GPIO.get returns boolean and must not be compared with a number",
      ),
      ...collectMatches(
        path,
        source,
        /\b[01]\s*(?:===?|!==?)\s*GPIO\.get\s*\([^\n)]*\)/g,
        "GPIO.get returns boolean and must not be compared with a number",
      ),
    );
  }

  assert.deepEqual(failures, [], failures.join("\n"));
});

test("RP2040-PiZero guidance feature-detects its absent onboard LED", () => {
  const source = readFileSync(rp2040PiZeroRoadmap, "utf8");
  const guidance = source.split("\n").find((line) => line.includes("No onboard LED"));

  assert.ok(guidance, "roadmap must document the missing onboard LED");
  assert.match(guidance, /`board\.led` is absent/i);
  assert.match(guidance, /feature-detect[^\n]*`board\.devices\.led`/i);
  assert.match(guidance, /`board\.capabilities\.gpio`[^\n]*`board\.exposedPins`/i);
  assert.match(guidance, /never expect a graceful no-op/i);
  assert.doesNotMatch(guidance, /use `board\.led\(\)` returns gracefully/i);
});

const displayBoards = Object.freeze({
  "waveshare-lcd-1.28": {
    boardId: "waveshare_rp2040_touch_lcd_1.28",
    peripheralClockHz: 125000000,
  },
  "waveshare-lcd-1.47": {
    boardId: "waveshare_rp2350_lcd_1.47_a",
    peripheralClockHz: 150000000,
  },
  "waveshare-lcd-1.69": {
    boardId: "waveshare_rp2350_touch_lcd_1.69",
    peripheralClockHz: 150000000,
  },
});

function actualRpSpiRate(peripheralClockHz, requestedHz) {
  let best = 0;
  for (let prescale = 2; prescale <= 254; prescale += 2) {
    for (let postdiv = 1; postdiv <= 256; postdiv += 1) {
      const rate = Math.floor(peripheralClockHz / (prescale * postdiv));
      if (rate <= requestedHz && rate > best) best = rate;
    }
  }
  return best;
}

function actualRpI2cRate(peripheralClockHz, requestedHz) {
  const period = Math.floor((peripheralClockHz + Math.floor(requestedHz / 2)) / requestedHz);
  return Math.floor(peripheralClockHz / period);
}

function numericProperty(objectSource, name) {
  const match = objectSource.match(new RegExp(`\\b${name}\\s*:\\s*(\\d+)\\b`));
  return match ? Number(match[1]) : undefined;
}

test("Waveshare display examples use declared routes and exact RP bus rates", () => {
  const failures = [];
  for (const [directory, config] of Object.entries(displayBoards)) {
    const descriptor = boardDescriptors[config.boardId];
    const routes = descriptor.capabilities.spi?.routes ?? [];
    const directoryPath = join(examplesRoot, directory);
    for (const path of javascriptFiles(directoryPath)) {
      const source = readFileSync(path, "utf8");
      for (const match of source.matchAll(/\{[^{}]*\bspiBus\s*:\s*\d+[^{}]*\}/gs)) {
        const route = {
          bus: numericProperty(match[0], "spiBus"),
          sck: numericProperty(match[0], "sck"),
          mosi: numericProperty(match[0], "mosi"),
          miso: numericProperty(match[0], "miso"),
        };
        if (Object.values(route).some((value) => value === undefined)) continue;
        const declared = routes.some((candidate) =>
          candidate.bus === route.bus && candidate.sck === route.sck &&
          candidate.mosi === route.mosi && candidate.miso === route.miso);
        if (!declared) {
          failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: SPI route is not declared: ${JSON.stringify(route)}`);
        }
      }

      const ratePatterns = [
        /(?:^|[^A-Za-z0-9_$])SPI\.init\s*\([^\n)]*,\s*(\d+)\s*\)/g,
      ];
      if (source.includes("SPI.init")) {
        ratePatterns.push(/\bbaudrate\s*=\s*options\.baudrate\s*\|\|\s*(\d+)\b/g);
      }
      for (const expression of ratePatterns) {
        for (const match of source.matchAll(expression)) {
          const requested = Number(match[1]);
          const actual = actualRpSpiRate(config.peripheralClockHz, requested);
          if (actual !== requested) {
            failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: SPI rate ${requested} is not exactly representable; RP SDK selects ${actual}`);
          }
        }
      }

      const i2cRoutes = descriptor.capabilities.i2c?.routes ?? [];
      for (const match of source.matchAll(/I2C\.init\s*\(\s*([^,\n]+),\s*([^,\n]+),\s*([^,\n]+),\s*(\d+)\s*\)/g)) {
        const requested = Number(match[4]);
        const actual = actualRpI2cRate(config.peripheralClockHz, requested);
        if (actual !== requested) {
          failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: I2C rate ${requested} is not exactly representable; RP SDK selects ${actual}`);
        }
      }
      if (source.includes("I2C.init")) {
        for (const match of source.matchAll(/\bbaudrate\s*=\s*options\.baudrate\s*\|\|\s*(\d+)\b/g)) {
          const requested = Number(match[1]);
          const actual = actualRpI2cRate(config.peripheralClockHz, requested);
          if (actual !== requested) {
            failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: I2C default rate ${requested} is not exactly representable; RP SDK selects ${actual}`);
          }
        }
      }
      for (const match of source.matchAll(/\{[^{}]*\bi2cBus\s*:\s*\d+[^{}]*\}/gs)) {
        const route = {
          bus: numericProperty(match[0], "i2cBus"),
          sda: numericProperty(match[0], "sda"),
          scl: numericProperty(match[0], "scl"),
        };
        if (Object.values(route).some((value) => value === undefined)) continue;
        const declared = i2cRoutes.some((candidate) =>
          candidate.bus === route.bus && candidate.sda === route.sda && candidate.scl === route.scl);
        if (!declared) {
          failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: I2C route is not declared: ${JSON.stringify(route)}`);
        }
      }
    }
  }

  assert.deepEqual(failures, [], failures.join("\n"));
});
