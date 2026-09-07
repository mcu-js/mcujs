"use strict";

const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const { join } = require("node:path");
const test = require("node:test");
const { runInNewContext } = require("node:vm");

const { boardDescriptors, shippingBoardIds } = require("../runtime/board-registry.js");
const schema = require("../docs/static/schemas/mcujs-portable-api-0.2.schema.json");
const contract = schema["x-mcujs-contract"];
const {
  CONFORMANCE_FRAME_END,
  CONFORMANCE_FRAME_START,
  createSerialPlan,
  expectedRuntimeSurface,
  formatEvidenceLine,
  generateContractCases,
  parseEvidenceLines,
  runPortableApiConformance,
  validateEvidence,
} = require("./conformance/portable-api-conformance.js");

const lanes = [
  { boardId: "pico", lane: "full-rp" },
  { boardId: "waveshare_rp2350_lcd_1.47_a", lane: "constrained-rp" },
  { boardId: "seeed_xiao_esp32s3", lane: "esp32" },
];

function candidatesFor(descriptor) {
  return [...new Set([
    ...descriptor.modules,
    ...Object.keys(contract.modules),
    "node:module",
    "__missing_module__",
  ])];
}

function successObservation(testCase, descriptor) {
  if (testCase.kind !== "return") return { status: "success", valueType: "undefined" };
  const returns = testCase.expected.returns;
  const type = returns.types[0];
  const correlatedValue = returns.correlatesWithArgument
    ? testCase.fixture?.arguments?.[returns.correlatesWithArgument]
    : undefined;
  let value;
  if (type === "undefined") value = undefined;
  else if (type === "boolean") value = true;
  else if (type === "string") value = "fixture";
  else if (type === "integer" || type === "number" || type === "uint8") {
    value = returns.minimumFromCapability
      ? returns.minimumFromCapability.split(".").reduce((node, key) => node[key], descriptor.capabilities)
      : (returns.minimum ?? 0);
  } else if (type === "byteArray" && Array.isArray(correlatedValue)) {
    value = correlatedValue.map(() => 0);
  } else if (type === "byteArray" || type === "stringArray") value = [];
  else value = {};
  return {
    status: "success",
    value,
    valueType: Array.isArray(value) ? "array" : typeof value,
    ...(returns.unit ? { unit: returns.unit } : {}),
  };
}

// This fixture exists only to unit-test the evidence validator. It is never
// reported as backend or hardware conformance evidence.
function validatorUnitFixture(boardId, lane = "validator-unit-fixture") {
  const descriptor = boardDescriptors[boardId];
  const surface = expectedRuntimeSurface(contract, descriptor);
  const cases = generateContractCases(contract, descriptor).map((testCase) => {
    const observed = testCase.expected.status === "throw"
      ? {
          status: "throw",
          errorClass: testCase.expected.errorClass,
          name: testCase.expected.name ?? testCase.expected.errorClass,
          code: testCase.expected.code,
          message: "validator fixture exception",
          ...(testCase.expected.code ? { resource: "fixture" } : {}),
        }
      : successObservation(testCase, descriptor);
    return {
      id: testCase.id,
      kind: testCase.kind,
      status: "pass",
      expected: testCase.expected,
      observed,
    };
  });
  const candidates = candidatesFor(descriptor);
  return {
    formatVersion: 1,
    apiVersion: contract.versioning.apiVersion,
    board: boardId,
    lane,
    discovery: {
      registered: [...descriptor.modules],
      builtinModules: [...descriptor.modules],
      has: Object.fromEntries(candidates.map((name) => [name, descriptor.modules.includes(name)])),
      help: [...descriptor.modules],
      capabilities: Object.keys(descriptor.capabilities),
      exports: structuredClone(surface.exports),
    },
    cases,
    summary: { passed: cases.length, failed: 0, skipped: 0 },
  };
}


test("one schema produces full RP, constrained RP2350, and ESP execution plans without board sniffing", () => {
  for (const { boardId, lane } of lanes) {
    const descriptor = boardDescriptors[boardId];
    const adapter = {
      registeredModules: () => [...descriptor.modules],
      builtinModules: () => [...descriptor.modules],
      hasModule: (name) => descriptor.modules.includes(name),
      helpModules: () => [...descriptor.modules],
      capabilityNames: () => Object.keys(descriptor.capabilities),
      exportNames: (name) => Object.keys(Object.fromEntries(
        (expectedRuntimeSurface(contract, descriptor).exports[name] ?? []).map((entry) => [entry, true]),
      )),
      // Deliberately no observation. The runner must report unexecuted cases as
      // skips, never manufacture a passing value from the expectation.
      runCase: () => undefined,
    };
    const evidence = runPortableApiConformance({ adapter, contract, descriptor, lane });
    assert.equal(evidence.board, boardId);
    assert.equal(evidence.lane, lane);
    assert.ok(evidence.cases.length > 0, `${boardId} produced no contract cases`);
    assert.ok(evidence.cases.every((entry) => entry.status === "skip"));
    assert.equal(evidence.summary.passed, 0);
    assert.equal(evidence.summary.skipped, evidence.cases.length);
    assert.equal(
      validateEvidence({ contract, descriptor, evidence, allowSkipped: true, expectedLane: lane }).valid,
      true,
      boardId,
    );
  }
});

test("every shipping board map produces a complete, uniquely identified case plan", () => {
  for (const boardId of shippingBoardIds) {
    const cases = generateContractCases(contract, boardDescriptors[boardId]);
    assert.ok(cases.length > 0, boardId);
    assert.equal(new Set(cases.map((entry) => entry.id)).size, cases.length, boardId);
  }
});

test("generated cases cover schema types, fixed/capability ranges, routes, enums, and resource constraints", () => {
  const picoCases = generateContractCases(contract, boardDescriptors.pico);
  const ids = new Set(picoCases.map((entry) => entry.id));
  for (const id of [
    "gpio.init.pin.required",
    "gpio.init.pin.wrongType",
    "pwm.init.frequency.nonFinite",
    "pwm.init.frequency.nonIntegral",
    "pwm.init.frequency.maximum",
    "pwm.init.frequency.maximum+1",
    "i2c.write.address.minimum",
    "i2c.write.address.maximum+1",
    "i2c.init.signature1.route.valid",
    "i2c.init.signature1.route.invalid",
    "spi.writeBufferDMA.byteLength.resourceMaximum",
    "spi.writeBufferDMA.byteLength.resourceMaximum+1",
    "neopixel.setPixel.index.configurationMaximum-1",
    "neopixel.setPixel.index.configurationMaximum",
    "spi.transfer.data[].wrongType",
    "spi.transfer.data[].nonFinite",
    "spi.transfer.data[].nonIntegral",
    "spi.transfer.data[].minimum",
    "spi.transfer.data[].minimum-1",
    "spi.transfer.data[].maximum",
    "spi.transfer.data[].maximum+1",
    "i2c.write.data.variant.uint8.nonFinite",
    "i2c.write.data.variant.uint8.nonIntegral",
    "i2c.write.data.variant.uint8.minimum",
    "i2c.write.data.variant.uint8.minimum-1",
    "i2c.write.data.variant.uint8.maximum",
    "i2c.write.data.variant.uint8.maximum+1",
    "spi.transfer.data.variant.uint8.nonFinite",
    "spi.transfer.data.variant.uint8.nonIntegral",
    "spi.transfer.data.variant.uint8.minimum",
    "spi.transfer.data.variant.uint8.minimum-1",
    "spi.transfer.data.variant.uint8.maximum",
    "spi.transfer.data.variant.uint8.maximum+1",
  ]) assert.ok(ids.has(id), `missing generated case ${id}`);

  const gpioMode = picoCases.find((entry) => entry.id === "gpio.init.mode.allowed");
  assert.deepEqual(gpioMode.value, { moduleConstant: "INPUT" });
  const invalidMode = picoCases.find((entry) => entry.id === "gpio.init.mode.unavailable");
  assert.equal(typeof invalidMode.value, "number");
  assert.equal(invalidMode.expected.errorClass, "RangeError");

  const directItemMinimum = picoCases.find(
    (entry) => entry.id === "i2c.write.data[].minimum",
  );
  const directItemAboveMaximum = picoCases.find(
    (entry) => entry.id === "i2c.write.data[].maximum+1",
  );
  const unionItemMinimum = picoCases.find(
    (entry) => entry.id === "spi.transfer.data[].minimum",
  );
  const unionItemWrongType = picoCases.find(
    (entry) => entry.id === "spi.transfer.data[].wrongType",
  );
  assert.equal(directItemMinimum.argumentPath, "data[]");
  assert.deepEqual(directItemMinimum.value, [0],
    "a direct array-item boundary must carry a complete root array fixture");
  assert.deepEqual(directItemAboveMaximum.value, [256],
    "an invalid direct array item must mutate only the targeted root-array element");
  assert.equal(unionItemMinimum.argumentPath, "data[]");
  assert.deepEqual(unionItemMinimum.value, [0],
    "an array branch of a union must carry a valid complete root array fixture");
  assert.equal(unionItemWrongType.argumentPath, "data[]");
  assert.deepEqual(unionItemWrongType.value, ["not-a-number"],
    "an array branch of a union must carry the invalid item inside a valid root array");

  for (const operation of ["i2c.write", "spi.transfer"]) {
    const scalarCases = new Map(picoCases
      .filter((entry) => entry.id.startsWith(`${operation}.data.variant.uint8.`))
      .map((entry) => [entry.id.slice(`${operation}.data.variant.uint8.`.length), entry]));
    const expectedValues = new Map([
      ["nonFinite", { encodedNumber: "NaN" }],
      ["nonIntegral", 0.5],
      ["minimum", 0],
      ["minimum-1", -1],
      ["maximum", 255],
      ["maximum+1", 256],
    ]);
    for (const [suffix, expectedValue] of expectedValues) {
      const scalarCase = scalarCases.get(suffix);
      assert.equal(scalarCase.argumentPath, "data",
        `${operation} scalar ${suffix} must target the direct data argument`);
      assert.deepEqual(scalarCase.value, expectedValue,
        `${operation} scalar ${suffix} must carry a directly runnable scalar root fixture`);
    }
  }

  const constraints = new Set(picoCases.flatMap((entry) => entry.constraints ?? []));
  for (const field of [
    "allowedFromCapability", "conditionalAllowedFromCapability", "minimumFromCapability",
    "maximumFromCapability", "minimumLength", "routeFromCapability",
    "defaultFromCapability", "defaultRouteFromCapability", "maximumFromArgumentResource",
    "exclusiveMaximumFromConfiguration", "invalidResourceError",
  ]) assert.ok(constraints.has(field), `no case covers ${field}`);

  for (const id of [
    "spi.init.implicit.bitsPerWord.allowed",
    "spi.init.implicit.bitOrder.allowed",
  ]) assert.ok(ids.has(id), `missing implicit-configuration case ${id}`);

  const featherCases = generateContractCases(contract, boardDescriptors.adafruit_feather_rp2040);
  const featherIds = new Set(featherCases.map((entry) => entry.id));
  assert.ok(featherIds.has("board.neopixel.signature2.colors[][].maximum+1"),
    "nested NeoPixel array item constraints were not generated");
  assert.ok(featherIds.has("board.neopixel.signature0.colors.minimumLength"));
  assert.equal(featherIds.has("board.neopixel.signature0.colors.minimumLength-1"), false,
    "a zero minimum must not generate an impossible minus-one collection");
  const nestedItemMinimum = featherCases.find(
    (entry) => entry.id === "board.neopixel.signature2.colors[].minimumLength",
  );
  const nestedLeafMaximum = featherCases.find(
    (entry) => entry.id === "board.neopixel.signature2.colors[][].maximum+1",
  );
  assert.equal(nestedItemMinimum.argumentPath, "colors[]");
  assert.deepEqual(nestedItemMinimum.value, [[]],
    "an inner collection boundary must retain its valid outer root collection");
  assert.equal(nestedLeafMaximum.argumentPath, "colors[][]");
  assert.deepEqual(nestedLeafMaximum.value, [[256]],
    "a nested invalid leaf must be mutated at the declared path inside a valid root fixture");
  assert.ok(featherCases.some((entry) => (
    entry.id === "board.neopixel.signature2.colors.onboardMaximum"
      && entry.value.length === boardDescriptors.adafruit_feather_rp2040.board.devices.neopixel.length
  )), "onboard-device maximum was not projected into board.neopixel cases");
});

test("correlated returns generate one exact shape case per accepted argument form", () => {
  const cases = generateContractCases(contract, boardDescriptors.pico);
  const scalar = cases.find((entry) => entry.id === "spi.transfer.return.correlates.uint8");
  const array = cases.find((entry) => entry.id === "spi.transfer.return.correlates.byteArray");
  assert.deepEqual(scalar.expected.returns.types, ["uint8"]);
  assert.equal(typeof scalar.fixture.arguments.data, "number");
  assert.deepEqual(array.expected.returns.types, ["byteArray"]);
  assert.ok(Array.isArray(array.fixture.arguments.data));
  assert.ok(array.fixture.arguments.data.length > 0);

  const evidence = validatorUnitFixture("pico");
  const arrayEvidence = evidence.cases.find((entry) => entry.id === array.id);
  arrayEvidence.observed.value.pop();
  const result = validateEvidence({ contract, descriptor: boardDescriptors.pico, evidence });
  assert.equal(result.valid, false, "array return length stopped correlating with its input");
  assert.ok(result.errors.some((error) => error.code === "FAILED_CASE"));
});

test("capability-derived exact maxima and maximum-plus-one stay board-specific", () => {
  for (const { boardId } of lanes) {
    const descriptor = boardDescriptors[boardId];
    const cases = generateContractCases(contract, descriptor);
    const pwmMax = cases.find((entry) => entry.id === "pwm.init.frequency.maximum");
    const pwmAbove = cases.find((entry) => entry.id === "pwm.init.frequency.maximum+1");
    assert.equal(pwmMax.value, descriptor.capabilities.pwm.frequency.maxHz, boardId);
    assert.deepEqual(pwmAbove.expected, { status: "throw", errorClass: "RangeError", code: null });
    if (descriptor.capabilities.spi) {
      assert.equal(
        cases.find((entry) => entry.id === "spi.transfer.data.maximum").value.length,
        descriptor.capabilities.spi.maxTransferBytes,
      );
      assert.equal(
        cases.find((entry) => entry.id === "spi.transfer.data.maximum+1").value.length,
        descriptor.capabilities.spi.maxTransferBytes + 1,
      );
    }
  }
});

test("schema availability drives absent modules and optional methods without board-name tests", () => {
  const constrained = expectedRuntimeSurface(contract, boardDescriptors["waveshare_rp2350_lcd_1.47_a"]);
  assert.ok(constrained.exports.spi.includes("writeBufferDMA"));
  assert.equal(Object.hasOwn(constrained.exports, "adc"), false);
  const esp = expectedRuntimeSurface(contract, boardDescriptors.seeed_xiao_esp32s3);
  assert.ok(esp.exports.spi.includes("transfer"));
  assert.equal(esp.exports.spi.includes("writeBufferDMA"), false);
  assert.equal(esp.exports.board.includes("neopixel"), false);
  assert.ok(esp.exports.board.includes("safeMode"));
  const feather = expectedRuntimeSurface(contract, boardDescriptors.adafruit_feather_rp2040);
  assert.ok(feather.exports.board.includes("neopixel"));
  assert.ok(feather.exports.spi.includes("writeBufferDMA"));

  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const surface = expectedRuntimeSurface(contract, descriptor);
    assert.equal(
      surface.exports.board.includes("led"),
      Object.hasOwn(descriptor.board.devices, "led"),
      `${boardId} onboard LED shortcut availability`,
    );
    assert.equal(
      surface.exports.board.includes("buttonPressed"),
      Object.hasOwn(descriptor.board.devices, "button"),
      `${boardId} onboard button shortcut availability`,
    );
  }
});

test("touch and IMU interrupt endpoints remain input-only GPIO capabilities", () => {
  for (const boardId of [
    "waveshare_rp2040_touch_lcd_1.28",
    "waveshare_rp2350_touch_lcd_1.69",
  ]) {
    const gpio = boardDescriptors[boardId].capabilities.gpio;
    for (const pin of [21, 23, 24]) {
      assert.ok(gpio.pins.includes(pin), `${boardId} input pin ${pin} missing`);
      assert.equal(gpio.outputPins.includes(pin), false,
        `${boardId} interrupt pin ${pin} advertised as push-pull output`);
    }
  }
});

test("blink examples feature-detect onboard LEDs, stay rerunnable, and write strict booleans", () => {
  for (const filename of ["index.js", "blink.js"]) {
    const source = readFileSync(join(__dirname, "..", "examples", "blink", filename), "utf8");
    assert.doesNotMatch(source, /board\.(?:name|chip)|pico|rp2040|rp2350|esp32/i);
    assert.doesNotMatch(source, /\b(?:let|const)\b/);

    const clearedIntervals = [];
    const clearedTimeouts = [];
    const writes = [];
    const gpio = {
      OUTPUT: 0,
      init(pin, mode) { assert.equal(pin, 21); assert.equal(mode, 0); },
      set(pin, value) {
        assert.equal(pin, 21);
        assert.equal(typeof value, "boolean");
        writes.push(value);
      },
    };
    const context = {
      console: { log() {} },
      require(name) {
        if (name === "board") {
          return { devices: { led: { type: "gpio", pin: 21, activeLow: true } } };
        }
        if (name === "gpio") return gpio;
        throw new Error(`unexpected module ${name}`);
      },
      setInterval() { return 11; },
      clearInterval(id) { clearedIntervals.push(id); },
      setTimeout() { return 12; },
      clearTimeout(id) { clearedTimeouts.push(id); },
    };
    runInNewContext(source, context);
    runInNewContext(source, context);
    assert.deepEqual(writes, [true, true], `${filename} active-low initial writes`);
    assert.deepEqual(clearedIntervals, [11], `${filename} stale interval cleanup`);
    assert.deepEqual(clearedTimeouts, [12], `${filename} stale timeout cleanup`);

    const managedWrites = [];
    runInNewContext(source, {
      console: { log() {} },
      require(name) {
        if (name !== "board") throw new Error(`unexpected module ${name}`);
        return {
          devices: { led: { type: "managed" } },
          led(value) {
            assert.equal(typeof value, "boolean");
            managedWrites.push(value);
          },
        };
      },
      setInterval() { return 21; },
      clearInterval() {},
      setTimeout() { return 22; },
      clearTimeout() {},
    });
    assert.deepEqual(managedWrites, [false], `${filename} managed LED initial write`);
  }
});

test("validator rejects unknown has keys/cases, lane drift, and inconsistent summaries", () => {
  const descriptor = boardDescriptors.pico;
  const mutations = [
    ["extra has", "DISCOVERY_HAS_KEYS", (e) => { e.discovery.has.injected = true; }],
    ["extra case", "EXTRA_CASE", (e) => e.cases.push({ id: "injected", kind: "return", status: "pass", observed: {} })],
    ["lane", "LANE_MISMATCH", (e) => { e.lane = "wrong-lane"; }],
    ["summary", "SUMMARY_MISMATCH", (e) => { e.summary.passed -= 1; }],
    ["missing expectation", "CASE_EXPECTATION_MISMATCH", (e) => { delete e.cases[0].expected; }],
  ];
  for (const [name, code, mutate] of mutations) {
    const evidence = validatorUnitFixture("pico", "validator-unit-fixture");
    mutate(evidence);
    const result = validateEvidence({
      contract, descriptor, evidence, expectedLane: "validator-unit-fixture",
    });
    assert.equal(result.valid, false, `${name} drift was accepted`);
    assert.ok(result.errors.some((error) => error.code === code), JSON.stringify(result.errors));
  }
});

test("validator independently detects every discovery view and rechecks observations", () => {
  const descriptor = boardDescriptors.seeed_xiao_esp32s3;
  const mutations = [
    ["registered", "DISCOVERY_REGISTERED_MISMATCH", (e) => e.discovery.registered.pop()],
    ["builtins", "DISCOVERY_BUILTINS_MISMATCH", (e) => e.discovery.builtinModules.pop()],
    ["has", "DISCOVERY_HAS_MISMATCH", (e) => { e.discovery.has.spi = false; }],
    ["help", "DISCOVERY_HELP_MISMATCH", (e) => e.discovery.help.pop()],
    ["capabilities", "DISCOVERY_CAPABILITIES_MISMATCH", (e) => e.discovery.capabilities.pop()],
    ["exports", "EXPORT_SURFACE_MISMATCH", (e) => e.discovery.exports.spi.pop()],
    ["observation", "FAILED_CASE", (e) => {
      const entry = e.cases.find((item) => item.id === "adc.readVoltagePin.return");
      entry.observed.value = descriptor.capabilities.adc.voltage.maxVolts + 1;
    }],
  ];
  for (const [name, code, mutate] of mutations) {
    const evidence = validatorUnitFixture(descriptor.board.name);
    mutate(evidence);
    const result = validateEvidence({ contract, descriptor, evidence });
    assert.equal(result.valid, false, `${name} drift was accepted`);
    assert.ok(result.errors.some((error) => error.code === code), JSON.stringify(result.errors));
  }
});

test("serial evidence is control-character framed and cannot be parsed from echoed source", () => {
  const evidence = validatorUnitFixture("seeed_xiao_esp32s3", "serial-hardware");
  const line = formatEvidenceLine(evidence);
  assert.ok(line.startsWith(CONFORMANCE_FRAME_START));
  assert.ok(line.endsWith(CONFORMANCE_FRAME_END));
  assert.deepEqual(
    parseEvidenceLines(`echo ${JSON.stringify(line)}\r\n${line}\r\n> `),
    [JSON.parse(JSON.stringify(evidence))],
  );
  assert.deepEqual(parseEvidenceLines(`prefix${line}\r\n`), []);
  assert.deepEqual(parseEvidenceLines(`${line}suffix\r\n`), []);
  assert.throws(() => parseEvidenceLines(
    `${CONFORMANCE_FRAME_START}{"formatVersion":2}${CONFORMANCE_FRAME_END}\r\n`,
  ), /unsupported conformance evidence format/i);
});

test("serial probe emits complete skip-explicit evidence and can replace planned cases with observations", () => {
  const source = readFileSync(join(__dirname, "conformance", "serial-hardware-probe.js"), "utf8");
  assert.doesNotMatch(source, /seeed_xiao|pico|rp2040|rp2350/i);
  assert.doesNotMatch(source, /\b(?:let|const)\b/);
  assert.match(source, /root\.__mcujsConformanceProbe/);

  const descriptor = boardDescriptors.seeed_xiao_esp32s3;
  const surface = expectedRuntimeSurface(contract, descriptor);
  const output = [];
  const moduleApi = {
    builtinModules: [...descriptor.modules],
    has(name) {
      if (typeof name !== "string") throw new TypeError("module name required");
      return descriptor.modules.includes(name);
    },
  };
  const modules = Object.fromEntries(Object.entries(surface.exports).map(([name, exports]) => [
    name,
    Object.fromEntries(exports.map((exportName) => [exportName, function () {}])),
  ]));
  for (const name of descriptor.modules) {
    if (!Object.hasOwn(modules, name)) modules[name] = {};
  }
  const board = Object.assign(modules.board, {
    apiVersion: "0.2",
    name: descriptor.board.name,
    capabilities: () => structuredClone(descriptor.capabilities),
    capability(name) {
      if (typeof name !== "string") throw new TypeError("capability name required");
      return structuredClone(descriptor.capabilities[name]);
    },
  });
  modules.board = board;
  modules["mcujs:module"] = moduleApi;
  modules["node:module"] = moduleApi;
  const context = {
    console: { log(line) { output.push(line); } },
    require(name) {
      if (!Object.hasOwn(modules, name)) throw new Error(`module not found: ${name}`);
      return modules[name];
    },
  };
  runInNewContext(source, context);
  output.length = 0;
  const plan = createSerialPlan({ contract, descriptor, helpModules: descriptor.modules });
  context.__mcujsConformanceProbe.configure(structuredClone(plan));
  const observedId = plan.cases.find((entry) => entry.expected.status === "throw").id;
  context.__mcujsConformanceProbe.addCase(observedId, function () { throw new TypeError("observed"); });
  context.__mcujsConformanceProbe.run();

  const [evidence] = parseEvidenceLines(output.join("\n"));
  assert.equal(evidence.cases.length, plan.cases.length);
  assert.equal(evidence.cases.find((entry) => entry.id === observedId).status, "pass");
  assert.ok(evidence.cases.filter((entry) => entry.id !== observedId).every((entry) => (
    entry.status === "skip" && typeof entry.reason === "string" && entry.reason.length > 0
  )));
  assert.deepEqual(evidence.discovery.registered, descriptor.modules);
  assert.deepEqual(evidence.discovery.help, descriptor.modules);
  assert.deepEqual(evidence.discovery.exports, surface.exports);
  assert.equal(validateEvidence({
    contract,
    descriptor,
    evidence,
    allowSkipped: true,
    expectedLane: "serial-hardware",
  }).valid, true);
});

test("native registry lanes link production backend factories instead of redefining them", () => {
  const harness = readFileSync(join(__dirname, "runtime_bindings_test.c"), "utf8");
  for (const factory of ["gpio", "pwm", "i2c"]) {
    assert.doesNotMatch(harness, new RegExp(`js_create_${factory}_module\\s*\\(`));
  }
  const script = readFileSync(join(__dirname, "..", "scripts", "test-runtime-bindings.sh"), "utf8");
  assert.match(script, /\$\{backend\}\/bindings\/gpio\.c/);
  assert.match(script, /compile_binding_test "\$\{FULL\}" platform\/rp2/);
  assert.match(script, /compile_binding_test "\$\{CONSTRAINED\}" platform\/esp32\/main/);
  assert.match(script, /nm -g/);
});
