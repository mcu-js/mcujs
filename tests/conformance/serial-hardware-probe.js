/*
 * Portable MCU.js serial conformance probe.
 *
 * A host collector configures this rerunnable probe with the schema-derived
 * board plan and independently captured `.help` modules. Unregistered cases are
 * emitted as explicit skips; addCase() replaces selected planned cases with
 * real observations. The RS/US frame prevents echoed source from being parsed.
 */
(function (root) {
  "use strict";

  var FRAME_START = "\x1eMCUJS_CONFORMANCE_V1 ";
  var FRAME_END = "\x1f";
  var plan = null;
  var registeredCases = {};

  function copyArray(values) {
    return Array.prototype.slice.call(values || []);
  }

  function valueType(value) {
    if (Array.isArray(value)) return "array";
    return typeof value;
  }

  function capture(call, options) {
    try {
      var value = call();
      var result = {
        status: "success",
        value: value,
        valueType: valueType(value),
      };
      if (options && options.unit) result.unit = options.unit;
      return result;
    } catch (error) {
      return {
        status: "throw",
        errorClass: error && error.constructor ? error.constructor.name : typeof error,
        name: error && error.name,
        code: error && Object.prototype.hasOwnProperty.call(error, "code") ? error.code : null,
        message: error && String(error.message || error),
      };
    }
  }

  function matchesExpectation(observed, expected) {
    if (observed.status !== expected.status) return false;
    if (expected.status !== "throw") return true;
    return observed.errorClass === expected.errorClass
      && observed.code === expected.code
      && (!expected.name || observed.name === expected.name);
  }

  function configure(nextPlan) {
    if (!nextPlan || !Array.isArray(nextPlan.candidates)
        || !Array.isArray(nextPlan.cases) || !Array.isArray(nextPlan.helpModules)
        || !nextPlan.exportNames || typeof nextPlan.exportNames !== "object") {
      throw new TypeError("probe plan requires candidates, cases, helpModules, and exportNames");
    }
    plan = nextPlan;
    registeredCases = {};
  }

  function addCase(id, call, options) {
    if (!plan || typeof id !== "string" || typeof call !== "function") {
      throw new TypeError("configure the probe before adding an id/function case");
    }
    if (!plan.cases.some(function (entry) { return entry.id === id; })) {
      throw new RangeError("probe case id is not in the configured plan");
    }
    registeredCases[id] = { call: call, options: options || {} };
  }

  function observeModules(candidates) {
    var registered = [];
    var loaded = {};
    var index;
    for (index = 0; index < candidates.length; index += 1) {
      try {
        loaded[candidates[index]] = require(candidates[index]);
        registered.push(candidates[index]);
      } catch (error) {
        loaded[candidates[index]] = null;
      }
    }
    return { registered: registered, loaded: loaded };
  }

  function orderedObservedExports(module, expectedNames) {
    var actual = Object.keys(module);
    var ordered = expectedNames.filter(function (name) { return actual.indexOf(name) !== -1; });
    actual.forEach(function (name) {
      if (ordered.indexOf(name) === -1) ordered.push(name);
    });
    return ordered;
  }

  function run() {
    var board = require("board");
    var modules = require("mcujs:module");
    var capabilities = board.capabilities();
    var names = copyArray(modules.builtinModules);
    var activePlan = plan || {
      candidates: names.concat(["__missing_module__"]),
      cases: [],
      helpModules: [],
      exportNames: {},
    };
    var observation = observeModules(activePlan.candidates);
    var has = {};
    var exports = {};
    var cases = [];
    var index;

    for (index = 0; index < activePlan.candidates.length; index += 1) {
      has[activePlan.candidates[index]] = modules.has(activePlan.candidates[index]);
    }
    Object.keys(activePlan.exportNames).forEach(function (moduleName) {
      var module = observation.loaded[moduleName];
      exports[moduleName] = module
        ? orderedObservedExports(module, activePlan.exportNames[moduleName])
        : [];
    });

    cases = activePlan.cases.map(function (definition) {
      var registered = registeredCases[definition.id];
      if (!registered) {
        return {
          id: definition.id,
          kind: definition.kind,
          status: "skip",
          expected: definition.expected,
          reason: "no electrically safe hardware callback registered for this planned case",
        };
      }
      var observed = capture(registered.call, registered.options);
      return {
        id: definition.id,
        kind: definition.kind,
        status: matchesExpectation(observed, definition.expected) ? "pass" : "fail",
        expected: definition.expected,
        observed: observed,
      };
    });

    console.log(FRAME_START + JSON.stringify({
      formatVersion: 1,
      apiVersion: board.apiVersion,
      board: board.name,
      lane: "serial-hardware",
      discovery: {
        registered: observation.registered,
        builtinModules: names,
        has: has,
        help: copyArray(activePlan.helpModules),
        capabilities: Object.keys(capabilities),
        exports: exports,
      },
      cases: cases,
      summary: {
        passed: cases.filter(function (entry) { return entry.status === "pass"; }).length,
        failed: cases.filter(function (entry) { return entry.status === "fail"; }).length,
        skipped: cases.filter(function (entry) { return entry.status === "skip"; }).length,
      },
    }) + FRAME_END);
  }

  root.__mcujsConformanceProbe = Object.freeze({
    addCase: addCase,
    configure: configure,
    run: run,
  });
  root.__mcujsConformanceProbe.run();
}(globalThis));
