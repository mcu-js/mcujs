"use strict";

const CONFORMANCE_PREFIX = "MCUJS_CONFORMANCE_V1 ";
const CONFORMANCE_FRAME_START = `\x1e${CONFORMANCE_PREFIX}`;
const CONFORMANCE_FRAME_END = "\x1f";
const FORMAT_VERSION = 1;

function unique(values) {
  return [...new Set(values)];
}

function sameArray(left, right) {
  return Array.isArray(left)
    && Array.isArray(right)
    && left.length === right.length
    && left.every((value, index) => value === right[index]);
}

function sameKeySet(left, right) {
  return Array.isArray(left)
    && Array.isArray(right)
    && sameArray([...left].sort(), [...right].sort());
}

function sameJson(left, right) {
  return JSON.stringify(left) === JSON.stringify(right);
}

function resolvePath(root, path) {
  let values = [root];
  for (const segment of path.split(".")) {
    const projected = segment.endsWith("[]");
    const name = projected ? segment.slice(0, -2) : segment;
    const next = [];
    for (const value of values) {
      if (value === null || value === undefined || !Object.hasOwn(value, name)) continue;
      const child = value[name];
      if (projected) {
        if (Array.isArray(child)) next.push(...child);
      } else {
        next.push(child);
      }
    }
    values = next;
  }
  if (values.length === 0) return undefined;
  return values.length === 1 ? values[0] : values;
}

function capabilityValue(descriptor, path) {
  return resolvePath(descriptor.capabilities, path);
}

function moduleAvailable(definition, descriptor) {
  if (definition.availability === "required") return true;
  if (definition.availability === "capability") {
    return Object.hasOwn(descriptor.capabilities, definition.capability);
  }
  throw new Error(`unsupported module availability: ${definition.availability}`);
}

function exportAvailable(moduleName, definition, descriptor) {
  switch (definition.availability) {
    case "required":
      return true;
    case "capability":
      return Object.hasOwn(descriptor.capabilities, definition.capability);
    case "capabilityField":
      return capabilityValue(descriptor, definition.capabilityField) === true;
    case "capabilityCollectionValue": {
      const moduleCapability = descriptor.capabilities[moduleName];
      const values = moduleCapability?.[definition.capabilityField];
      return Array.isArray(values) && values.includes(definition.capabilityValue);
    }
    case "onboardDevice":
      return Object.hasOwn(descriptor.board.devices, definition.onboardDevice);
    default:
      throw new Error(`unsupported export availability: ${definition.availability}`);
  }
}

function expectedRuntimeSurface(contract, descriptor) {
  const exports = {};
  for (const [moduleName, moduleDefinition] of Object.entries(contract.modules)) {
    if (!moduleAvailable(moduleDefinition, descriptor)) continue;
    exports[moduleName] = Object.entries(moduleDefinition.exports ?? {})
      .filter(([, definition]) => exportAvailable(moduleName, definition, descriptor))
      .map(([name]) => name);
  }
  return {
    modules: [...descriptor.modules],
    capabilities: Object.keys(descriptor.capabilities),
    exports,
  };
}

function errorExpectation(errorClass, code = null, name = null) {
  return { status: "throw", errorClass, code, ...(name ? { name } : {}) };
}

function wrongTypeValue(type) {
  const javascriptTypes = type.javascriptTypes ?? [type.javascriptType];
  if (javascriptTypes.some((entry) => entry === "number" || entry === "uint8")) return "not-a-number";
  if (javascriptTypes.includes("string")) return 1;
  if (javascriptTypes.includes("boolean")) return 1;
  if (javascriptTypes.includes("array")) return {};
  if (javascriptTypes.includes("object")) return [];
  return null;
}

function effectiveArgument(contract, argument) {
  const type = contract.types[argument.type] ?? {};
  return { ...type, ...argument, typeName: argument.type };
}

function arrayArgument(contract, argument) {
  if (argument.javascriptType === "array") return argument;
  for (const typeName of argument.javascriptTypes ?? []) {
    const candidate = contract.types[typeName];
    if (candidate?.javascriptType === "array") return { ...candidate, ...argument };
  }
  return null;
}

function representativeValue(contract, definition) {
  const effective = typeof definition === "string"
    ? effectiveArgument(contract, { name: "fixture", type: definition })
    : definition;
  const array = arrayArgument(contract, effective);
  if (array) {
    const length = array.minimumLength ?? array.minItems ?? 0;
    return collectionValue(contract, array, length);
  }
  if (effective.javascriptType === "string") return "fixture";
  if (effective.javascriptType === "boolean") return false;
  if (effective.javascriptType === "number"
      || effective.javascriptTypes?.includes("uint8")) {
    return minimumFor(effective, { capabilities: {} }) ?? 0;
  }
  if (effective.javascriptType === "object") {
    const value = {};
    for (const [name, field] of Object.entries(effective.fields ?? {})) {
      if (field.required) value[name] = representativeValue(contract, effectiveArgument(contract, field));
    }
    return value;
  }
  return undefined;
}

function collectionValue(contract, type, length) {
  if (type.javascriptType === "string") return "x".repeat(Math.max(0, length));
  const array = arrayArgument(contract, type);
  if (array) {
    return Array.from(
      { length: Math.max(0, length) },
      () => representativeValue(contract, array.items),
    );
  }
  return length;
}

function argumentDefinitions(
  contract,
  argument,
  prefix = argument.name,
  ancestry = [],
  argumentPath = argument.name,
) {
  const effective = effectiveArgument(contract, argument);
  const definitions = [{ argument: effective, path: prefix, argumentPath, ancestry }];
  if (effective.javascriptType === "object" && effective.fields) {
    for (const [fieldName, field] of Object.entries(effective.fields)) {
      definitions.push(...argumentDefinitions(
        contract,
        { name: fieldName, ...field },
        `${prefix}.${fieldName}`,
        [...ancestry, { kind: "field", name: fieldName, parent: effective }],
        `${argumentPath}.${fieldName}`,
      ));
    }
  }
  const arrays = [];
  if (effective.items !== undefined) arrays.push(effective);
  for (const typeName of effective.javascriptTypes ?? []) {
    const alternate = contract.types[typeName];
    if (alternate?.items !== undefined) {
      arrays.push({
        ...alternate,
        ...effective,
        javascriptType: alternate.javascriptType,
        items: alternate.items,
      });
    }
  }
  const seenItems = new Set();
  for (const array of arrays) {
    const items = array.items;
    const itemKey = JSON.stringify(items);
    if (seenItems.has(itemKey)) continue;
    seenItems.add(itemKey);
    const item = typeof items === "string"
      ? { name: "item", type: items }
      : { name: "item", ...items };
    definitions.push(...argumentDefinitions(
      contract,
      item,
      `${prefix}[]`,
      [...ancestry, { kind: "array", parent: array }],
      `${argumentPath}[]`,
    ));
  }
  const seenVariants = new Set();
  for (const typeName of effective.javascriptTypes ?? []) {
    const alternate = contract.types[typeName];
    if (!alternate || alternate.items !== undefined || seenVariants.has(typeName)) continue;
    seenVariants.add(typeName);
    const variantDefinitions = argumentDefinitions(
      contract,
      { name: argument.name, type: typeName },
      `${prefix}.variant.${typeName}`,
      ancestry,
      argumentPath,
    );
    variantDefinitions[0].variantRoot = true;
    definitions.push(...variantDefinitions);
  }
  return definitions;
}

function omitValue(value) {
  return value !== null
    && typeof value === "object"
    && !Array.isArray(value)
    && Object.keys(value).length === 1
    && value.omit === true;
}

function materializeArgumentValue(contract, ancestry, leafValue) {
  function materialize(index) {
    if (index === ancestry.length) return leafValue;
    const step = ancestry[index];
    const child = materialize(index + 1);
    if (step.kind === "field") {
      const parent = representativeValue(contract, step.parent) ?? {};
      if (omitValue(child)) delete parent[step.name];
      else parent[step.name] = child;
      return parent;
    }
    if (step.kind === "array") {
      const minimumLength = step.parent.minimumLength ?? step.parent.minItems ?? 0;
      const parent = collectionValue(contract, step.parent, Math.max(1, minimumLength));
      if (!Array.isArray(parent)) throw new Error("array ancestry did not produce an array fixture");
      if (omitValue(child)) parent.shift();
      else parent[0] = child;
      return parent;
    }
    throw new Error(`unsupported argument ancestry step: ${step.kind}`);
  }
  return materialize(0);
}

function pushCase(cases, base, suffix, expected, details = {}) {
  cases.push({ ...base, id: `${base.id}.${suffix}`, expected, ...details });
}

function minimumFor(argument, descriptor) {
  if (argument.minimumFromCapability) return capabilityValue(descriptor, argument.minimumFromCapability);
  if (Array.isArray(argument.range)) return argument.range[0];
  return argument.minimum;
}

function maximumFor(argument, descriptor) {
  if (argument.maximumFromCapability) return capabilityValue(descriptor, argument.maximumFromCapability);
  if (Array.isArray(argument.range)) return argument.range[1];
  return argument.maximum;
}

function symbolicAllowedValue(argument, value) {
  if (!argument.capabilityValueMap) return value;
  const symbol = Object.entries(argument.capabilityValueMap)
    .find(([, capabilityName]) => capabilityName === value)?.[0];
  if (!symbol) throw new Error(`no symbolic constant maps to capability value ${value}`);
  return { moduleConstant: symbol };
}

function unavailableAllowedValue(argument, allowed) {
  if (argument.capabilityValueMap) return 2147483647;
  if (allowed.every((value) => typeof value === "number")) {
    let unavailable = 0;
    while (allowed.includes(unavailable)) unavailable += 1;
    return unavailable;
  }
  return "__mcujs_unavailable__";
}

function pushArgumentCases(cases, base, argument, contract, descriptor, options = {}) {
  if (argument.required) {
    pushCase(cases, base, "required", errorExpectation("TypeError"), {
      value: { omit: true }, constraints: ["required"],
    });
  }
  if (!options.variantRoot) {
    pushCase(cases, base, "wrongType", errorExpectation("TypeError"), {
      value: wrongTypeValue(argument), constraints: ["javascriptType"],
    });
  }

  const numeric = argument.javascriptType === "number"
    || argument.javascriptTypes?.some((type) => type === "uint8");
  if (numeric && argument.finite) {
    pushCase(cases, base, "nonFinite", errorExpectation("TypeError"), {
      value: { encodedNumber: "NaN" }, constraints: ["finite"],
    });
  }
  if (numeric && argument.integral) {
    pushCase(cases, base, "nonIntegral", errorExpectation("RangeError"), {
      value: 0.5, constraints: ["integral"],
    });
  }

  const minimum = minimumFor(argument, descriptor);
  const maximum = maximumFor(argument, descriptor);
  const minimumConstraint = argument.minimumFromCapability ? "minimumFromCapability" : "minimum";
  const maximumConstraint = argument.maximumFromCapability ? "maximumFromCapability" : "maximum";
  if (typeof minimum === "number") {
    pushCase(cases, base, "minimum", { status: "success" }, {
      value: collectionValue(contract, argument, minimum), constraints: [minimumConstraint],
    });
    pushCase(cases, base, "minimum-1", errorExpectation("RangeError"), {
      value: collectionValue(contract, argument, minimum - 1), constraints: [minimumConstraint],
    });
  }
  if (typeof maximum === "number") {
    pushCase(cases, base, "maximum", { status: "success" }, {
      value: collectionValue(contract, argument, maximum), constraints: [maximumConstraint],
    });
    pushCase(cases, base, "maximum+1", errorExpectation("RangeError"), {
      value: collectionValue(contract, argument, maximum + 1), constraints: [maximumConstraint],
    });
  }

  const minimumLength = argument.minimumLength ?? argument.minItems;
  const maximumLength = argument.maxItems;
  if (typeof minimumLength === "number") {
    pushCase(cases, base, "minimumLength", { status: "success" }, {
      value: collectionValue(contract, argument, minimumLength), constraints: ["minimumLength"],
    });
    if (minimumLength > 0) {
      pushCase(cases, base, "minimumLength-1", errorExpectation("RangeError"), {
        value: collectionValue(contract, argument, minimumLength - 1), constraints: ["minimumLength"],
      });
    }
  }
  if (typeof maximumLength === "number") {
    pushCase(cases, base, "maximumLength", { status: "success" }, {
      value: collectionValue(contract, argument, maximumLength), constraints: ["maximumLength"],
    });
    pushCase(cases, base, "maximumLength+1", errorExpectation("RangeError"), {
      value: collectionValue(contract, argument, maximumLength + 1), constraints: ["maximumLength"],
    });
  }

  if (argument.defaultFromCapability) {
    pushCase(cases, base, "default", { status: "success" }, {
      value: { omit: true }, constraints: ["defaultFromCapability"],
    });
  }
  if (argument.allowedFromCapability) {
    const allowed = capabilityValue(descriptor, argument.allowedFromCapability);
    if (Array.isArray(allowed) && allowed.length > 0) {
      pushCase(cases, base, "allowed", { status: "success" }, {
        value: symbolicAllowedValue(argument, allowed[0]), constraints: ["allowedFromCapability"],
      });
      pushCase(cases, base, "unavailable", errorExpectation("RangeError"), {
        value: unavailableAllowedValue(argument, allowed), constraints: ["allowedFromCapability"],
      });
    }
  }
  if (argument.conditionalAllowedFromCapability) {
    const condition = argument.conditionalAllowedFromCapability;
    const allowed = capabilityValue(descriptor, condition.allowedFromCapability);
    if (Array.isArray(allowed) && allowed.length > 0) {
      pushCase(cases, base, "conditionalAllowed", { status: "success" }, {
        value: allowed[0],
        fixture: { [condition.whenArgument]: condition.capabilityValue },
        constraints: ["conditionalAllowedFromCapability"],
      });
      pushCase(cases, base, "conditionalUnavailable", errorExpectation("RangeError"), {
        value: unavailableAllowedValue(argument, allowed),
        fixture: { [condition.whenArgument]: condition.capabilityValue },
        constraints: ["conditionalAllowedFromCapability"],
      });
    }
  }
  if (argument.maximumFromArgumentResource) {
    const relation = argument.maximumFromArgumentResource;
    pushCase(cases, base, "resourceMaximum", { status: "success" }, {
      value: { resourceBoundary: "maximum" }, fixture: { resource: relation },
      constraints: ["maximumFromArgumentResource"],
    });
    pushCase(cases, base, "resourceMaximum+1", errorExpectation("RangeError"), {
      value: { resourceBoundary: "maximum+1" }, fixture: { resource: relation },
      constraints: ["maximumFromArgumentResource"],
    });
  }
  if (argument.invalidResourceError) {
    pushCase(cases, base, "invalidResource", errorExpectation(argument.invalidResourceError), {
      value: { invalidResource: true }, constraints: ["invalidResourceError"],
    });
  }
  if (argument.maximumFromOnboardDevice) {
    const device = base.onboardDevice
      ? descriptor.board.devices[base.onboardDevice]
      : descriptor.board.devices;
    const maximumOnboard = resolvePath(device, argument.maximumFromOnboardDevice);
    if (typeof maximumOnboard === "number") {
      pushCase(cases, base, "onboardMaximum", { status: "success" }, {
        value: collectionValue(contract, argument, maximumOnboard), constraints: ["maximumFromOnboardDevice"],
      });
      pushCase(cases, base, "onboardMaximum+1", errorExpectation("RangeError"), {
        value: collectionValue(contract, argument, maximumOnboard + 1), constraints: ["maximumFromOnboardDevice"],
      });
    }
  }
  if (argument.exclusiveMaximumFromConfiguration) {
    pushCase(cases, base, "configurationMaximum-1", { status: "success" }, {
      value: { configurationBoundary: "maximum-1" },
      fixture: { configuration: argument.exclusiveMaximumFromConfiguration },
      constraints: ["exclusiveMaximumFromConfiguration"],
    });
    pushCase(cases, base, "configurationMaximum", errorExpectation("RangeError"), {
      value: { configurationBoundary: "maximum" },
      fixture: { configuration: argument.exclusiveMaximumFromConfiguration },
      constraints: ["exclusiveMaximumFromConfiguration"],
    });
  }
}

function pushRouteCases(cases, operationBase, definitions, descriptor) {
  const routeDefinitions = definitions.filter(({ argument }) => argument.routeFromCapability);
  if (routeDefinitions.length === 0) return;
  const path = routeDefinitions[0].argument.routeFromCapability.path;
  const routes = capabilityValue(descriptor, path);
  if (!Array.isArray(routes) || routes.length === 0) return;
  const route = structuredClone(routes[0]);
  cases.push({
    ...operationBase,
    id: `${operationBase.id}.route.valid`,
    kind: "boundary",
    expected: { status: "success" },
    value: route,
    constraints: ["routeFromCapability"],
  });
  const invalid = { ...route };
  const field = routeDefinitions[0].argument.routeFromCapability.field;
  invalid[field] = typeof invalid[field] === "number" ? invalid[field] + 1000 : "__invalid_route__";
  cases.push({
    ...operationBase,
    id: `${operationBase.id}.route.invalid`,
    kind: "boundary",
    expected: errorExpectation("RangeError"),
    value: invalid,
    constraints: ["routeFromCapability"],
  });
}

function pushOperationalCases(cases, contract, operationBase, signature) {
  const occurrences = new Map();
  const conditions = signature.operationalErrorConditions
    ?? (signature.operationalErrors ?? []).map((code) => ({ code }));
  for (const condition of conditions) {
    const definition = contract.errors.operational[condition.code];
    if (!definition) throw new Error(`unknown operational error ${condition.code}`);
    const occurrence = occurrences.get(condition.code) ?? 0;
    occurrences.set(condition.code, occurrence + 1);
    cases.push({
      ...operationBase,
      id: `${operationBase.id}.error.${condition.code}.${occurrence}`,
      kind: "operational-error",
      expected: errorExpectation(definition.class, condition.code, definition.name),
      fixture: condition,
      constraints: [condition.cause ?? "operationalError"],
    });
  }
}

function pushReturnCases(cases, contract, operationBase, signature, descriptor) {
  const returns = signature.returns ?? { types: ["undefined"] };
  const returnConstraints = [];
  for (const name of [
    "minimumFromCapability", "maximumFromCapability", "maximumFromCapabilityBitWidth",
  ]) if (returns[name]) returnConstraints.push(name);

  if (!returns.correlatesWithArgument) {
    cases.push({
      ...operationBase,
      id: `${operationBase.id}.return`,
      kind: "return",
      expected: { status: "success", returns },
      ...(returnConstraints.length ? { constraints: returnConstraints } : {}),
    });
    return;
  }

  const argumentName = returns.correlatesWithArgument;
  const argument = (signature.arguments ?? []).find((entry) => entry.name === argumentName);
  if (!argument) throw new Error(`correlated return references missing argument ${argumentName}`);
  const effective = effectiveArgument(contract, argument);
  const acceptedTypes = effective.javascriptTypes ?? [argument.type];
  const returnTypes = returns.types ?? [];
  const correlatedTypes = returnTypes.filter((typeName) => acceptedTypes.includes(typeName));
  if (!sameKeySet(correlatedTypes, acceptedTypes)
      || !sameKeySet(correlatedTypes, returnTypes)) {
    throw new Error(`correlated return types differ from argument ${argumentName} types`);
  }

  for (const typeName of correlatedTypes) {
    const selected = effectiveArgument(contract, { ...argument, type: typeName });
    const array = arrayArgument(contract, selected);
    let input;
    if (array) {
      const minimumLength = Math.max(
        1,
        argument.minimumLength ?? argument.minItems ?? 0,
        array.minimumLength ?? array.minItems ?? 0,
      );
      const maximumLength = maximumFor(selected, descriptor);
      if (typeof maximumLength === "number" && minimumLength > maximumLength) {
        throw new Error(`cannot create correlated ${typeName} fixture within its length limits`);
      }
      input = collectionValue(contract, selected, minimumLength);
    } else {
      const minimum = minimumFor(selected, descriptor);
      input = typeof minimum === "number" ? minimum : representativeValue(contract, selected);
    }
    cases.push({
      ...operationBase,
      id: `${operationBase.id}.return.correlates.${typeName}`,
      kind: "return",
      expected: { status: "success", returns: { ...returns, types: [typeName] } },
      fixture: { arguments: { [argumentName]: input } },
      constraints: [...returnConstraints, "correlatesWithArgument"],
    });
  }
}

function generateContractCases(contract, descriptor) {
  const cases = [];
  for (const [moduleName, moduleDefinition] of Object.entries(contract.modules)) {
    if (!moduleAvailable(moduleDefinition, descriptor)) continue;
    for (const [exportName, exportDefinition] of Object.entries(moduleDefinition.exports ?? {})) {
      if (exportDefinition.kind !== "method"
          || !exportAvailable(moduleName, exportDefinition, descriptor)) continue;
      const signatures = exportDefinition.signatures ?? [];
      for (const [name, implicit] of Object.entries(exportDefinition.implicitConfiguration ?? {})) {
        if (!implicit.allowedFromCapability) continue;
        cases.push({
          id: `${moduleName}.${exportName}.implicit.${name}.allowed`,
          kind: "configuration",
          module: moduleName,
          operation: exportName,
          expected: { status: "success" },
          value: implicit.value,
          constraints: ["allowedFromCapability"],
        });
      }
      signatures.forEach((signature, signatureIndex) => {
        const signatureSuffix = signatures.length > 1 ? `.signature${signatureIndex}` : "";
        const operationId = `${moduleName}.${exportName}${signatureSuffix}`;
        const operationBase = {
          id: operationId,
          module: moduleName,
          operation: exportName,
          signature: signatureIndex,
        };
        pushReturnCases(cases, contract, operationBase, signature, descriptor);

        const definitions = [];
        for (const argument of signature.arguments ?? []) {
          const argumentSet = argumentDefinitions(contract, argument);
          definitions.push(...argumentSet);
          for (const {
            argument: boundary, path, argumentPath, ancestry, variantRoot,
          } of argumentSet) {
            const firstCase = cases.length;
            pushArgumentCases(cases, {
              ...operationBase,
              id: `${operationId}.${path}`,
              kind: "boundary",
              argument: argument.name,
              argumentPath,
              ...(exportDefinition.onboardDevice
                ? { onboardDevice: exportDefinition.onboardDevice }
                : {}),
            }, boundary, contract, descriptor, { variantRoot });
            if (ancestry.length > 0) {
              for (const testCase of cases.slice(firstCase)) {
                if (Object.hasOwn(testCase, "value")) {
                  testCase.value = materializeArgumentValue(contract, ancestry, testCase.value);
                }
              }
            }
          }
          const rootType = contract.types[argument.type];
          if (rootType?.defaultRouteFromCapability) {
            cases.push({
              ...operationBase,
              id: `${operationId}.${argument.name}.defaultRoute`,
              kind: "boundary",
              argument: argument.name,
              argumentPath: argument.name,
              expected: { status: "success" },
              value: { omitRoute: true },
              constraints: ["defaultRouteFromCapability"],
            });
          }
        }
        pushRouteCases(cases, operationBase, definitions, descriptor);
        pushOperationalCases(cases, contract, operationBase, signature);
      });
    }
  }
  const ids = cases.map((entry) => entry.id);
  if (new Set(ids).size !== ids.length) throw new Error("generated duplicate conformance case IDs");
  return cases;
}

function observedMatches(testCase, observed, descriptor) {
  if (!observed || observed.status !== testCase.expected.status) return false;
  if (testCase.expected.status === "throw") {
    return observed.errorClass === testCase.expected.errorClass
      && (testCase.expected.code ?? null) === (observed.code ?? null)
      && (!testCase.expected.name || observed.name === testCase.expected.name)
      && typeof observed.message === "string"
      && observed.message.length > 0;
  }
  if (testCase.kind !== "return") return true;
  const returns = testCase.expected.returns;
  const types = returns.types ?? [];
  const matching = types.some((type) => {
    if (type === "integer") return observed.valueType === "number" && Number.isInteger(observed.value);
    if (type === "number") return observed.valueType === "number";
    if (type === "uint8") {
      return observed.valueType === "number" && Number.isInteger(observed.value)
        && observed.value >= 0 && observed.value <= 255;
    }
    if (type === "byteArray") {
      return observed.valueType === "array" && Array.isArray(observed.value)
        && observed.value.every((value) => Number.isInteger(value) && value >= 0 && value <= 255);
    }
    if (type === "stringArray") {
      return observed.valueType === "array" && Array.isArray(observed.value)
        && observed.value.every((value) => typeof value === "string");
    }
    if (type === "capabilityDescriptor" || type === "capabilityMap") {
      return observed.valueType === "object" && observed.value !== null
        && !Array.isArray(observed.value);
    }
    return observed.valueType === type;
  });
  if (!matching || (returns.unit && observed.unit !== returns.unit)) return false;
  if (returns.correlatesWithArgument) {
    const argumentsFixture = testCase.fixture?.arguments;
    if (!argumentsFixture
        || !Object.hasOwn(argumentsFixture, returns.correlatesWithArgument)) return false;
    const input = argumentsFixture[returns.correlatesWithArgument];
    if (Array.isArray(input)) {
      if (!Array.isArray(observed.value) || observed.value.length !== input.length) return false;
    } else if (Array.isArray(observed.value)) {
      return false;
    }
  }
  if (observed.valueType === "number") {
    const minimum = returns.minimumFromCapability
      ? capabilityValue(descriptor, returns.minimumFromCapability)
      : returns.minimum;
    let maximum = returns.maximumFromCapability
      ? capabilityValue(descriptor, returns.maximumFromCapability)
      : returns.maximum;
    if (returns.maximumFromCapabilityBitWidth) {
      const bits = capabilityValue(descriptor, returns.maximumFromCapabilityBitWidth);
      maximum = (2 ** bits) - 1;
    }
    if (!Number.isFinite(observed.value)
        || (typeof minimum === "number" && observed.value < minimum)
        || (typeof maximum === "number" && observed.value > maximum)) return false;
  }
  return true;
}

function runPortableApiConformance({ adapter, contract, descriptor, lane }) {
  const surface = expectedRuntimeSurface(contract, descriptor);
  const candidates = unique([
    ...descriptor.modules,
    ...Object.keys(contract.modules),
    "node:module",
    "__missing_module__",
  ]);
  const cases = generateContractCases(contract, descriptor).map((testCase) => {
    const observed = adapter.runCase(testCase);
    if (!observed || observed.status === "skip") {
      return {
        id: testCase.id,
        kind: testCase.kind,
        status: "skip",
        expected: testCase.expected,
        reason: observed?.reason ?? "adapter did not execute this production case",
      };
    }
    return {
      id: testCase.id,
      kind: testCase.kind,
      status: observedMatches(testCase, observed, descriptor) ? "pass" : "fail",
      expected: testCase.expected,
      observed,
    };
  });
  const exports = {};
  for (const moduleName of Object.keys(surface.exports)) exports[moduleName] = adapter.exportNames(moduleName);
  return {
    formatVersion: FORMAT_VERSION,
    apiVersion: contract.versioning.apiVersion,
    board: descriptor.board.name,
    lane,
    discovery: {
      registered: adapter.registeredModules(),
      builtinModules: adapter.builtinModules(),
      has: Object.fromEntries(candidates.map((name) => [name, adapter.hasModule(name)])),
      help: adapter.helpModules(),
      capabilities: adapter.capabilityNames(),
      exports,
    },
    cases,
    summary: {
      passed: cases.filter((entry) => entry.status === "pass").length,
      failed: cases.filter((entry) => entry.status === "fail").length,
      skipped: cases.filter((entry) => entry.status === "skip").length,
    },
  };
}

function createSerialPlan({ contract, descriptor, helpModules }) {
  if (!Array.isArray(helpModules)) throw new TypeError("serial plan requires observed help modules");
  const surface = expectedRuntimeSurface(contract, descriptor);
  return {
    candidates: unique([
      ...descriptor.modules,
      ...Object.keys(contract.modules),
      "node:module",
      "__missing_module__",
    ]),
    cases: generateContractCases(contract, descriptor)
      .map(({ id, kind, expected }) => ({ id, kind, expected })),
    helpModules: [...helpModules],
    exportNames: surface.exports,
  };
}

function validationError(code, message) {
  return { code, message };
}

function validateEvidence({
  contract, descriptor, evidence, allowSkipped = false, expectedLane,
}) {
  const errors = [];
  const expected = expectedRuntimeSurface(contract, descriptor);
  if (evidence?.formatVersion !== FORMAT_VERSION) {
    errors.push(validationError("FORMAT_VERSION", "unsupported conformance evidence format"));
  }
  if (evidence?.apiVersion !== contract.versioning.apiVersion) {
    errors.push(validationError("API_VERSION", "evidence API version does not match the contract"));
  }
  if (evidence?.board !== descriptor.board.name) {
    errors.push(validationError("BOARD_ID", "evidence board does not match the descriptor"));
  }
  if (typeof evidence?.lane !== "string" || evidence.lane.length === 0
      || (expectedLane !== undefined && evidence.lane !== expectedLane)) {
    errors.push(validationError("LANE_MISMATCH", "evidence lane is missing or unexpected"));
  }

  const discovery = evidence?.discovery ?? {};
  if (!sameArray(discovery.registered, expected.modules)) {
    errors.push(validationError("DISCOVERY_REGISTERED_MISMATCH", "native registration differs from the descriptor"));
  }
  if (!sameArray(discovery.builtinModules, expected.modules)) {
    errors.push(validationError("DISCOVERY_BUILTINS_MISMATCH", "builtinModules differs from native registration"));
  }
  if (!sameArray(discovery.help, expected.modules)) {
    errors.push(validationError("DISCOVERY_HELP_MISMATCH", "REPL help differs from native registration"));
  }
  if (!sameArray(discovery.capabilities, expected.capabilities)) {
    errors.push(validationError("DISCOVERY_CAPABILITIES_MISMATCH", "capability discovery differs from the descriptor"));
  }

  const hasCandidates = unique([
    ...expected.modules,
    ...Object.keys(contract.modules),
    "node:module",
    "__missing_module__",
  ]);
  const hasKeys = Object.keys(discovery.has ?? {});
  if (!sameKeySet(hasKeys, hasCandidates)) {
    errors.push(validationError("DISCOVERY_HAS_KEYS", "modules.has evidence has missing or unknown keys"));
  }
  for (const name of hasCandidates) {
    if (!Object.hasOwn(discovery.has ?? {}, name)
        || discovery.has[name] !== expected.modules.includes(name)) {
      errors.push(validationError("DISCOVERY_HAS_MISMATCH", `modules.has(${name}) disagrees with builtinModules`));
      break;
    }
  }

  if (!sameKeySet(Object.keys(discovery.exports ?? {}), Object.keys(expected.exports))) {
    errors.push(validationError("EXPORT_SURFACE_MISMATCH", "module export evidence differs from contract availability"));
  }
  for (const [moduleName, exportNames] of Object.entries(expected.exports)) {
    if (!sameArray(discovery.exports?.[moduleName], exportNames)) {
      errors.push(validationError("EXPORT_SURFACE_MISMATCH", `${moduleName} exports differ from contract availability`));
    }
  }

  const expectedCases = generateContractCases(contract, descriptor);
  const expectedById = new Map(expectedCases.map((entry) => [entry.id, entry]));
  const evidenceById = new Map();
  for (const entry of Array.isArray(evidence?.cases) ? evidence.cases : []) {
    if (evidenceById.has(entry.id)) {
      errors.push(validationError("DUPLICATE_CASE", `duplicate case ${entry.id}`));
    }
    if (!expectedById.has(entry.id)) {
      errors.push(validationError("EXTRA_CASE", `unknown case ${entry.id}`));
    }
    evidenceById.set(entry.id, entry);
  }
  for (const testCase of expectedCases) {
    const entry = evidenceById.get(testCase.id);
    if (!entry) {
      errors.push(validationError("MISSING_CASE", `missing case ${testCase.id}`));
      continue;
    }
    if (entry.kind !== testCase.kind) {
      errors.push(validationError("CASE_KIND_MISMATCH", `case ${testCase.id} kind changed`));
    }
    if (!Object.hasOwn(entry, "expected") || !sameJson(entry.expected, testCase.expected)) {
      errors.push(validationError("CASE_EXPECTATION_MISMATCH", `case ${testCase.id} expectation changed`));
    }
    if (entry.status === "skip") {
      if (!allowSkipped || typeof entry.reason !== "string" || entry.reason.length === 0) {
        errors.push(validationError("FAILED_CASE", `case ${testCase.id} was skipped without permission/reason`));
      }
    } else if (entry.status !== "pass" || !observedMatches(testCase, entry.observed, descriptor)) {
      errors.push(validationError("FAILED_CASE", `case ${testCase.id} did not pass`));
    }
  }

  const actualSummary = {
    passed: (evidence?.cases ?? []).filter((entry) => entry.status === "pass").length,
    failed: (evidence?.cases ?? []).filter((entry) => entry.status === "fail").length,
    skipped: (evidence?.cases ?? []).filter((entry) => entry.status === "skip").length,
  };
  if (!sameJson(evidence?.summary, actualSummary)) {
    errors.push(validationError("SUMMARY_MISMATCH", "case summary does not match case statuses"));
  }
  return { valid: errors.length === 0, errors };
}

function formatEvidenceLine(evidence) {
  return `${CONFORMANCE_FRAME_START}${JSON.stringify(evidence)}${CONFORMANCE_FRAME_END}`;
}

function parseEvidenceLines(transcript) {
  const evidence = [];
  for (const rawLine of transcript.split(/\r?\n/)) {
    if (!rawLine.startsWith(CONFORMANCE_FRAME_START)
        || !rawLine.endsWith(CONFORMANCE_FRAME_END)) continue;
    const json = rawLine.slice(CONFORMANCE_FRAME_START.length, -CONFORMANCE_FRAME_END.length);
    const value = JSON.parse(json);
    if (value.formatVersion !== FORMAT_VERSION) {
      throw new Error(`Unsupported conformance evidence format: ${value.formatVersion}`);
    }
    evidence.push(value);
  }
  return evidence;
}

module.exports = {
  CONFORMANCE_FRAME_END,
  CONFORMANCE_FRAME_START,
  CONFORMANCE_PREFIX,
  createSerialPlan,
  expectedRuntimeSurface,
  formatEvidenceLine,
  generateContractCases,
  parseEvidenceLines,
  runPortableApiConformance,
  validateEvidence,
};
