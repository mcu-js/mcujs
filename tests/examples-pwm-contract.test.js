const { existsSync, readFileSync, readdirSync, statSync } = require("node:fs");
const { join } = require("node:path");
const vm = require("node:vm");
const test = require("node:test");
const assert = require("node:assert/strict");

const source = readFileSync(join(__dirname, "../examples/pwm-fade/index.js"), "utf8");
const buzzerSource = readFileSync(join(__dirname, "../examples/waveshare-lcd-1.69/buzzer.js"), "utf8");
const rootReadme = readFileSync(join(__dirname, "../README.md"), "utf8");
const examplesReadme = readFileSync(join(__dirname, "../examples/README.md"), "utf8");
const builtInModules = readFileSync(join(__dirname, "../docs/docs/built-in-modules.md"), "utf8");
const apiReference = readFileSync(join(__dirname, "../docs/docs/api-reference.md"), "utf8");
const migration = readFileSync(join(__dirname, "../docs/docs/migration/0.2.md"), "utf8");
const conformance = readFileSync(join(__dirname, "../docs/docs/development/portable-api-conformance.md"), "utf8");
const waveformProtocolPath = join(__dirname, "../docs/docs/development/pwm-waveform-protocol.md");
const pwmReadmeSection = examplesReadme.match(/### pwm-fade\/[\s\S]*?(?=\n### |$)/)?.[0] ?? "";

function collectTextFiles(path) {
  if (!statSync(path).isDirectory()) return [path];
  return readdirSync(path)
    .flatMap((name) => collectTextFiles(join(path, name)))
    .filter((name) => /\.(?:js|md)$/.test(name));
}

function createRuntime({
  pwmPresent = true,
  led = { type: "gpio", pin: 25, activeLow: false },
  overridePin,
  overrideActiveLow,
} = {}) {
  let nextTimer = 1;
  const intervals = new Map();
  const timeouts = new Map();
  const calls = { init: [], duty: [], stop: [], logs: [], clears: [] };
  const pwm = {
    init() { calls.init.push([...arguments]); },
    setDuty(pin, duty) { calls.duty.push({ pin, duty }); },
    stop(pin) { calls.stop.push(pin); },
  };
  const board = {
    devices: led ? { led } : {},
    capability(name) {
      assert.equal(name, "pwm");
      return {
        pins: [1, 2, 3, 4, 5, 6, 7, 8, 9, 25],
        maxOutputs: 8,
        timerCount: 4,
        duty: { min: 0, max: 1, unit: "ratio" },
        frequency: { minHz: 10, maxHz: 1000000, resolutionVaries: true },
      };
    },
  };
  const modules = { has(name) { return name === "pwm" && pwmPresent; } };
  const sandbox = {
    console: { log(...parts) { calls.logs.push(parts.join(" ")); } },
    require(name) {
      if (name === "board") return board;
      if (name === "mcujs:module") return modules;
      if (name === "pwm" && pwmPresent) return pwm;
      throw new Error(`unexpected require: ${name}`);
    },
    setInterval(callback) {
      const id = nextTimer++;
      intervals.set(id, callback);
      return id;
    },
    clearInterval(id) {
      calls.clears.push(["interval", id]);
      intervals.delete(id);
    },
    setTimeout(callback) {
      const id = nextTimer++;
      timeouts.set(id, callback);
      return id;
    },
    clearTimeout(id) {
      calls.clears.push(["timeout", id]);
      timeouts.delete(id);
    },
  };
  if (overridePin !== undefined) sandbox.pwmFadePin = overridePin;
  if (overrideActiveLow !== undefined) {
    sandbox.pwmFadeActiveLow = overrideActiveLow;
  }
  const context = vm.createContext(sandbox);
  return {
    calls,
    context,
    run() { vm.runInContext(source, context, { filename: "examples/pwm-fade/index.js" }); },
    tick(count = 1) {
      for (let i = 0; i < count; i += 1) {
        for (const callback of [...intervals.values()]) callback();
      }
    },
    finish() {
      for (const [id, callback] of [...timeouts.entries()]) {
        timeouts.delete(id);
        callback();
      }
    },
    activeIntervals() { return intervals.size; },
  };
}

test("PWM fade is feature-detected, rerunnable, bounded, and ratio-only", () => {
  assert.doesNotMatch(source, /\b(?:board|chip)\.(?:name|chip)\b|boardApi\.(?:name|chip)\b/);
  assert.doesNotMatch(source, /\b(?:const|let)\b/);
  assert.match(source, /require\(['"]mcujs:module['"]\)/);
  assert.match(source, /require\(['"]pwm['"]\)/);
  assert.match(source, /capability\(['"]pwm['"]\)/);

  const runtime = createRuntime();
  runtime.run();
  assert.deepEqual(runtime.calls.init, [[25, 1250]]);
  assert.equal(runtime.activeIntervals(), 1);

  runtime.tick(40);
  assert.ok(runtime.calls.duty.length > 0);
  for (const { duty } of runtime.calls.duty) {
    assert.ok(Number.isFinite(duty) && duty >= 0 && duty <= 1, `invalid duty ${duty}`);
    assert.equal(duty * 64, Math.trunc(duty * 64), `duty ${duty} is not a sixty-fourth`);
  }
  assert.ok(runtime.calls.duty.some(({ duty }) => duty === 1 / 64));

  runtime.run();
  assert.equal(runtime.activeIntervals(), 1, "rerun must replace the prior interval");
  assert.ok(runtime.calls.stop.length >= 1, "rerun must release the prior PWM output");

  runtime.finish();
  assert.equal(runtime.activeIntervals(), 0);
  assert.equal(runtime.calls.duty.at(-1).duty, 0);
  assert.equal(runtime.calls.stop.at(-1), 25);
  assert.ok(runtime.calls.logs.includes("Demo complete!"));
});

test("PWM fade accepts only an explicitly advertised user pin override", () => {
  const runtime = createRuntime({
    led: null,
    overridePin: 2,
    overrideActiveLow: true,
  });
  runtime.run();
  assert.deepEqual(runtime.calls.init, [[2, 1250]]);
  runtime.tick();
  assert.equal(runtime.calls.duty.at(-1).duty, 63 / 64);
  runtime.finish();
  assert.equal(runtime.calls.stop.at(-1), 2);

  const invalid = createRuntime({ overridePin: 21 });
  invalid.run();
  assert.deepEqual(invalid.calls.init, []);
  assert.deepEqual(invalid.calls.duty, []);
  assert.deepEqual(invalid.calls.stop, []);
});

test("PWM fade exits without touching hardware when PWM or a PWM-capable onboard LED is absent", () => {
  for (const options of [
    { pwmPresent: false },
    { led: null },
    { led: { type: "managed" } },
    { led: { type: "gpio", pin: 21, activeLow: true } },
  ]) {
    const runtime = createRuntime(options);
    runtime.run();
    assert.deepEqual(runtime.calls.init, []);
    assert.deepEqual(runtime.calls.duty, []);
    assert.deepEqual(runtime.calls.stop, []);
    assert.equal(runtime.activeIntervals(), 0);
  }
});

test("PWM user docs describe the portable contract, migration, and non-destructive waveform protocol", () => {
  assert.doesNotMatch(pwmReadmeSection, /GPIO 25|Hardware:\*\* None/);
  assert.match(pwmReadmeSection, /pwmFadePin/);
  assert.match(pwmReadmeSection, /PWM-capable/);

  assert.match(builtInModules, /### PWM contract/);
  for (const term of [
    "pwm.init(pin, frequency)",
    "pwm.setDuty(pin, duty)",
    "pwm.stop(pin)",
    "maxOutputs",
    "timerCount",
    "ERR_NOT_SUPPORTED",
    "ERR_RESOURCE_EXHAUSTED",
  ]) {
    assert.ok(builtInModules.includes(term), `built-in module docs omit ${term}`);
  }

  assert.match(apiReference, /PWM.*Built-in Modules/);
  assert.match(rootReadme, /duty:\s*0\.0-1\.0 ratio/);
  assert.doesNotMatch(rootReadme, /PWM\.setDuty[^\n]*(?:0-65535|raw)/);
  assert.match(migration, /raw\s*\/\s*65535/);
  assert.match(migration, /representable exactly/);
  assert.match(migration, /maxOutputs/);
  assert.ok(existsSync(waveformProtocolPath), "PWM waveform protocol is missing");
  assert.match(conformance, /pwm-waveform-protocol\.md/);

  const protocol = readFileSync(waveformProtocolPath, "utf8");
  for (const term of [
    "10 MS/s",
    "100 cycles",
    "0.5%",
    "0.25 percentage point",
    "1 / 64",
    "1100 Hz",
    "1601 Hz",
    "No flashing",
  ]) {
    assert.ok(protocol.includes(term), `waveform protocol omits ${term}`);
  }
});

test("repository user guidance never advertises raw PWM duty", () => {
  const roots = [
    join(__dirname, "../README.md"),
    join(__dirname, "../examples"),
    join(__dirname, "../docs/docs"),
  ];
  const migrationPath = join(__dirname, "../docs/docs/migration/0.2.md");
  for (const path of roots.flatMap(collectTextFiles)) {
    if (path === migrationPath) continue;
    const lines = readFileSync(path, "utf8").split("\n");
    for (let index = 0; index < lines.length; index += 1) {
      const line = lines[index];
      if (/duty|PWM\.setDuty/i.test(line) && /0\s*-\s*65535/.test(line)) {
        assert.fail(`${path}:${index + 1} advertises stale raw PWM duty`);
      }
    }
  }
});

function rp2350FrequencyIsExact(frequency) {
  const clock = 150000000;
  for (let dividerScaled = 16; dividerScaled <= 4095; dividerScaled += 1) {
    const denominator = frequency * dividerScaled;
    if ((clock * 16) % denominator !== 0) continue;
    const period = (clock * 16) / denominator;
    if (period >= 1 && period <= 65535) return true;
  }
  return false;
}

test("Waveshare buzzer selects nearest exact RP frequency for every declared note", () => {
  assert.doesNotMatch(buzzerSource, /board\.(?:name|chip)|chip\.(?:name|id)/);
  assert.match(buzzerSource, /capability\(['"]pwm['"]\)/);

  const accepted = [];
  const pwm = {
    init(pin, frequency) {
      assert.equal(pin, 2);
      if (!rp2350FrequencyIsExact(frequency)) {
        const error = new Error("not exact");
        error.code = "ERR_NOT_SUPPORTED";
        throw error;
      }
      accepted.push(frequency);
    },
    setDuty() {},
  };
  const module = { exports: {} };
  vm.runInNewContext(buzzerSource, {
    PWM: pwm,
    board: {
      capability(name) {
        assert.equal(name, "pwm");
        return { frequency: { minHz: 10, maxHz: 2048 } };
      },
      delay() {},
    },
    module,
  }, { filename: "examples/waveshare-lcd-1.69/buzzer.js" });

  const requested = Object.values(module.exports.NOTE).filter((frequency) => frequency > 0);
  assert.equal(requested.length, 36);
  module.exports.init();
  for (const frequency of requested) {
    const before = accepted.length;
    module.exports.tone(frequency);
    assert.equal(accepted.length, before + 1, `${frequency} Hz did not configure`);
    const selected = accepted.at(-1);
    assert.ok(rp2350FrequencyIsExact(selected), `${selected} Hz is not exact`);
    for (let candidate = 10; candidate <= 2048; candidate += 1) {
      if (!rp2350FrequencyIsExact(candidate)) continue;
      assert.ok(
        Math.abs(selected - frequency) <= Math.abs(candidate - frequency),
        `${selected} Hz is not nearest to ${frequency} Hz`,
      );
    }
  }
});
