const assert = require("node:assert/strict");
const { readdirSync, readFileSync } = require("node:fs");
const { createRequire } = require("node:module");
const { basename, join } = require("node:path");
const test = require("node:test");

const docsRequire = createRequire(join(__dirname, "../docs/package.json"));
const Ajv2020 = docsRequire("ajv/dist/2020").default;
const {
  validatePortableApiContract,
  validatePortableApiManifest,
} = require("../scripts/validate-portable-api-manifest.js");

const schemaUrl = join(
  __dirname,
  "../docs/static/schemas/mcujs-portable-api-0.2.schema.json",
);
const apiDesignUrl = join(
  __dirname,
  "../docs/docs/api-design/portable-api-contract.md",
);
const migrationUrl = join(
  __dirname,
  "../docs/docs/migration/0.2.md",
);
const portableApiDesignUrl = join(
  __dirname,
  "../docs/docs/development/mcujs-0.2-portable-api.md",
);

function loadSchema() {
  return JSON.parse(readFileSync(schemaUrl, "utf8"));
}

function sorted(values) {
  return [...values].sort();
}

function walkCFiles(directory) {
  return readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) {
      return walkCFiles(path);
    }
    return entry.isFile() && entry.name.endsWith(".c") ? [path] : [];
  });
}

function collectLiveCompatibilityAliasNames(contract) {
  const names = new Set();
  const platformDirectory = join(__dirname, "../platform");

  for (const path of walkCFiles(platformDirectory)) {
    const source = readFileSync(path, "utf8");
    const moduleName = basename(path, ".c");
    if (!contract.modules[moduleName]) {
      continue;
    }

    for (const match of source.matchAll(/js_register_global\("([^"]+)"/g)) {
      names.add(`global.${match[1]}`);
    }

    if (moduleName === "board" || moduleName === "adc") {
      const objectName = moduleName;
      for (const match of source.matchAll(
        new RegExp(`js_set_(?:number|string|boolean)\\(${objectName}, \\"([^\\"]+)\\"`, "g"),
      )) {
        if (!contract.modules[moduleName].exports[match[1]]) {
          names.add(`${moduleName}.${match[1]}`);
        }
      }
    }
  }

  const requireSource = readFileSync(join(__dirname, "../host/bindings/require.c"), "utf8");
  for (const match of requireSource.matchAll(
    /strcmp\(specifier, "([^"]+)"\) == 0\) \{\s*lookup = "([^"]+)";/g,
  )) {
    names.add(`require('${match[1]}')`);
  }

  for (const [moduleName, moduleContract] of Object.entries(contract.modules)) {
    for (const [exportName, exported] of Object.entries(moduleContract.exports)) {
      for (const signature of exported.signatures ?? []) {
        if (signature.compatibility) {
          names.add(signature.compatibility);
          assert.equal(
            signature.compatibility,
            `${moduleName}.${exportName}(positional)`,
            `${moduleName}.${exportName} compatibility overload is not source-addressable`,
          );
        }
      }
    }
  }

  return names;
}

function validManifest() {
  return {
    formatVersion: 1,
    apiVersion: "0.2",
    board: {
      name: "portable-test-board",
      chip: "TEST-MCU",
      firmwareVersion: "0.2.0",
      exposedPins: [1, 2, 3, 4, 5, 6],
      pins: { D0: 1, A0: 1, SDA: 2, SCL: 3, SCK: 4, MOSI: 5, MISO: 6 },
      devices: {},
    },
    capabilities: {
      gpio: {
        pins: [1, 2, 3, 4, 5, 6],
        outputPins: [1, 2, 3, 4, 5, 6],
        modes: ["input", "output"],
      },
      pwm: {
        pins: [1],
        maxOutputs: 1,
        timerCount: 1,
        duty: { min: 0, max: 1, unit: "ratio" },
        frequency: { minHz: 1, maxHz: 1000000, resolutionVaries: true },
      },
      adc: {
        resolutionBits: 12,
        pins: [1],
        channels: [{ channel: 0, pin: 1, aliases: ["A0"] }],
        voltage: { supported: true, calibrated: true, minVolts: 0, maxVolts: 3.3 },
        temperature: { supported: false },
        vsys: false,
      },
      boot: { safeMode: true },
      i2c: {
        buses: [0],
        routes: [{ bus: 0, sda: 2, scl: 3 }],
        defaultBus: 0,
        defaultRoute: { bus: 0, sda: 2, scl: 3 },
        frequency: { minHz: 10000, maxHz: 1000000 },
        maxTransferBytes: 256,
      },
      spi: {
        buses: [0],
        routes: [{ bus: 0, sck: 4, mosi: 5, miso: 6 }],
        defaultBus: 0,
        defaultRoute: { bus: 0, sck: 4, mosi: 5, miso: 6 },
        frequency: { minHz: 10000, maxHz: 8000000 },
        maxTransferBytes: 64,
        modes: [0],
        bitsPerWord: [8],
        bitOrders: ["msb"],
        fullDuplex: true,
        dma: false,
      },
      neopixel: {
        pins: [1],
        maxLength: 16,
        orders: ["RGB", "GRB"],
      },
      image: {
        methods: ["info", "decodeJPEG", "decodeBMP", "drawJPEG", "drawBMP"],
        formats: {
          jpeg: { profiles: ["baseline"] },
          bmp: {
            variants: [
              { bitsPerPixel: 16, pixelFormat: "rgb565", compression: ["none", "rgb565-bitfields"] },
              { bitsPerPixel: 24, pixelFormat: "bgr888", compression: ["none"] },
              { bitsPerPixel: 32, pixelFormat: "bgra8888", alpha: "ignored", compression: ["none"] },
            ],
            maxWidth: 4096,
            maxHeight: 4096,
          },
        },
        maxInputBytes: 16384,
        destination: { pixelFormat: "rgb565", byteOrders: ["swapped", "native"] },
      },
      fs: { implementation: "fat", writable: true, hostTransfer: true },
      usb: { classes: ["cdc", "msc"] },
    },
  };
}

function collectTypeReferences(contract) {
  const references = [];
  for (const [moduleName, moduleContract] of Object.entries(contract.modules)) {
    for (const [exportName, exported] of Object.entries(moduleContract.exports)) {
      if (exported.type) {
        references.push([`${moduleName}.${exportName}`, exported.type]);
      }
      for (const [index, signature] of (exported.signatures ?? []).entries()) {
        for (const argument of signature.arguments) {
          references.push([
            `${moduleName}.${exportName} signature ${index} argument ${argument.name}`,
            argument.type,
          ]);
        }
        for (const returnType of signature.returns.types) {
          references.push([
            `${moduleName}.${exportName} signature ${index} return`,
            returnType,
          ]);
        }
      }
    }
  }
  return references;
}

function resolveProjectedPath(root, path) {
  let values = [root];
  for (const segment of path.split(".")) {
    const projected = segment.endsWith("[]");
    const key = projected ? segment.slice(0, -2) : segment;
    values = values.flatMap((value) => {
      assert.ok(
        value !== null && typeof value === "object" && key in value,
        `unresolved contract path ${path} at ${key}`,
      );
      const next = value[key];
      if (!projected) {
        return [next];
      }
      assert.ok(Array.isArray(next), `${path} projects non-array ${key}`);
      return next;
    });
  }
  return values;
}

function collectOperationReferences(contract) {
  const references = [];
  const referenceNames = new Set(Object.keys(contract.operationConstraints.fields));

  function walk(value, location) {
    if (Array.isArray(value)) {
      value.forEach((entry, index) => walk(entry, `${location}[${index}]`));
      return;
    }
    if (value === null || typeof value !== "object") {
      return;
    }
    for (const [key, entry] of Object.entries(value)) {
      if (referenceNames.has(key)) {
        references.push([`${location}.${key}`, key, entry]);
      }
      walk(entry, `${location}.${key}`);
    }
  }

  walk(contract.types, "types");
  walk(contract.modules, "modules");
  return references;
}

test("the 0.2 contract is a strict Draft 2020-12 capability-manifest schema", () => {
  const schema = loadSchema();

  assert.equal(schema.$schema, "https://json-schema.org/draft/2020-12/schema");
  assert.equal(
    schema.$id,
    "https://mcujs.org/schemas/mcujs-portable-api-0.2.schema.json",
  );
  assert.equal(schema.type, "object");
  assert.equal(schema.additionalProperties, false);
  assert.deepEqual(
    sorted(schema.required),
    sorted(["formatVersion", "apiVersion", "board", "capabilities"]),
  );
  assert.equal(schema.properties.formatVersion.const, 1);
  assert.equal(schema.properties.apiVersion.const, "0.2");
  assert.equal(schema.properties.board.$ref, "#/$defs/boardIdentity");
  assert.equal(schema.properties.capabilities.$ref, "#/$defs/capabilityMap");
});

test("the standard schema accepts static manifests and rejects dynamic descriptor state", () => {
  const schema = loadSchema();
  const ajv = new Ajv2020({ allErrors: true, strict: true });
  ajv.addKeyword({ keyword: "x-mcujs-contract" });
  const validate = ajv.compile(schema);
  const manifest = {
    formatVersion: 1,
    apiVersion: "0.2",
    board: {
      name: "seeed_xiao_esp32s3",
      chip: "ESP32-S3",
      firmwareVersion: "0.2.0",
      exposedPins: [1],
      pins: { D0: 1, A0: 1 },
      devices: {},
    },
    capabilities: {
      gpio: {
        pins: [1],
        outputPins: [1],
        modes: ["input", "output"],
      },
      usb: { classes: ["cdc", "msc"] },
    },
  };

  assert.equal(validate(manifest), true, JSON.stringify(validate.errors));

  const dynamicManifest = structuredClone(manifest);
  dynamicManifest.capabilities.gpio.ready = true;
  assert.equal(validate(dynamicManifest), false);
  assert.ok(
    validate.errors.some(
      (error) =>
        error.keyword === "additionalProperties" &&
        error.params.additionalProperty === "ready",
    ),
    JSON.stringify(validate.errors),
  );
});

test("the mandatory manifest validator rejects contradictory capability descriptors", () => {
  const manifestValidation = loadSchema()["x-mcujs-contract"].manifestValidation;
  assert.equal(
    manifestValidation.enforcement,
    "structuralAndSemanticRequired",
  );
  assert.equal(
    manifestValidation.semanticValidator,
    "scripts/validate-portable-api-manifest.js",
  );
  const valid = validatePortableApiManifest(validManifest());
  assert.equal(valid.valid, true, JSON.stringify(valid.errors));

  const invalidCases = [
    {
      name: "GPIO output pin outside the GPIO pin set",
      mutate(manifest) {
        manifest.capabilities.gpio.outputPins = [7];
      },
      constraint: "gpio.outputPinsSubset",
    },
    {
      name: "PWM minimum frequency above its maximum",
      mutate(manifest) {
        manifest.capabilities.pwm.frequency = {
          minHz: 1001,
          maxHz: 1000,
          resolutionVaries: true,
        };
      },
      constraint: "pwm.frequencyRange",
    },
    {
      name: "PWM output count above its pin count",
      mutate(manifest) {
        manifest.capabilities.pwm.maxOutputs = 2;
      },
      constraint: "pwm.maxOutputs",
    },
    {
      name: "PWM timer count above its output count",
      mutate(manifest) {
        manifest.capabilities.pwm.timerCount = 2;
      },
      constraint: "pwm.timerCount",
    },
    {
      name: "I2C default bus absent from buses and routes",
      mutate(manifest) {
        manifest.capabilities.i2c.defaultBus = 1;
      },
      constraint: "i2c.defaultBus",
    },
    {
      name: "I2C minimum frequency above its maximum",
      mutate(manifest) {
        manifest.capabilities.i2c.frequency = { minHz: 1001, maxHz: 1000 };
      },
      constraint: "i2c.frequencyRange",
    },
    {
      name: "SPI default bus absent from buses and routes",
      mutate(manifest) {
        manifest.capabilities.spi.defaultBus = 1;
      },
      constraint: "spi.defaultBus",
    },
    {
      name: "SPI minimum frequency above its maximum",
      mutate(manifest) {
        manifest.capabilities.spi.frequency = { minHz: 1001, maxHz: 1000 };
      },
      constraint: "spi.frequencyRange",
    },
    {
      name: "ADC minimum voltage above its maximum",
      mutate(manifest) {
        manifest.capabilities.adc.voltage.minVolts = 3.3;
        manifest.capabilities.adc.voltage.maxVolts = 1.8;
      },
      constraint: "adc.voltageRange",
    },
    {
      name: "ADC channel pin absent from the ADC pin set",
      mutate(manifest) {
        manifest.capabilities.adc.channels[0].pin = 2;
      },
      constraint: "adc.channelPins",
    },
    {
      name: "ADC alias not resolved by board pins",
      mutate(manifest) {
        manifest.capabilities.adc.channels[0].aliases = ["SCL"];
      },
      constraint: "adc.channelAliases",
    },
    {
      name: "ADC voltage support without executable raw routes",
      mutate(manifest) {
        manifest.capabilities.adc.pins = [];
        manifest.capabilities.adc.channels = [];
      },
      schemaKeyword: "minItems",
    },
    {
      name: "NeoPixel capability without a pin",
      mutate(manifest) {
        manifest.capabilities.neopixel.pins = [];
      },
      schemaKeyword: "minItems",
    },
    {
      name: "NeoPixel capability without an order",
      mutate(manifest) {
        manifest.capabilities.neopixel.orders = [];
      },
      schemaKeyword: "minItems",
    },
  ];

  for (const invalidCase of invalidCases) {
    const manifest = validManifest();
    invalidCase.mutate(manifest);
    const result = validatePortableApiManifest(manifest);
    assert.equal(result.valid, false, invalidCase.name);
    if (invalidCase.constraint) {
      assert.ok(
        result.errors.some(
          (error) => error.params?.constraint === invalidCase.constraint,
        ),
        `${invalidCase.name}: ${JSON.stringify(result.errors)}`,
      );
    }
    if (invalidCase.schemaKeyword) {
      assert.ok(
        result.errors.some((error) => error.keyword === invalidCase.schemaKeyword),
        `${invalidCase.name}: ${JSON.stringify(result.errors)}`,
      );
    }
  }
});

test("the semantic validator enforces exposed pins, route coherence, and onboard limits", () => {
  const declaredConstraints = loadSchema()["x-mcujs-contract"].manifestValidation.constraints;
  const invalidCases = [
    ["board alias outside exposed pins", "board.pinAliasesExposed", (manifest) => {
      manifest.board.pins.D7 = 7;
    }],
    ["GPIO pin outside exposed pins", "gpio.pinsExposed", (manifest) => {
      manifest.capabilities.gpio.pins.push(7);
    }],
    ["GPIO output pins without output mode", "gpio.outputMode", (manifest) => {
      manifest.capabilities.gpio.modes = ["input"];
    }],
    ["GPIO output mode without output pins", "gpio.outputMode", (manifest) => {
      manifest.capabilities.gpio.outputPins = [];
    }],
    ["GPIO onboard LED pin outside exposed pins", "onboard.ledPinExposed", (manifest) => {
      manifest.board.devices.led = { type: "gpio", pin: 7, activeLow: true };
    }],
    ["PWM pin outside exposed pins", "pwm.pinsExposed", (manifest) => {
      manifest.capabilities.pwm.pins = [7];
    }],
    ["ADC pin outside exposed pins", "adc.pinsExposed", (manifest) => {
      manifest.capabilities.adc.pins[0] = 7;
      manifest.capabilities.adc.channels[0].pin = 7;
      manifest.board.pins.A0 = 7;
    }],
    ["zero-width ADC voltage range", "adc.voltageRange", (manifest) => {
      manifest.capabilities.adc.voltage.minVolts = 3.3;
      manifest.capabilities.adc.voltage.maxVolts = 3.3;
    }],

    ["I2C reuses one pin for SDA and SCL", "i2c.distinctRoutePins", (manifest) => {
      manifest.capabilities.i2c.routes[0].scl = 2;
    }],
    ["I2C route pin outside exposed pins", "i2c.routePinsExposed", (manifest) => {
      manifest.capabilities.i2c.routes[0].sda = 7;
    }],
    ["I2C advertised bus without a route", "i2c.busRoutes", (manifest) => {
      manifest.capabilities.i2c.buses.push(1);
    }],
    ["full-duplex SPI reuses role pins", "spi.distinctRoutePins", (manifest) => {
      manifest.capabilities.spi.routes[0] = { bus: 0, sck: 4, mosi: 4, miso: 4 };
    }],
    ["half-duplex SPI reuses SCK as a data pin", "spi.distinctRoutePins", (manifest) => {
      manifest.capabilities.spi.fullDuplex = false;
      manifest.capabilities.spi.routes[0] = { bus: 0, sck: 4, mosi: 5, miso: 4 };
    }],
    ["SPI route pin outside exposed pins", "spi.routePinsExposed", (manifest) => {
      manifest.capabilities.spi.routes[0].sck = 7;
    }],
    ["SPI advertised bus without a route", "spi.busRoutes", (manifest) => {
      manifest.capabilities.spi.buses.push(1);
    }],
    ["SPI capability omits the contract default mode", "spi.defaultMode", (manifest) => {
      manifest.capabilities.spi.modes = [1];
    }],
    ["NeoPixel driver pin outside exposed pins", "neopixel.pinsExposed", (manifest) => {
      manifest.capabilities.neopixel.pins = [7];
    }],
    ["NeoPixel capability omits the contract default order", "neopixel.defaultOrder", (manifest) => {
      manifest.capabilities.neopixel.orders = ["RGB"];
    }],
    ["onboard NeoPixel above driver maximum", "onboard.neopixelLength", (manifest) => {
      manifest.board.devices.neopixel = {
        type: "neopixel", pin: 1, length: 17, order: "GRB",
      };
    }],
    ["onboard NeoPixel pin absent from driver", "onboard.neopixelPin", (manifest) => {
      manifest.board.devices.neopixel = {
        type: "neopixel", pin: 2, length: 1, order: "GRB",
      };
    }],
    ["onboard NeoPixel without driver capability", "onboard.neopixelCapability", (manifest) => {
      manifest.board.devices.neopixel = {
        type: "neopixel", pin: 1, length: 1, order: "GRB",
      };
      delete manifest.capabilities.neopixel;
    }],
    ["onboard NeoPixel order absent from driver", "onboard.neopixelOrder", (manifest) => {
      manifest.capabilities.neopixel.orders = ["GRB"];
      manifest.board.devices.neopixel = {
        type: "neopixel", pin: 1, length: 1, order: "RGB",
      };
    }],
  ];

  for (const [name, constraint, mutate] of invalidCases) {
    assert.equal(typeof declaredConstraints[constraint], "string", `${constraint} is undocumented`);
    const manifest = validManifest();
    mutate(manifest);
    const result = validatePortableApiManifest(manifest);
    assert.equal(result.valid, false, name);
    assert.ok(
      result.errors.some((error) => error.params?.constraint === constraint),
      `${name}: ${JSON.stringify(result.errors)}`,
    );
  }
});

test("bus options resolve omitted pins through one explicit default route", () => {
  const schema = loadSchema();
  const contract = schema["x-mcujs-contract"];

  for (const [name, roles] of Object.entries({
    i2c: ["sda", "scl"],
    spi: ["sck", "mosi", "miso"],
  })) {
    const capabilityDefinition = schema.$defs[`${name}Capability`];
    assert.ok(capabilityDefinition.required.includes("defaultRoute"));
    assert.equal(
      capabilityDefinition.properties.defaultRoute.$ref,
      `#/$defs/${name}Route`,
    );

    const options = contract.types[`${name}Options`];
    assert.equal(options.defaultRouteFromCapability, `${name}.defaultRoute`);
    assert.match(options.routeRule, /bus omitted\/equal to defaultBus/);
    for (const role of roles) {
      assert.equal(
        options.fields[role].defaultFromCapability,
        `${name}.defaultRoute.${role}`,
      );
    }

    const manifest = validManifest();
    manifest.capabilities[name].defaultRoute = structuredClone(
      manifest.capabilities[name].routes[0],
    );
    manifest.capabilities[name].routes.push({
      ...manifest.capabilities[name].routes[0],
      [roles[0]]: 1,
    });
    const explicit = validatePortableApiManifest(manifest);
    assert.equal(explicit.valid, true, `${name}: ${JSON.stringify(explicit.errors)}`);

    const missing = structuredClone(manifest);
    delete missing.capabilities[name].defaultRoute;
    const missingResult = validatePortableApiManifest(missing);
    assert.equal(missingResult.valid, false, `${name} accepted an ambiguous omitted route`);
    assert.ok(
      missingResult.errors.some(
        (error) => error.keyword === "required" && error.params.missingProperty === "defaultRoute",
      ),
      `${name}: ${JSON.stringify(missingResult.errors)}`,
    );

    const unlisted = structuredClone(manifest);
    unlisted.capabilities[name].defaultRoute = {
      ...unlisted.capabilities[name].defaultRoute,
      [roles[0]]: 6,
    };
    const unlistedResult = validatePortableApiManifest(unlisted);
    assert.equal(unlistedResult.valid, false, `${name} accepted an unlisted default route`);
    assert.ok(
      unlistedResult.errors.some(
        (error) => error.params?.constraint === `${name}.defaultRoute`,
      ),
      `${name}: ${JSON.stringify(unlistedResult.errors)}`,
    );

    const wrongBus = structuredClone(manifest);
    wrongBus.capabilities[name].buses.push(1);
    wrongBus.capabilities[name].routes.push({
      ...wrongBus.capabilities[name].defaultRoute,
      bus: 1,
    });
    wrongBus.capabilities[name].defaultBus = 1;
    const wrongBusResult = validatePortableApiManifest(wrongBus);
    assert.equal(wrongBusResult.valid, false, `${name} accepted a default route for another bus`);
    assert.ok(
      wrongBusResult.errors.some(
        (error) => error.params?.constraint === `${name}.defaultRoute`,
      ),
      `${name}: ${JSON.stringify(wrongBusResult.errors)}`,
    );
  }
});

test("the onboard LED inventory supports GPIO and transport-neutral managed devices", () => {
  const gpioManifest = validManifest();
  gpioManifest.board.devices.led = { type: "gpio", pin: 1, activeLow: true };
  const gpioResult = validatePortableApiManifest(gpioManifest);
  assert.equal(gpioResult.valid, true, JSON.stringify(gpioResult.errors));

  const managedManifest = validManifest();
  managedManifest.board.devices.led = { type: "managed" };
  const managedResult = validatePortableApiManifest(managedManifest);
  assert.equal(managedResult.valid, true, JSON.stringify(managedResult.errors));

  const neopixelManifest = validManifest();
  neopixelManifest.board.devices.neopixel = {
    type: "neopixel", pin: 1, length: 1, order: "GRB",
  };
  const neopixelResult = validatePortableApiManifest(neopixelManifest);
  assert.equal(neopixelResult.valid, true, JSON.stringify(neopixelResult.errors));
});

test("the schema freezes the three availability states", () => {
  const { availabilityStates } = loadSchema()["x-mcujs-contract"];

  assert.deepEqual(sorted(Object.keys(availabilityStates)), [
    "temporarilyUnavailableOrFailed",
    "unsupported",
    "unsupportedConfiguration",
  ]);
  assert.equal(availabilityStates.unsupported.apiPresence, "absent");
  assert.equal(availabilityStates.unsupportedConfiguration.apiPresence, "present");
  assert.equal(availabilityStates.unsupportedConfiguration.behavior, "throw");
  assert.equal(
    availabilityStates.temporarilyUnavailableOrFailed.apiPresence,
    "present",
  );
  assert.equal(
    availabilityStates.temporarilyUnavailableOrFailed.behavior,
    "throw",
  );
});

test("the schema freezes stable operational errors", () => {
  const contract = loadSchema()["x-mcujs-contract"];

  assert.deepEqual(sorted(Object.keys(contract.errors.operational)), [
    "EBUSY",
    "EIO",
    "ENXIO",
    "ERR_NOT_SUPPORTED",
    "ERR_RESOURCE_EXHAUSTED",
  ]);
  assert.deepEqual(contract.errors.programming.TypeError.for, [
    "missingArgument",
    "wrongType",
    "nonFiniteNumber",
  ]);
  assert.deepEqual(contract.errors.programming.RangeError.for, [
    "nonIntegralNumber",
    "outOfRange",
    "oversizedValue",
  ]);
  assert.equal(
    contract.errors.operational.ERR_NOT_SUPPORTED.name,
    "NotSupportedError",
  );
  assert.equal(contract.errors.operational.EBUSY.name, "ResourceBusyError");
  assert.equal(
    contract.errors.operational.ERR_RESOURCE_EXHAUSTED.name,
    "ResourceExhaustedError",
  );
  assert.equal(
    contract.errors.operational.ENXIO.state,
    "temporarilyUnavailableOrFailed",
  );
  assert.equal(
    contract.errors.operational.EIO.state,
    "temporarilyUnavailableOrFailed",
  );

  assert.deepEqual(contract.errors.argumentBoundary, {
    RangeError:
      "The supplied value violates an explicit type, range, length, route, enum, or capability-derived argument constraint.",
    ERR_NOT_SUPPORTED:
      "Every supplied value passes its explicit argument constraints, but the advertised operation cannot implement the requested valid combination or exact hardware representation.",
  });
  assert.deepEqual(contract.errors.argumentConstraintErrors, {
    allowedFromCapability: "RangeError",
    conditionalAllowedFromCapability: "RangeError",
    minimumFromCapability: "RangeError",
    maximumFromCapability: "RangeError",
    exclusiveMaximumFromConfiguration: "RangeError",
    minimumLength: "RangeError",
    maximumFromArgumentResource: "RangeError",
    routeFromCapability: "RangeError",
  });
  assert.deepEqual(contract.errors.operationalConditionKinds, {
    unsupportedConfiguration: { code: "ERR_NOT_SUPPORTED" },
    resourceBusy: { code: "EBUSY" },
    uninitializedUse: { code: "EBUSY" },
    resourceExhausted: { code: "ERR_RESOURCE_EXHAUSTED" },
    externalDeviceUnavailable: { code: "ENXIO" },
    nativeIoFailure: { code: "EIO" },
  });

  for (const [moduleName, moduleContract] of Object.entries(contract.modules)) {
    for (const [exportName, exported] of Object.entries(moduleContract.exports)) {
      for (const [signatureIndex, signature] of (exported.signatures ?? []).entries()) {
        const location = `${moduleName}.${exportName} signature ${signatureIndex}`;
        const declaredErrors = signature.operationalErrors ?? [];
        for (const code of declaredErrors) {
          assert.ok(contract.errors.operational[code], `${location} names unknown ${code}`);
        }
        for (const condition of signature.operationalErrorConditions ?? []) {
          assert.ok(declaredErrors.includes(condition.code), `${location} condition omits ${condition.code}`);
          assert.equal(
            contract.errors.operationalConditionKinds[condition.cause]?.code,
            condition.code,
            `${location} has an invalid ${condition.cause} to ${condition.code} mapping`,
          );
          if (condition.argument) {
            assert.ok(
              signature.arguments.some((argument) => argument.name === condition.argument.name),
              `${location} condition references unknown argument ${condition.argument.name}`,
            );
          }
        }
      }
    }
  }
});

test("every core peripheral signature has a complete operational-error mapping", () => {
  const contract = loadSchema()["x-mcujs-contract"];
  const expected = {
    "gpio.init#0": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED"],
    "gpio.set#0": ["EBUSY", "EIO"],
    "gpio.get#0": ["EBUSY", "EIO"],
    "gpio.toggle#0": ["EBUSY", "EIO"],
    "pwm.init#0": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED", "ERR_RESOURCE_EXHAUSTED"],
    "pwm.setDuty#0": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED"],
    "pwm.stop#0": ["EBUSY", "EIO"],
    "adc.readPin#0": ["EBUSY", "EIO"],
    "adc.readChannel#0": ["EBUSY", "EIO"],
    "adc.readVoltagePin#0": ["EBUSY", "EIO"],
    "adc.readVoltageChannel#0": ["EBUSY", "EIO"],
    "adc.readTempC#0": ["EBUSY", "EIO"],
    "i2c.init#0": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED", "ERR_RESOURCE_EXHAUSTED"],
    "i2c.init#1": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED", "ERR_RESOURCE_EXHAUSTED"],
    "i2c.write#0": ["EBUSY", "EIO", "ENXIO", "ERR_RESOURCE_EXHAUSTED"],
    "i2c.read#0": ["EBUSY", "EIO", "ENXIO", "ERR_RESOURCE_EXHAUSTED"],
    "spi.init#0": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED", "ERR_RESOURCE_EXHAUSTED"],
    "spi.init#1": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED", "ERR_RESOURCE_EXHAUSTED"],
    "spi.transfer#0": ["EBUSY", "EIO", "ERR_RESOURCE_EXHAUSTED"],
    "neopixel.init#0": ["EBUSY", "EIO", "ERR_NOT_SUPPORTED", "ERR_RESOURCE_EXHAUSTED"],
    "neopixel.setPixel#0": ["EBUSY", "EIO"],
    "neopixel.show#0": ["EBUSY", "EIO", "ERR_RESOURCE_EXHAUSTED"],
    "neopixel.clear#0": ["EBUSY", "EIO"],
  };

  for (const [location, errorCodes] of Object.entries(expected)) {
    const [qualifiedName, indexText] = location.split("#");
    const [moduleName, exportName] = qualifiedName.split(".");
    const signature = contract.modules[moduleName].exports[exportName].signatures[Number(indexText)];
    assert.deepEqual(sorted(signature.operationalErrors), errorCodes, `${location} error set drifted`);
    assert.deepEqual(
      sorted(new Set(signature.operationalErrorConditions.map(({ code }) => code))),
      errorCodes,
      `${location} has an error without a condition`,
    );
  }

  const valid = validatePortableApiContract(contract);
  assert.equal(valid.valid, true, JSON.stringify(valid.errors));

  assert.equal(contract.types.spiOptions.fields.mode.allowedFromCapability, "spi.modes");
  for (const signature of contract.modules.spi.exports.init.signatures) {
    const unsupported = signature.operationalErrorConditions.find(
      ({ code }) => code === "ERR_NOT_SUPPORTED",
    );
    assert.deepEqual(unsupported, {
      code: "ERR_NOT_SUPPORTED",
      cause: "unsupportedConfiguration",
      afterArgumentValidation: true,
      configuration: "inRangeFrequencyNotExactlyRepresentable",
    });
  }

  const contradictoryModeMapping = structuredClone(contract);
  contradictoryModeMapping.modules.spi.exports.init.signatures[0]
    .operationalErrorConditions[0].argumentConstraint = {
      path: "types.spiOptions.fields.mode",
      kind: "allowedFromCapability",
    };
  const contradictoryModeResult = validatePortableApiContract(contradictoryModeMapping);
  assert.equal(contradictoryModeResult.valid, false);
  assert.ok(
    contradictoryModeResult.errors.some(
      (error) => error.constraint === "operationalErrors.argumentConstraintCode",
    ),
    JSON.stringify(contradictoryModeResult.errors),
  );

  for (const invalidMapping of [undefined, "ERR_NOT_SUPPORTED"]) {
    const invalidConditionalMapping = structuredClone(contract);
    if (invalidMapping === undefined) {
      delete invalidConditionalMapping.errors.argumentConstraintErrors
        .conditionalAllowedFromCapability;
    } else {
      invalidConditionalMapping.errors.argumentConstraintErrors
        .conditionalAllowedFromCapability = invalidMapping;
    }
    const invalidConditionalResult = validatePortableApiContract(
      invalidConditionalMapping,
    );
    assert.equal(invalidConditionalResult.valid, false);
    assert.ok(
      invalidConditionalResult.errors.some(
        (error) =>
          error.location ===
            "errors.argumentConstraintErrors.conditionalAllowedFromCapability" &&
          error.constraint === "argumentBoundary.constraintMapping",
      ),
      JSON.stringify(invalidConditionalResult.errors),
    );
  }

  const prematureUnsupportedConfiguration = structuredClone(contract);
  delete prematureUnsupportedConfiguration.modules.spi.exports.init.signatures[0]
    .operationalErrorConditions[0].afterArgumentValidation;
  const prematureUnsupportedResult = validatePortableApiContract(
    prematureUnsupportedConfiguration,
  );
  assert.equal(prematureUnsupportedResult.valid, false);
  assert.ok(
    prematureUnsupportedResult.errors.some(
      (error) => error.constraint === "operationalErrors.afterArgumentValidation",
    ),
    JSON.stringify(prematureUnsupportedResult.errors),
  );

  const omitted = structuredClone(contract);
  delete omitted.modules.gpio.exports.init.signatures[0].operationalErrors;
  const missingMapping = validatePortableApiContract(omitted);
  assert.equal(missingMapping.valid, false);
  assert.ok(
    missingMapping.errors.some((error) => error.constraint === "operationalErrors.required"),
    JSON.stringify(missingMapping.errors),
  );

  const mismatched = structuredClone(contract);
  mismatched.modules.pwm.exports.init.signatures[0].operationalErrorConditions[0].code = "EIO";
  const invalidCause = validatePortableApiContract(mismatched);
  assert.equal(invalidCause.valid, false);
  assert.ok(
    invalidCause.errors.some((error) => error.constraint === "operationalErrors.causeCode"),
    JSON.stringify(invalidCause.errors),
  );

  const omittedCompatibilityError = structuredClone(contract);
  delete omittedCompatibilityError.modules.spi.exports.writeBufferDMA.signatures[0]
    .operationalErrors;
  const missingCompatibilityMapping = validatePortableApiContract(omittedCompatibilityError);
  assert.equal(missingCompatibilityMapping.valid, false);
  assert.ok(
    missingCompatibilityMapping.errors.some(
      (error) => error.constraint === "operationalErrors.required",
    ),
    JSON.stringify(missingCompatibilityMapping.errors),
  );
});

test("all contract modules, public names, types, and units are explicit", () => {
  const contract = loadSchema()["x-mcujs-contract"];
  const expectedExports = {
    board: [
      "apiVersion",
      "buttonPressed",
      "capabilities",
      "capability",
      "chip",
      "cpuFreq",
      "delay",
      "devices",
      "enterUf2",
      "flashSize",
      "freeMemory",
      "led",
      "millis",
      "name",
      "neopixel",
      "pins",
      "ramSize",
      "reset",
      "safeMode",
      "storageReady",
      "uniqueId",
      "version",
    ],
    "mcujs:module": ["builtinModules", "has"],
    gpio: ["INPUT", "INPUT_PULLDOWN", "INPUT_PULLUP", "OUTPUT", "get", "init", "set", "toggle"],
    pwm: ["init", "setDuty", "stop"],
    adc: [
      "readChannel",
      "readPin",
      "readTempC",
      "readVoltageChannel",
      "readVoltagePin",
    ],
    i2c: ["init", "read", "write"],
    spi: ["init", "transfer", "writeBufferDMA"],
    neopixel: ["clear", "init", "setPixel", "show"],
  };

  assert.deepEqual(sorted(Object.keys(contract.modules)), sorted(Object.keys(expectedExports)));
  for (const [moduleName, exportNames] of Object.entries(expectedExports)) {
    assert.deepEqual(
      sorted(Object.keys(contract.modules[moduleName].exports)),
      sorted(exportNames),
      `${moduleName} exports drifted`,
    );
  }

  for (const [location, typeName] of collectTypeReferences(contract)) {
    assert.ok(contract.types[typeName], `${location} references unknown type ${typeName}`);
  }

  for (const [typeName, typeContract] of Object.entries(contract.types)) {
    if (typeContract.javascriptType === "number") {
      assert.equal(typeContract.finite, true, `${typeName} must reject non-finite numbers`);
    }
  }

  for (const moduleContract of Object.values(contract.modules)) {
    for (const exported of Object.values(moduleContract.exports)) {
      for (const signature of exported.signatures ?? []) {
        for (const value of [...signature.arguments, signature.returns]) {
          if (value.unit) {
            assert.ok(contract.units[value.unit], `unknown unit ${value.unit}`);
          }
        }
      }
    }
  }

  assert.deepEqual(contract.units.ratio.range, [0, 1]);
  assert.equal(contract.units.frequency.symbol, "Hz");
  assert.equal(contract.units.duration.symbol, "ms");
  assert.equal(contract.units.voltage.symbol, "V");
  assert.equal(contract.units.transferSize.symbol, "bytes");
  assert.equal(contract.units.temperature.symbol, "°C");
});

test("operation parameters have complete machine-readable capability limits", () => {
  const contract = loadSchema()["x-mcujs-contract"];
  const modules = contract.modules;
  const types = contract.types;
  const argument = (moduleName, exportName, signatureIndex, argumentName) =>
    modules[moduleName].exports[exportName].signatures[signatureIndex].arguments.find(
      (entry) => entry.name === argumentName,
    );

  assert.deepEqual(contract.operationConstraints, {
    capabilityPathRoot: "manifest.capabilities",
    onboardDevicePathRoot: "manifest.board.devices",
    configurationPathRoot: "initialized module state",
    pathSyntax: "dotPathWithArrayProjection",
    fields: {
      defaultFromCapability: "omitted value comes from the referenced capability number",
      allowedFromCapability: "value must be a member of the referenced collection",
      conditionalAllowedFromCapability: "the referenced argument selects a narrower capability collection for this value",
      minimumFromCapability: "value must be greater than or equal to the referenced number",
      maximumFromCapability: "value or value length must be less than or equal to the referenced number",
      maximumFromArgumentResource: "value must be less than or equal to a runtime resource property resolved from the named argument",
      minimumLength: "value length must be greater than or equal to the fixed number",
      routeFromCapability: "the correlated argument tuple must equal one object in the referenced route collection",
      defaultRouteFromCapability: "omitted correlated route fields come from the referenced route object",
      maximumFromCapabilityBitWidth: "numeric result must be at most two raised to the referenced bit width minus one",
      maximumFromOnboardDevice: "value length must not exceed the referenced onboard-device number",
      exclusiveMaximumFromConfiguration: "value must be less than the referenced initialized configuration value",
      invalidResourceError: "programming error thrown when a runtime resource handle is unknown or stale",
    },
  });
  assert.deepEqual(types.gpioMode.capabilityValueMap, {
    OUTPUT: "output",
    INPUT: "input",
    INPUT_PULLUP: "inputPullup",
    INPUT_PULLDOWN: "inputPulldown",
  });
  assert.equal(argument("board", "delay", 0, "milliseconds").minimum, 0);

  const allowed = [
    [argument("gpio", "init", 0, "pin"), "gpio.pins"],
    [argument("gpio", "set", 0, "pin"), "gpio.outputPins"],
    [argument("gpio", "get", 0, "pin"), "gpio.pins"],
    [argument("gpio", "toggle", 0, "pin"), "gpio.outputPins"],
    [argument("pwm", "init", 0, "pin"), "pwm.pins"],
    [argument("pwm", "setDuty", 0, "pin"), "pwm.pins"],
    [argument("pwm", "stop", 0, "pin"), "pwm.pins"],
    [argument("adc", "readPin", 0, "pin"), "adc.pins"],
    [argument("adc", "readChannel", 0, "channel"), "adc.channels[].channel"],
    [argument("adc", "readVoltagePin", 0, "pin"), "adc.pins"],
    [argument("adc", "readVoltageChannel", 0, "channel"), "adc.channels[].channel"],
    [argument("i2c", "write", 0, "bus"), "i2c.buses"],
    [argument("i2c", "read", 0, "bus"), "i2c.buses"],
    [argument("spi", "transfer", 0, "bus"), "spi.buses"],
  ];
  for (const [parameter, capabilityPath] of allowed) {
    assert.equal(parameter.allowedFromCapability, capabilityPath);
  }

  assert.equal(argument("gpio", "init", 0, "mode").allowedFromCapability, "gpio.modes");
  assert.deepEqual(argument("gpio", "init", 0, "pin").conditionalAllowedFromCapability, {
    whenArgument: "mode",
    capabilityValue: "output",
    allowedFromCapability: "gpio.outputPins",
  });
  assert.deepEqual(argument("pwm", "init", 0, "frequency"), {
    name: "frequency",
    type: "integer",
    required: true,
    unit: "frequency",
    minimumFromCapability: "pwm.frequency.minHz",
    maximumFromCapability: "pwm.frequency.maxHz",
  });
  assert.equal(argument("pwm", "setDuty", 0, "duty").minimumFromCapability, "pwm.duty.min");
  assert.equal(argument("pwm", "setDuty", 0, "duty").maximumFromCapability, "pwm.duty.max");

  assert.equal(types.i2cOptions.fields.bus.allowedFromCapability, "i2c.buses");
  assert.equal(types.i2cOptions.defaultRouteFromCapability, "i2c.defaultRoute");
  assert.equal(types.i2cOptions.fields.frequency.minimumFromCapability, "i2c.frequency.minHz");
  assert.equal(types.i2cOptions.fields.frequency.maximumFromCapability, "i2c.frequency.maxHz");
  for (const role of ["sda", "scl"]) {
    assert.equal(types.i2cOptions.fields[role].defaultFromCapability, `i2c.defaultRoute.${role}`);
    assert.equal(types.i2cOptions.fields[role].routeFromCapability.path, "i2c.routes");
    assert.equal(types.i2cOptions.fields[role].routeFromCapability.field, role);
    assert.ok(types.i2cOptions.fields[role].routeFromCapability.correlatesWith.includes("bus"));
  }
  assert.equal(types.spiOptions.fields.bus.allowedFromCapability, "spi.buses");
  assert.equal(types.spiOptions.defaultRouteFromCapability, "spi.defaultRoute");
  assert.equal(types.spiOptions.fields.frequency.minimumFromCapability, "spi.frequency.minHz");
  assert.equal(types.spiOptions.fields.frequency.maximumFromCapability, "spi.frequency.maxHz");
  assert.equal(types.spiOptions.fields.mode.allowedFromCapability, "spi.modes");
  for (const role of ["sck", "mosi", "miso"]) {
    assert.equal(types.spiOptions.fields[role].defaultFromCapability, `spi.defaultRoute.${role}`);
    assert.equal(types.spiOptions.fields[role].routeFromCapability.path, "spi.routes");
    assert.equal(types.spiOptions.fields[role].routeFromCapability.field, role);
    assert.ok(types.spiOptions.fields[role].routeFromCapability.correlatesWith.includes("bus"));
  }
  assert.equal(types.neopixelOptions.fields.pin.allowedFromCapability, "neopixel.pins");
  assert.equal(types.neopixelOptions.fields.length.minimum, 1);
  assert.equal(types.neopixelOptions.fields.length.maximumFromCapability, "neopixel.maxLength");
  assert.equal(types.neopixelOptions.fields.order.allowedFromCapability, "neopixel.orders");

  for (const [moduleName, roles] of Object.entries({ i2c: ["sda", "scl"], spi: ["sck", "mosi", "miso"] })) {
    const positional = modules[moduleName].exports.init.signatures[1];
    assert.equal(positional.arguments[0].allowedFromCapability, `${moduleName}.buses`);
    for (const role of roles) {
      const routeArgument = positional.arguments.find((entry) => entry.name === role);
      assert.equal(routeArgument.routeFromCapability.path, `${moduleName}.routes`);
      assert.equal(routeArgument.routeFromCapability.field, role);
      assert.ok(routeArgument.routeFromCapability.correlatesWith.includes("bus"));
    }
    const frequency = positional.arguments.find((entry) => entry.name === "frequency");
    assert.equal(frequency.minimumFromCapability, `${moduleName}.frequency.minHz`);
    assert.equal(frequency.maximumFromCapability, `${moduleName}.frequency.maxHz`);
  }

  assert.equal(argument("i2c", "write", 0, "data").minimumLength, 1);
  assert.equal(argument("i2c", "write", 0, "data").maximumFromCapability, "i2c.maxTransferBytes");
  assert.equal(argument("i2c", "read", 0, "length").minimum, 1);
  assert.equal(argument("i2c", "read", 0, "length").maximumFromCapability, "i2c.maxTransferBytes");
  assert.equal(argument("spi", "transfer", 0, "data").minimumLength, 1);
  assert.equal(argument("spi", "transfer", 0, "data").maximumFromCapability, "spi.maxTransferBytes");
  assert.equal(argument("neopixel", "setPixel", 0, "index").minimum, 0);
  assert.equal(
    argument("neopixel", "setPixel", 0, "index").exclusiveMaximumFromConfiguration,
    "neopixel.length",
  );

  const manifest = validManifest();
  const references = collectOperationReferences(contract);
  assert.ok(references.length > 0);
  for (const [location, field, reference] of references) {
    if (field === "routeFromCapability") {
      const routes = resolveProjectedPath(manifest.capabilities, reference.path);
      assert.ok(routes.every(Array.isArray), `${location} must resolve to route arrays`);
      for (const routeSet of routes) {
        assert.ok(routeSet.length > 0, `${location} resolves to an empty route set`);
        assert.ok(routeSet.every((route) => reference.field in route), `${location} names an unknown route field`);
      }
      continue;
    }
    if (field === "conditionalAllowedFromCapability") {
      resolveProjectedPath(manifest.capabilities, reference.allowedFromCapability);
      continue;
    }
    if (field === "maximumFromCapabilityBitWidth") {
      const widths = resolveProjectedPath(manifest.capabilities, reference);
      assert.ok(
        widths.every((width) => Number.isInteger(width) && width > 0),
        `${location} must resolve to positive integer bit widths`,
      );
      continue;
    }
    if (field === "maximumFromArgumentResource") {
      assert.deepEqual(sorted(Object.keys(reference)), ["argument", "property", "relation"]);
      assert.equal(reference.relation, "lessThanOrEqual");
      continue;
    }
    if (field.endsWith("FromCapability")) {
      resolveProjectedPath(manifest.capabilities, reference);
      continue;
    }
    if (field === "maximumFromOnboardDevice") {
      assert.equal(typeof reference, "string", `${location} must contain a device-relative path`);
      continue;
    }
    if (field === "exclusiveMaximumFromConfiguration") {
      assert.match(reference, /^[a-z][A-Za-z0-9]*(?:\.[a-z][A-Za-z0-9]*)+$/, location);
      continue;
    }
    if (field === "invalidResourceError") {
      assert.ok(contract.errors.programming[reference], `${location} names unknown ${reference}`);
    }
  }
});

test("every ADC numeric result declares its machine-readable result bounds", () => {
  const adcExports = loadSchema()["x-mcujs-contract"].modules.adc.exports;
  const numericResults = Object.fromEntries(
    Object.entries(adcExports).map(([name, operation]) => [
      name,
      operation.signatures[0].returns,
    ]),
  );

  assert.deepEqual(sorted(Object.keys(numericResults)), [
    "readChannel",
    "readPin",
    "readTempC",
    "readVoltageChannel",
    "readVoltagePin",
  ]);
  for (const name of ["readPin", "readChannel"]) {
    assert.equal(numericResults[name].minimum, 0, name);
    assert.equal(
      numericResults[name].maximumFromCapabilityBitWidth,
      "adc.resolutionBits",
      name,
    );
  }
  for (const name of ["readVoltagePin", "readVoltageChannel"]) {
    assert.equal(
      numericResults[name].minimumFromCapability,
      "adc.voltage.minVolts",
      name,
    );
    assert.equal(
      numericResults[name].maximumFromCapability,
      "adc.voltage.maxVolts",
      name,
    );
  }
  assert.equal(numericResults.readTempC.unit, "temperature");
});

test("an advertised ADC capability has executable raw pin and channel operations", () => {
  const schema = loadSchema();
  assert.equal(schema.$defs.adcCapability.properties.pins.minItems, 1);
  assert.equal(schema.$defs.adcCapability.properties.channels.minItems, 1);

  const manifest = validManifest();
  manifest.capabilities.adc.pins = [];
  manifest.capabilities.adc.channels = [];
  manifest.capabilities.adc.voltage = { supported: false };
  manifest.capabilities.adc.temperature = { supported: false };
  manifest.capabilities.adc.vsys = false;
  const result = validatePortableApiManifest(manifest);
  assert.equal(result.valid, false, "accepted an ADC module with no valid raw operation");
  assert.ok(
    result.errors.some((error) => error.keyword === "minItems"),
    JSON.stringify(result.errors),
  );
});

test("every export uses a complete and unambiguous availability shape", () => {
  const contract = loadSchema()["x-mcujs-contract"];
  const manifest = validManifest();
  const gateKeys = ["capability", "capabilityField", "capabilityValue", "onboardDevice"];

  for (const [moduleName, moduleContract] of Object.entries(contract.modules)) {
    for (const [exportName, exported] of Object.entries(moduleContract.exports)) {
      const location = `${moduleName}.${exportName}`;
      const expectedGateKeys = {
        required: [],
        capability: ["capability"],
        capabilityField: ["capabilityField"],
        capabilityCollectionValue: ["capabilityField", "capabilityValue"],
        onboardDevice: ["onboardDevice"],
      }[exported.availability];
      assert.ok(expectedGateKeys, `${location} has unknown availability ${exported.availability}`);
      assert.deepEqual(
        sorted(gateKeys.filter((key) => key in exported)),
        sorted(expectedGateKeys),
        `${location} has an incomplete or irrelevant availability gate`,
      );
      switch (exported.availability) {
        case "required":
          break;
        case "capability":
          assert.equal(typeof exported.capability, "string", location);
          assert.ok(exported.capability in manifest.capabilities, location);
          break;
        case "capabilityField": {
          assert.equal(typeof exported.capabilityField, "string", location);
          const values = resolveProjectedPath(
            manifest.capabilities,
            exported.capabilityField,
          );
          assert.ok(values.every((value) => typeof value === "boolean"), location);
          break;
        }
        case "capabilityCollectionValue": {
          assert.equal(typeof exported.capabilityField, "string", location);
          assert.equal(typeof exported.capabilityValue, "string", location);
          const capabilityName = moduleContract.capability;
          assert.equal(typeof capabilityName, "string", location);
          const collection = manifest.capabilities[capabilityName]?.[exported.capabilityField];
          if (collection !== undefined) {
            assert.ok(Array.isArray(collection), location);
          }
          break;
        }
        case "onboardDevice":
          assert.equal(typeof exported.onboardDevice, "string", location);
          break;
      }
    }
  }

  const adc = contract.modules.adc.exports;
  assert.equal(adc.readVoltagePin.availability, "capabilityField");
  assert.equal(adc.readVoltagePin.capabilityField, "adc.voltage.supported");
  assert.equal(adc.readVoltageChannel.availability, "capabilityField");
  assert.equal(adc.readVoltageChannel.capabilityField, "adc.voltage.supported");
  assert.equal(adc.readTempC.availability, "capabilityField");
  assert.equal(adc.readTempC.capabilityField, "adc.temperature.supported");
});

test("SPI 0.2 exposes only its executable implicit 8-bit MSB format", () => {
  const contract = loadSchema()["x-mcujs-contract"];
  assert.deepEqual(contract.modules.spi.exports.init.implicitConfiguration, {
    bitsPerWord: {
      value: 8,
      allowedFromCapability: "spi.bitsPerWord",
      selectable: false,
    },
    bitOrder: {
      value: "msb",
      allowedFromCapability: "spi.bitOrders",
      selectable: false,
    },
  });

  for (const [name, constraint, mutate] of [
    ["unreachable 16-bit words", "spi.fixedBitsPerWord", (manifest) => {
      manifest.capabilities.spi.bitsPerWord = [16];
    }],
    ["unreachable LSB-first order", "spi.fixedBitOrder", (manifest) => {
      manifest.capabilities.spi.bitOrders = ["lsb"];
    }],
    ["extra unreachable word format", "spi.fixedBitsPerWord", (manifest) => {
      manifest.capabilities.spi.bitsPerWord = [8, 16];
    }],
  ]) {
    assert.equal(typeof contract.manifestValidation.constraints[constraint], "string");
    const manifest = validManifest();
    mutate(manifest);
    const result = validatePortableApiManifest(manifest);
    assert.equal(result.valid, false, name);
    assert.ok(
      result.errors.some((error) => error.params?.constraint === constraint),
      `${name}: ${JSON.stringify(result.errors)}`,
    );
  }
});

test("capability descriptors are static and onboard inventory stays separate", () => {
  const schema = loadSchema();
  const contract = schema["x-mcujs-contract"];
  const capabilityMap = schema.$defs.capabilityMap;
  const expectedCapabilities = ["adc", "boot", "fs", "gpio", "i2c", "image", "neopixel", "pwm", "spi", "usb"];
  const dynamicNames = /^(available|busy|connected|current|mounted|occupied|owner|ready|state|status)$/i;

  assert.equal(capabilityMap.additionalProperties, false);
  assert.deepEqual(sorted(Object.keys(capabilityMap.properties)), expectedCapabilities);
  assert.deepEqual(sorted(Object.keys(contract.capabilities)), expectedCapabilities);

  for (const [name, metadata] of Object.entries(contract.capabilities)) {
    assert.equal(metadata.dynamic, false, `${name} must be static`);
    assert.equal(capabilityMap.properties[name].$ref, metadata.schema);
    const definitionName = metadata.schema.replace("#/$defs/", "");
    const definition = schema.$defs[definitionName];
    assert.equal(definition.additionalProperties, false, `${name} descriptor must be closed`);
    for (const fieldName of Object.keys(definition.properties)) {
      assert.doesNotMatch(fieldName, dynamicNames, `${name}.${fieldName} is dynamic state`);
    }
  }

  assert.equal(contract.capabilities.neopixel.scope, "externalDriver");
  assert.equal(contract.onboardDevices.scope, "physicalOnboardInventory");
  assert.ok(schema.$defs.boardIdentity.properties.devices);
  assert.ok(schema.$defs.boardIdentity.properties.exposedPins);
  assert.ok(schema.$defs.onboardDevices.properties.neopixel);
  assert.equal(schema.$defs.neopixelCapability.properties.onboard, undefined);
  assert.deepEqual(contract.dynamicState.storageReady, {
    owner: "board",
    availability: "capability",
    capability: "fs",
    returnType: "boolean",
  });
});

test("live nonportable compatibility extensions have explicit capability gates", () => {
  const contract = loadSchema()["x-mcujs-contract"];
  const safeMode = contract.modules.board.exports.safeMode;
  assert.equal(safeMode.portable, false);
  assert.equal(safeMode.compatibilityOnly, true);
  assert.equal(safeMode.availability, "capabilityField");
  assert.equal(safeMode.capabilityField, "boot.safeMode");
  assert.deepEqual(
    safeMode.signatures.map((signature) => ({
      arguments: signature.arguments.map((argument) => argument.type),
      returns: signature.returns.types,
    })),
    [
      { arguments: [], returns: ["boolean"] },
      { arguments: ["boolean"], returns: ["undefined"] },
    ],
  );
  const safeModeSetter = safeMode.signatures[1];
  assert.deepEqual(sorted(safeModeSetter.operationalErrors), ["EBUSY", "EIO"]);
  assert.deepEqual(safeModeSetter.operationalErrorConditions, [
    {
      code: "EBUSY",
      cause: "resourceBusy",
      argument: { name: "enabled", equals: false },
      dynamicState: "bootQualificationActive",
    },
    {
      code: "EIO",
      cause: "nativeIoFailure",
      nativeFailure: "persistentBootStateWrite",
    },
  ]);

  const writeBufferDMA = contract.modules.spi.exports.writeBufferDMA;
  assert.equal(writeBufferDMA.portable, false);
  assert.equal(writeBufferDMA.compatibilityOnly, true);
  assert.equal(writeBufferDMA.availability, "capabilityCollectionValue");
  assert.equal(writeBufferDMA.capabilityField, "compatibilityExtensions");
  assert.equal(writeBufferDMA.capabilityValue, "writeBufferDMA");
  const dmaSignature = writeBufferDMA.signatures[0];
  assert.deepEqual(
    dmaSignature.arguments.map(({ name, type, required }) => ({ name, type, required })),
    [
      { name: "bus", type: "integer", required: true },
      { name: "bufferHandle", type: "graphicsBufferHandle", required: true },
      { name: "byteLength", type: "integer", required: true },
    ],
  );
  assert.deepEqual(dmaSignature.returns.types, ["undefined"]);
  assert.deepEqual(dmaSignature.operationalErrors, ["EBUSY", "ERR_RESOURCE_EXHAUSTED"]);
  assert.deepEqual(dmaSignature.operationalErrorConditions, [
    {
      code: "EBUSY",
      cause: "uninitializedUse",
    },
    {
      code: "ERR_RESOURCE_EXHAUSTED",
      cause: "resourceExhausted",
      dynamicState: "dmaChannelUnavailable",
    },
  ]);
  const [, bufferHandle, byteLength] = dmaSignature.arguments;
  assert.equal(bufferHandle.invalidResourceError, "RangeError");
  assert.deepEqual(contract.types[bufferHandle.type].resourceProperties.byteLength, {
    type: "integer",
    unit: "transferSize",
    minimum: 0,
  });
  assert.equal(byteLength.minimum, 0);
  assert.deepEqual(byteLength.maximumFromArgumentResource, {
    argument: "bufferHandle",
    property: "byteLength",
    relation: "lessThanOrEqual",
  });
  assert.equal(byteLength.oversized, "RangeError");

  const resource = { byteLength: 128 };
  const withinHandleLimit = (value) => {
    const reference = byteLength.maximumFromArgumentResource;
    assert.equal(reference.relation, "lessThanOrEqual");
    return value <= resource[reference.property];
  };
  assert.equal(withinHandleLimit(resource.byteLength), true, "the handle-derived maximum is valid");
  assert.equal(withinHandleLimit(resource.byteLength + 1), false, "maximum + 1 is rejected");

  const manifest = validManifest();
  manifest.capabilities.i2c.defaultRoute = structuredClone(manifest.capabilities.i2c.routes[0]);
  manifest.capabilities.spi.defaultRoute = structuredClone(manifest.capabilities.spi.routes[0]);
  manifest.capabilities.spi.dma = true;
  manifest.capabilities.spi.compatibilityExtensions = ["writeBufferDMA"];
  assert.equal(validatePortableApiManifest(manifest).valid, true);

  manifest.capabilities.spi.dma = false;
  const invalid = validatePortableApiManifest(manifest);
  assert.equal(invalid.valid, false);
  assert.ok(
    invalid.errors.some(
      (error) => error.params?.constraint === "spi.writeBufferDMARequiresDma",
    ),
    JSON.stringify(invalid.errors),
  );
});

test("pre-1.0 compatibility aliases match the live public surface and have typed gates", () => {
  const contract = loadSchema()["x-mcujs-contract"];
  const aliases = Object.fromEntries(
    contract.compatibility.aliases.map((alias) => [alias.name, alias]),
  );
  assert.equal(
    Object.keys(aliases).length,
    contract.compatibility.aliases.length,
    "compatibility alias names must be unique",
  );

  assert.equal(contract.versioning.apiVersion, "0.2");
  assert.equal(contract.versioning.pre1BreakingChanges, "minorOnly");
  assert.equal(contract.versioning.patchChanges, "nonBreakingOnly");
  assert.equal(contract.compatibility.removalNotBefore, "1.0");
  assert.deepEqual(contract.compatibility.removalPolicy, {
    availableThrough: "0.x",
    removalNotBefore: "1.0",
    requiresMigrationNotes: true,
  });
  assert.deepEqual(
    sorted(Object.keys(aliases)),
    sorted(collectLiveCompatibilityAliasNames(contract)),
    "the schema alias inventory drifted from native public bindings",
  );
  assert.equal(aliases["global.board"].target, "require('board')");
  assert.equal(aliases["global.GPIO"].target, "require('gpio')");
  assert.equal(aliases["global.PWM"].target, "require('pwm')");
  assert.equal(aliases["global.I2C"].target, "require('i2c')");
  assert.equal(aliases["global.SPI"].target, "require('spi')");
  assert.equal(aliases["global.adc"].target, "require('adc')");
  assert.equal(aliases["global.neopixel"].target, "require('neopixel')");
  assert.equal(aliases["i2c.init(positional)"].target, "i2c.init(options)");
  assert.equal(aliases["spi.init(positional)"].target, "spi.init(options)");
  assert.deepEqual(aliases["require('node:module')"], {
    name: "require('node:module')",
    target: "require('mcujs:module')",
    kind: "moduleSpecifier",
    javascriptType: "object",
    availability: "required",
    portable: false,
    removalNotBefore: "1.0",
    removalRequiresMigrationNotes: true,
  });
  assert.deepEqual(
    {
      target: aliases["board.ledPin"].target,
      kind: aliases["board.ledPin"].kind,
      javascriptType: aliases["board.ledPin"].javascriptType,
      availability: aliases["board.ledPin"].availability,
      onboardDevice: aliases["board.ledPin"].onboardDevice,
      onboardDeviceField: aliases["board.ledPin"].onboardDeviceField,
      onboardDeviceType: aliases["board.ledPin"].onboardDeviceType,
    },
    {
      target: "board.devices.led.pin",
      kind: "property",
      javascriptType: "number",
      availability: "onboardDeviceField",
      onboardDevice: "led",
      onboardDeviceField: "pin",
      onboardDeviceType: "gpio",
    },
  );
  assert.equal(aliases["board.neopixelPin"].availability, "onboardDeviceField");
  assert.equal(aliases["board.neopixelPin"].onboardDevice, "neopixel");
  assert.equal(aliases["board.neopixelPin"].onboardDeviceField, "pin");
  assert.equal(aliases["board.neopixelLength"].availability, "onboardDeviceField");
  assert.equal(aliases["board.neopixelLength"].onboardDeviceField, "length");
  assert.equal(aliases["adc.TEMP"].availability, "capabilityFieldEquals");
  assert.equal(aliases["adc.TEMP"].capabilityField, "adc.temperature.rawChannel");
  assert.equal(aliases["adc.TEMP"].capabilityValue, true);
  assert.equal(aliases["adc.VSYS"].availability, "capabilityFieldEquals");
  assert.equal(aliases["adc.VSYS"].capabilityField, "adc.vsys");
  assert.equal(aliases["adc.VSYS"].capabilityValue, true);

  const globalCapabilityAliases = {
    "global.GPIO": "gpio",
    "global.PWM": "pwm",
    "global.adc": "adc",
    "global.I2C": "i2c",
    "global.SPI": "spi",
    "global.neopixel": "neopixel",
  };
  for (const [name, capability] of Object.entries(globalCapabilityAliases)) {
    assert.equal(aliases[name].kind, "globalBinding");
    assert.equal(aliases[name].javascriptType, "object");
    assert.equal(aliases[name].availability, "capability");
    assert.equal(aliases[name].capability, capability);
  }
  const availabilityGateKeys = [
    "capability",
    "capabilityField",
    "capabilityValue",
    "onboardDevice",
    "onboardDeviceField",
    "onboardDeviceType",
  ];
  for (const alias of Object.values(aliases)) {
    assert.ok(["globalBinding", "moduleSpecifier", "overload", "property"].includes(alias.kind));
    assert.ok(["function", "number", "object"].includes(alias.javascriptType));
    assert.ok(
      ["required", "capability", "capabilityFieldEquals", "onboardDeviceField"].includes(
        alias.availability,
      ),
    );
    const expectedGateKeys = {
      required: [],
      capability: ["capability"],
      capabilityFieldEquals: ["capabilityField", "capabilityValue"],
      onboardDeviceField: [
        "onboardDevice",
        "onboardDeviceField",
        ...(alias.onboardDeviceType === undefined ? [] : ["onboardDeviceType"]),
      ],
    }[alias.availability];
    assert.deepEqual(
      sorted(availabilityGateKeys.filter((key) => key in alias)),
      sorted(expectedGateKeys),
      `${alias.name} has an incomplete or irrelevant availability gate`,
    );
    assert.equal(alias.portable, false);
    assert.equal(alias.removalNotBefore, "1.0");
    assert.equal(alias.removalRequiresMigrationNotes, true);
  }
});

test("onboard shortcut overloads preserve existing calls but reject oversized colors", () => {
  const boardExports = loadSchema()["x-mcujs-contract"].modules.board.exports;
  const signature = (entry) => entry.signatures.map((item) => ({
    arguments: item.arguments.map((argument) => argument.type),
    returns: item.returns.types,
  }));

  assert.deepEqual(signature(boardExports.led), [
    { arguments: [], returns: ["boolean"] },
    { arguments: ["boolean"], returns: ["undefined"] },
  ]);
  assert.deepEqual(signature(boardExports.neopixel), [
    { arguments: ["neopixelColorArray"], returns: ["undefined"] },
    { arguments: ["neopixelColorObject"], returns: ["undefined"] },
    { arguments: ["neopixelColorArrayList"], returns: ["undefined"] },
    { arguments: ["neopixelColorObjectList"], returns: ["undefined"] },
  ]);

  const types = loadSchema()["x-mcujs-contract"].types;
  assert.equal(types.neopixelColorArray.maxItems, 3);
  assert.equal(types.neopixelColorArray.oversized, "RangeError");
  assert.equal(
    types.neopixelColorArrayList.maximumFromOnboardDevice,
    "length",
  );
  assert.equal(types.neopixelColorArrayList.oversized, "RangeError");
  assert.equal(
    types.neopixelColorObjectList.maximumFromOnboardDevice,
    "length",
  );
  assert.equal(types.neopixelColorObjectList.oversized, "RangeError");
});

test("Docusaurus documents the frozen contract and 0.1 migration", () => {
  const apiDesign = readFileSync(apiDesignUrl, "utf8");
  const migration = readFileSync(migrationUrl, "utf8");
  const portableApiDesign = readFileSync(portableApiDesignUrl, "utf8");

  for (const phrase of [
    "/schemas/mcujs-portable-api-0.2.schema.json",
    "unsupported configuration",
    "temporarily unavailable",
    "ERR_NOT_SUPPORTED",
    "EBUSY",
    "board.devices",
    "board.exposedPins",
    "runtime-exposed",
    "type: 'managed'",
    "gpioCapability.pins.indexOf(led.pin) !== -1",
    "allowedFromCapability",
    "exclusiveMaximumFromConfiguration",
    "defaultRouteFromCapability",
    "maximumFromCapabilityBitWidth",
    "maximumFromArgumentResource",
    "capabilityField",
    "8-bit",
    "MSB-first",
    "boot qualification",
    "static capability",
    "semantic validator",
    "modules.has('spi')",
    "writeBufferDMA",
    "safeMode",
  ]) {
    assert.ok(apiDesign.includes(phrase), `API design docs are missing ${phrase}`);
  }

  for (const phrase of [
    "0.1",
    "0.2",
    "0..1",
    "GPIO",
    "i2c.init(options)",
    "spi.init(options)",
    "adc.TEMP",
    "adc.VSYS",
    "board.name",
    "global.adc",
    "global.neopixel",
    "`board.led()` returns",
    "oversized NeoPixel",
    "writeBufferDMA",
    "safeMode",
    "maximumFromArgumentResource",
    "boot qualification",
    "ERR_RESOURCE_EXHAUSTED",
  ]) {
    assert.ok(migration.includes(phrase), `migration docs are missing ${phrase}`);
  }

  assert.match(
    migration,
    /Unlisted routes or modes and frequencies outside the advertised range throw `RangeError`/,
  );
  assert.match(
    migration,
    /`ERR_NOT_SUPPORTED` starts only after every explicit argument constraint passes/,
  );
  assert.doesNotMatch(migration, /Choose a listed route, mode, or frequency/);

  const spiExports = loadSchema()["x-mcujs-contract"].modules.spi.exports;
  assert.ok(spiExports.init);
  assert.equal(spiExports.open, undefined);
  assert.match(
    portableApiDesign,
    /SPI exists but requested mode is absent from advertised `spi\.modes` \| `spi\.init` throws `RangeError`/,
  );
  assert.match(
    portableApiDesign,
    /SPI exists and frequency is in range but not exactly representable \| `spi\.init` throws `NotSupportedError` \/ `ERR_NOT_SUPPORTED`/,
  );
  assert.doesNotMatch(portableApiDesign, /requested mode[^\n]*`ERR_NOT_SUPPORTED`/);
  assert.match(
    apiDesign,
    /SPI mode absent from advertised `spi\.modes` therefore throws `RangeError`/,
  );
  assert.doesNotMatch(portableApiDesign, /spi\.open/);
});
