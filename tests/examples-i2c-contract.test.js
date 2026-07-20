const { existsSync, readFileSync } = require("node:fs");
const { join } = require("node:path");
const vm = require("node:vm");
const test = require("node:test");
const assert = require("node:assert/strict");

const source = readFileSync(join(__dirname, "../examples/i2c-scan/index.js"), "utf8");
const rootReadme = readFileSync(join(__dirname, "../README.md"), "utf8");
const examplesReadme = readFileSync(join(__dirname, "../examples/README.md"), "utf8");
const builtInModules = readFileSync(join(__dirname, "../docs/docs/built-in-modules.md"), "utf8");
const migration = readFileSync(join(__dirname, "../docs/docs/migration/0.2.md"), "utf8");
const conformance = readFileSync(join(__dirname, "../docs/docs/development/portable-api-conformance.md"), "utf8");
const protocolPath = join(__dirname, "../docs/docs/development/i2c-peripheral-protocol.md");
const i2cReadmeSection = examplesReadme.match(/### i2c-scan\/[\s\S]*?(?=\n### |$)/)?.[0] ?? "";

function createRuntime({ i2cPresent = true, devices = [0x3c, 0x68], failCode } = {}) {
  const calls = { init: [], read: [], logs: [] };
  const capability = {
    buses: [0, 1],
    routes: [
      { bus: 0, sda: 5, scl: 6 },
      { bus: 1, sda: 3, scl: 4 },
    ],
    defaultBus: 0,
    defaultRoute: { bus: 0, sda: 5, scl: 6 },
    frequency: { minHz: 1, maxHz: 1000000 },
    maxTransferBytes: 256,
  };
  const i2c = {
    init(options) { calls.init.push(options); },
    read(bus, address, length) {
      calls.read.push({ bus, address, length });
      if (failCode) {
        const error = new Error("injected failure");
        error.code = failCode;
        throw error;
      }
      if (devices.includes(address)) return [address & 0xff];
      const error = new Error("no target");
      error.code = "ENXIO";
      throw error;
    },
  };
  const board = {
    capability(name) {
      assert.equal(name, "i2c");
      return capability;
    },
  };
  const modules = { has(name) { return name === "i2c" && i2cPresent; } };
  const context = vm.createContext({
    console: { log(...parts) { calls.logs.push(parts.join(" ")); } },
    require(name) {
      if (name === "board") return board;
      if (name === "mcujs:module") return modules;
      if (name === "i2c" && i2cPresent) return i2c;
      throw new Error(`unexpected require: ${name}`);
    },
  });
  return {
    calls,
    run() { vm.runInContext(source, context, { filename: "examples/i2c-scan/index.js" }); },
  };
}

test("I2C scanner is capability-driven, rerunnable, bounded, and uses options init", () => {
  assert.doesNotMatch(source, /\b(?:board|chip)\.(?:name|chip)\b|boardApi\.(?:name|chip)\b/);
  assert.doesNotMatch(source, /\b(?:const|let)\b/);
  assert.match(source, /require\(['"]mcujs:module['"]\)/);
  assert.match(source, /require\(['"]board['"]\)/);
  assert.match(source, /require\(['"]i2c['"]\)/);
  assert.match(source, /capability\(['"]i2c['"]\)/);

  const runtime = createRuntime();
  runtime.run();
  assert.equal(runtime.calls.init.length, 1);
  assert.equal(runtime.calls.init[0].bus, 0);
  assert.equal(runtime.calls.init[0].frequency, 100000);
  assert.equal("sda" in runtime.calls.init[0], false);
  assert.equal("scl" in runtime.calls.init[0], false);
  assert.equal(runtime.calls.read.length, 0x77 - 0x08 + 1);
  assert.deepEqual(runtime.calls.read[0], { bus: 0, address: 0x08, length: 1 });
  assert.deepEqual(runtime.calls.read.at(-1), { bus: 0, address: 0x77, length: 1 });
  assert.ok(runtime.calls.logs.includes("Found device at 0x3C"));
  assert.ok(runtime.calls.logs.includes("Found device at 0x68"));
  assert.ok(runtime.calls.logs.includes("Demo complete!"));

  runtime.run();
  assert.equal(runtime.calls.init.length, 2, "persistent-realm rerun must succeed");
});

test("I2C scanner feature-detects the module and does not swallow bus failures", () => {
  const absent = createRuntime({ i2cPresent: false });
  absent.run();
  assert.deepEqual(absent.calls.init, []);
  assert.deepEqual(absent.calls.read, []);

  const failed = createRuntime({ failCode: "EIO" });
  assert.throws(() => failed.run(), /injected failure/);
  assert.equal(failed.calls.read.length, 1);
});

test("I2C docs cover options migration, limits, errors, and physical acceptance", () => {
  assert.match(rootReadme, /I2C\.init\(\{\s*frequency:\s*100000\s*\}\)/);
  assert.match(i2cReadmeSection, /board\.capability\(['"]i2c['"]\)/);
  assert.match(i2cReadmeSection, /defaultRoute/);
  assert.doesNotMatch(i2cReadmeSection, /GPIO 4|GPIO 5/);

  assert.match(builtInModules, /### I2C contract/);
  for (const term of [
    "i2c.init(options)",
    "i2c.write(bus, address, data)",
    "i2c.read(bus, address, length)",
    "defaultRoute",
    "maxTransferBytes",
    "ENXIO",
    "EBUSY",
    "EIO",
  ]) {
    assert.ok(builtInModules.includes(term), `built-in module docs omit ${term}`);
  }

  assert.match(migration, /i2c\.init\(bus, sda, scl, frequency\).*i2c\.init\(options\)/);
  assert.match(migration, /oversized transfers throw instead of being truncated/);
  assert.ok(existsSync(protocolPath), "I2C peripheral protocol is missing");
  assert.match(conformance, /i2c-peripheral-protocol\.md/);

  const protocol = readFileSync(protocolPath, "utf8");
  for (const term of [
    "No flashing",
    "target emulator",
    "0x42",
    "100 kHz",
    "400 kHz",
    "ENXIO",
    "logic analyzer",
    "maxTransferBytes",
    "common ground",
  ]) {
    assert.ok(protocol.includes(term), `I2C peripheral protocol omits ${term}`);
  }
});
