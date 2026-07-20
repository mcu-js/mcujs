#!/usr/bin/env node

const { readFileSync } = require("node:fs");
const { createRequire } = require("node:module");
const { join, resolve } = require("node:path");

const rootDir = resolve(__dirname, "..");
const docsRequire = createRequire(join(rootDir, "docs/package.json"));
const Ajv2020 = docsRequire("ajv/dist/2020").default;
const schemaPath = join(
  rootDir,
  "docs/static/schemas/mcujs-portable-api-0.2.schema.json",
);
const schema = JSON.parse(readFileSync(schemaPath, "utf8"));
const contract = schema["x-mcujs-contract"];
const ajv = new Ajv2020({ allErrors: true, strict: true });
ajv.addKeyword({ keyword: "x-mcujs-contract" });
const validateSchema = ajv.compile(schema);

function semanticError(errors, instancePath, constraint, message) {
  errors.push({
    instancePath,
    schemaPath: `#/x-mcujs-contract/manifestValidation/constraints/${constraint}`,
    keyword: "x-mcujs-semantic",
    params: { constraint },
    message,
  });
}

function validateSubset(errors, values, allowed, instancePath, constraint) {
  const allowedValues = new Set(allowed);
  values.forEach((value, index) => {
    if (!allowedValues.has(value)) {
      semanticError(
        errors,
        `${instancePath}/${index}`,
        constraint,
        `must also appear in ${instancePath.replace(/\/[^/]+$/, "/pins")}`,
      );
    }
  });
}

function validateFrequencyRange(errors, frequency, instancePath, constraint) {
  if (frequency.minHz > frequency.maxHz) {
    semanticError(
      errors,
      instancePath,
      constraint,
      "minHz must be less than or equal to maxHz",
    );
  }
}

function validateExposedPins(errors, pins, exposedPins, instancePath, constraint) {
  const exposed = new Set(exposedPins);
  pins.forEach((pin, index) => {
    if (!exposed.has(pin)) {
      semanticError(
        errors,
        `${instancePath}/${index}`,
        constraint,
        "must appear in board.exposedPins",
      );
    }
  });
}

function validateRoutePins(errors, name, capability, exposedPins) {
  const roles = name === "i2c" ? ["sda", "scl"] : ["sck", "mosi", "miso"];
  const exposed = new Set(exposedPins);
  capability.routes.forEach((route, index) => {
    const path = `/capabilities/${name}/routes/${index}`;
    for (const role of roles) {
      if (!exposed.has(route[role])) {
        semanticError(
          errors,
          `${path}/${role}`,
          `${name}.routePinsExposed`,
          "must appear in board.exposedPins",
        );
      }
    }

    const distinct = name === "i2c"
      ? route.sda !== route.scl
      : route.sck !== route.mosi &&
        route.sck !== route.miso &&
        (!capability.fullDuplex || route.mosi !== route.miso);
    if (!distinct) {
      semanticError(
        errors,
        path,
        `${name}.distinctRoutePins`,
        name === "i2c"
          ? "SDA and SCL must use distinct pins"
          : "SCK and data roles required for this duplex mode must use distinct pins",
      );
    }
  });
}

function validateBusCapability(errors, name, capability) {
  const path = `/capabilities/${name}`;
  const buses = new Set(capability.buses);
  if (!buses.has(capability.defaultBus)) {
    semanticError(
      errors,
      `${path}/defaultBus`,
      `${name}.defaultBus`,
      "must appear in buses",
    );
  }

  const routedBuses = new Set();
  capability.routes.forEach((route, index) => {
    if (!buses.has(route.bus)) {
      semanticError(
        errors,
        `${path}/routes/${index}/bus`,
        `${name}.routeBus`,
        "must appear in buses",
      );
    }
    routedBuses.add(route.bus);
  });
  capability.buses.forEach((bus, index) => {
    if (!routedBuses.has(bus)) {
      semanticError(
        errors,
        `${path}/buses/${index}`,
        `${name}.busRoutes`,
        "must have at least one matching route",
      );
    }
  });

  const routeFields = name === "i2c"
    ? ["bus", "sda", "scl"]
    : ["bus", "sck", "mosi", "miso"];
  const defaultRouteListed = capability.routes.some((route) =>
    routeFields.every((field) => route[field] === capability.defaultRoute[field])
  );
  if (
    capability.defaultRoute.bus !== capability.defaultBus ||
    !defaultRouteListed
  ) {
    semanticError(
      errors,
      `${path}/defaultRoute`,
      `${name}.defaultRoute`,
      "must be a listed route whose bus equals defaultBus",
    );
  }

  validateFrequencyRange(
    errors,
    capability.frequency,
    `${path}/frequency`,
    `${name}.frequencyRange`,
  );
}

function validateSemanticManifest(manifest) {
  const errors = [];
  const { capabilities } = manifest;
  const exposedPins = manifest.board.exposedPins;

  for (const [alias, pin] of Object.entries(manifest.board.pins)) {
    if (!exposedPins.includes(pin)) {
      semanticError(
        errors,
        `/board/pins/${alias}`,
        "board.pinAliasesExposed",
        "must appear in board.exposedPins",
      );
    }
  }

  const onboardLed = manifest.board.devices.led;
  if (onboardLed?.type === "gpio" && !exposedPins.includes(onboardLed.pin)) {
    semanticError(
      errors,
      "/board/devices/led/pin",
      "onboard.ledPinExposed",
      "must appear in board.exposedPins",
    );
  }

  if (capabilities.gpio) {
    validateExposedPins(
      errors,
      capabilities.gpio.pins,
      exposedPins,
      "/capabilities/gpio/pins",
      "gpio.pinsExposed",
    );
    validateSubset(
      errors,
      capabilities.gpio.outputPins,
      capabilities.gpio.pins,
      "/capabilities/gpio/outputPins",
      "gpio.outputPinsSubset",
    );
    const hasOutputPins = capabilities.gpio.outputPins.length > 0;
    const hasOutputMode = capabilities.gpio.modes.includes("output");
    if (hasOutputPins !== hasOutputMode) {
      semanticError(
        errors,
        "/capabilities/gpio/modes",
        "gpio.outputMode",
        "output mode and a non-empty outputPins collection must be present together",
      );
    }
  }

  if (capabilities.pwm) {
    validateExposedPins(
      errors,
      capabilities.pwm.pins,
      exposedPins,
      "/capabilities/pwm/pins",
      "pwm.pinsExposed",
    );
    validateFrequencyRange(
      errors,
      capabilities.pwm.frequency,
      "/capabilities/pwm/frequency",
      "pwm.frequencyRange",
    );
    if (capabilities.pwm.maxOutputs > capabilities.pwm.pins.length) {
      semanticError(
        errors,
        "/capabilities/pwm/maxOutputs",
        "pwm.maxOutputs",
        "must not exceed the number of PWM-capable pins",
      );
    }
    if (capabilities.pwm.timerCount > capabilities.pwm.maxOutputs) {
      semanticError(
        errors,
        "/capabilities/pwm/timerCount",
        "pwm.timerCount",
        "must not exceed the number of independently addressable PWM outputs",
      );
    }
  }

  if (capabilities.adc) {
    const adc = capabilities.adc;
    validateExposedPins(
      errors,
      adc.pins,
      exposedPins,
      "/capabilities/adc/pins",
      "adc.pinsExposed",
    );
    const adcPins = new Set(adc.pins);
    const channels = new Set();
    const channelPins = new Set();
    adc.channels.forEach((channel, index) => {
      const path = `/capabilities/adc/channels/${index}`;
      if (!adcPins.has(channel.pin)) {
        semanticError(
          errors,
          `${path}/pin`,
          "adc.channelPins",
          "must appear in adc.pins",
        );
      }
      if (channels.has(channel.channel)) {
        semanticError(
          errors,
          `${path}/channel`,
          "adc.uniqueChannels",
          "must be unique within adc.channels",
        );
      }
      channels.add(channel.channel);
      if (channelPins.has(channel.pin)) {
        semanticError(
          errors,
          `${path}/pin`,
          "adc.uniquePins",
          "must be unique within adc.channels",
        );
      }
      channelPins.add(channel.pin);
      channel.aliases.forEach((alias, aliasIndex) => {
        if (manifest.board.pins[alias] !== channel.pin) {
          semanticError(
            errors,
            `${path}/aliases/${aliasIndex}`,
            "adc.channelAliases",
            "must resolve to the channel pin in board.pins",
          );
        }
      });
    });
    if (
      adc.voltage.supported &&
      adc.voltage.minVolts >= adc.voltage.maxVolts
    ) {
      semanticError(
        errors,
        "/capabilities/adc/voltage",
        "adc.voltageRange",
        "minVolts must be less than maxVolts",
      );
    }
    if (adc.voltage.supported && (adc.pins.length === 0 || adc.channels.length === 0)) {
      semanticError(
        errors,
        "/capabilities/adc/voltage",
        "adc.voltageRoutes",
        "supported voltage conversion requires at least one pin and channel",
      );
    }
  }

  if (capabilities.i2c) {
    validateBusCapability(errors, "i2c", capabilities.i2c);
    validateRoutePins(errors, "i2c", capabilities.i2c, exposedPins);
  }
  if (capabilities.spi) {
    validateBusCapability(errors, "spi", capabilities.spi);
    validateRoutePins(errors, "spi", capabilities.spi, exposedPins);
    const defaultMode = contract.types.spiOptions.fields.mode.default;
    if (!capabilities.spi.modes.includes(defaultMode)) {
      semanticError(
        errors,
        "/capabilities/spi/modes",
        "spi.defaultMode",
        `must include the operation-contract default mode ${defaultMode}`,
      );
    }
    if (
      capabilities.spi.bitsPerWord.length !== 1 ||
      capabilities.spi.bitsPerWord[0] !== 8
    ) {
      semanticError(
        errors,
        "/capabilities/spi/bitsPerWord",
        "spi.fixedBitsPerWord",
        "the 0.2 byte-oriented SPI contract supports exactly [8]",
      );
    }
    if (
      capabilities.spi.bitOrders.length !== 1 ||
      capabilities.spi.bitOrders[0] !== "msb"
    ) {
      semanticError(
        errors,
        "/capabilities/spi/bitOrders",
        "spi.fixedBitOrder",
        "the 0.2 byte-oriented SPI contract supports exactly ['msb']",
      );
    }
    if (
      capabilities.spi.compatibilityExtensions?.includes("writeBufferDMA") &&
      !capabilities.spi.dma
    ) {
      semanticError(
        errors,
        "/capabilities/spi/compatibilityExtensions",
        "spi.writeBufferDMARequiresDma",
        "writeBufferDMA requires dma to be true",
      );
    }
  }

  if (capabilities.neopixel) {
    validateExposedPins(
      errors,
      capabilities.neopixel.pins,
      exposedPins,
      "/capabilities/neopixel/pins",
      "neopixel.pinsExposed",
    );
    const defaultOrder = contract.types.neopixelOptions.fields.order.default;
    if (!capabilities.neopixel.orders.includes(defaultOrder)) {
      semanticError(
        errors,
        "/capabilities/neopixel/orders",
        "neopixel.defaultOrder",
        `must include the operation-contract default order ${defaultOrder}`,
      );
    }
  }

  const onboardNeopixel = manifest.board.devices.neopixel;
  if (onboardNeopixel) {
    const driver = capabilities.neopixel;
    if (!driver) {
      semanticError(
        errors,
        "/board/devices/neopixel",
        "onboard.neopixelCapability",
        "requires the neopixel driver capability",
      );
    } else {
      if (!driver.pins.includes(onboardNeopixel.pin)) {
        semanticError(
          errors,
          "/board/devices/neopixel/pin",
          "onboard.neopixelPin",
          "must appear in capabilities.neopixel.pins",
        );
      }
      if (onboardNeopixel.length > driver.maxLength) {
        semanticError(
          errors,
          "/board/devices/neopixel/length",
          "onboard.neopixelLength",
          "must not exceed capabilities.neopixel.maxLength",
        );
      }
      if (!driver.orders.includes(onboardNeopixel.order)) {
        semanticError(
          errors,
          "/board/devices/neopixel/order",
          "onboard.neopixelOrder",
          "must appear in capabilities.neopixel.orders",
        );
      }
    }
  }

  return errors;
}

function contractError(errors, location, constraint, message) {
  errors.push({ location, constraint, message });
}

function collectDeclaredArgumentConstraints(candidate) {
  const declaredFields = new Set(
    Object.keys(candidate.operationConstraints?.fields ?? {}),
  );
  const constraints = new Set();

  function collect(value) {
    if (Array.isArray(value)) {
      value.forEach(collect);
      return;
    }
    if (value === null || typeof value !== "object") {
      return;
    }
    for (const [field, entry] of Object.entries(value)) {
      if (declaredFields.has(field) && !field.endsWith("Error")) {
        constraints.add(field);
      }
      collect(entry);
    }
  }

  for (const moduleContract of Object.values(candidate.modules ?? {})) {
    for (const exported of Object.values(moduleContract.exports ?? {})) {
      for (const signature of exported.signatures ?? []) {
        for (const argument of signature.arguments ?? []) {
          collect(argument);
        }
      }
    }
  }

  return constraints;
}

function validatePortableApiContract(candidate) {
  const errors = [];
  const errorContract = candidate.errors ?? {};
  const operational = errorContract.operational ?? {};
  const conditionKinds = errorContract.operationalConditionKinds ?? {};
  const argumentConstraintErrors = errorContract.argumentConstraintErrors ?? {};
  const declaredOperationConstraints = new Set(
    Object.keys(candidate.operationConstraints?.fields ?? {}),
  );
  const requiredModules = errorContract.operationalMappingsRequiredForModules ?? [];
  const requiredModuleNames = new Set(requiredModules);

  for (const constraint of collectDeclaredArgumentConstraints(candidate)) {
    if (argumentConstraintErrors[constraint] !== "RangeError") {
      contractError(
        errors,
        `errors.argumentConstraintErrors.${constraint}`,
        "argumentBoundary.constraintMapping",
        "capability-derived and explicit argument constraints must map to RangeError",
      );
    }
  }
  for (const constraint of Object.keys(argumentConstraintErrors)) {
    if (!declaredOperationConstraints.has(constraint)) {
      contractError(
        errors,
        `errors.argumentConstraintErrors.${constraint}`,
        "argumentBoundary.unknownConstraintMapping",
        "argument error mappings must name a declared operation constraint",
      );
    }
  }

  for (const moduleName of requiredModules) {
    if (!candidate.modules?.[moduleName]) {
      contractError(
        errors,
        `modules.${moduleName}`,
        "operationalErrors.requiredModule",
        "operational-error completeness names an unknown module",
      );
    }
  }

  for (const [moduleName, moduleContract] of Object.entries(candidate.modules ?? {})) {
    for (const [exportName, exported] of Object.entries(moduleContract.exports)) {
      if (exported.kind !== "method") {
        continue;
      }
      for (const [signatureIndex, signature] of exported.signatures.entries()) {
        const mappingRequired =
          (requiredModuleNames.has(moduleName) && !exported.compatibilityOnly) ||
          signature.operationalErrorMappingRequired === true;
        if (!mappingRequired) {
          continue;
        }
        const location = `modules.${moduleName}.exports.${exportName}.signatures.${signatureIndex}`;
        if (!Array.isArray(signature.operationalErrors) || signature.operationalErrors.length === 0) {
          contractError(
            errors,
            location,
            "operationalErrors.required",
            "must declare at least one operational error",
          );
          continue;
        }
        if (
          !Array.isArray(signature.operationalErrorConditions) ||
          signature.operationalErrorConditions.length === 0
        ) {
          contractError(
            errors,
            location,
            "operationalErrors.conditionsRequired",
            "must map every operational error to a machine-readable condition",
          );
          continue;
        }

        const conditionedCodes = new Set();
        for (const condition of signature.operationalErrorConditions) {
          conditionedCodes.add(condition.code);
          if (!signature.operationalErrors.includes(condition.code)) {
            contractError(
              errors,
              location,
              "operationalErrors.conditionDeclared",
              `${condition.code} is conditioned but not declared`,
            );
          }
          if (!operational[condition.code]) {
            contractError(
              errors,
              location,
              "operationalErrors.knownCode",
              `${condition.code} is not a stable operational code`,
            );
          }
          if (conditionKinds[condition.cause]?.code !== condition.code) {
            contractError(
              errors,
              location,
              "operationalErrors.causeCode",
              `${condition.cause} does not map to ${condition.code}`,
            );
          }
          if (
            condition.cause === "unsupportedConfiguration" &&
            condition.afterArgumentValidation !== true
          ) {
            contractError(
              errors,
              location,
              "operationalErrors.afterArgumentValidation",
              "unsupported configuration begins only after all argument constraints pass",
            );
          }
          if (condition.argumentConstraint) {
            const { path, kind } = condition.argumentConstraint;
            const constrainedArgument = path
              ?.split(".")
              .reduce((value, segment) => value?.[segment], candidate);
            if (!constrainedArgument || !(kind in constrainedArgument)) {
              contractError(
                errors,
                location,
                "operationalErrors.argumentConstraintReference",
                `${path}.${kind} is not a contract argument constraint`,
              );
            } else if (argumentConstraintErrors[kind] !== condition.code) {
              contractError(
                errors,
                location,
                "operationalErrors.argumentConstraintCode",
                `${path}.${kind} maps to ${argumentConstraintErrors[kind] ?? "no programming error"}, not ${condition.code}`,
              );
            }
          }
          if (
            condition.argument &&
            !signature.arguments.some((argument) => argument.name === condition.argument.name)
          ) {
            contractError(
              errors,
              location,
              "operationalErrors.argument",
              `${condition.argument.name} is not an argument of this signature`,
            );
          }
        }

        for (const code of signature.operationalErrors) {
          if (!operational[code]) {
            contractError(
              errors,
              location,
              "operationalErrors.knownCode",
              `${code} is not a stable operational code`,
            );
          }
          if (!conditionedCodes.has(code)) {
            contractError(
              errors,
              location,
              "operationalErrors.conditionMissing",
              `${code} has no machine-readable condition`,
            );
          }
        }

        if (
          exported.requiresInitialization &&
          !signature.operationalErrorConditions.some(({ cause }) => cause === "uninitializedUse")
        ) {
          contractError(
            errors,
            location,
            "operationalErrors.uninitializedUse",
            "an initialization-dependent operation must map uninitialized use",
          );
        }
      }
    }
  }

  return { valid: errors.length === 0, errors };
}

function validatePortableApiManifest(manifest) {
  const contractValidation = validatePortableApiContract(contract);
  if (!contractValidation.valid) {
    return contractValidation;
  }
  if (!validateSchema(manifest)) {
    return {
      valid: false,
      errors: structuredClone(validateSchema.errors ?? []),
    };
  }

  const errors = validateSemanticManifest(manifest);
  return { valid: errors.length === 0, errors };
}

function main(args) {
  if (args.length === 0) {
    console.error("Usage: node scripts/validate-portable-api-manifest.js MANIFEST.json [...]");
    return 2;
  }

  let failed = false;
  for (const manifestPath of args) {
    try {
      const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
      const result = validatePortableApiManifest(manifest);
      if (result.valid) {
        console.log(`${manifestPath}: valid`);
      } else {
        failed = true;
        console.error(`${manifestPath}: invalid`);
        for (const error of result.errors) {
          console.error(`  ${error.instancePath || "/"} ${error.message}`);
        }
      }
    } catch (error) {
      failed = true;
      console.error(`${manifestPath}: ${error.message}`);
    }
  }
  return failed ? 1 : 0;
}

if (require.main === module) {
  process.exitCode = main(process.argv.slice(2));
}

module.exports = {
  validatePortableApiContract,
  validatePortableApiManifest,
};
