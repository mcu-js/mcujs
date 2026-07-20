#!/usr/bin/env node
"use strict";

const { mkdirSync, readFileSync, writeFileSync } = require("node:fs");
const { dirname, join } = require("node:path");
const { boardDescriptors, manifestFor, shippingBoardIds } = require("../runtime/board-registry.js");

const root = join(__dirname, "..");
const generatedHeaderPath = "host/generated/runtime_registry_data.h";
const manifestDirectory = join(root, "runtime", "manifests");

const boardMacros = Object.freeze({
  pico: "MCUJS_BOARD_PICO",
  pico2: "MCUJS_BOARD_PICO2",
  pico2_w: "MCUJS_BOARD_PICO2_W",
  waveshare_rp2040_zero: "MCUJS_BOARD_WAVESHARE_RP2040_ZERO",
  waveshare_rp2040_pizero: "MCUJS_BOARD_WAVESHARE_RP2040_PIZERO",
  "waveshare_rp2040_touch_lcd_1.28": "MCUJS_BOARD_WAVESHARE_RP2040_TOUCH_LCD_1_28",
  "waveshare_rp2350_lcd_1.47_a": "MCUJS_BOARD_WAVESHARE_RP2350_LCD_1_47_A",
  "waveshare_rp2350_touch_lcd_1.69": "MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_1_69",
  adafruit_feather_rp2040: "MCUJS_BOARD_ADAFRUIT_FEATHER_RP2040",
  seeed_xiao_esp32s3: "MCUJS_BOARD_SEEED_XIAO_ESP32S3",
});

const featureMacros = Object.freeze({
  moduleLoader: "MCUJS_FEATURE_MODULE_LOADER",
  console: "MCUJS_FEATURE_CONSOLE",
  timers: "MCUJS_FEATURE_TIMERS",
  board: "MCUJS_FEATURE_BOARD",
  gpio: "MCUJS_FEATURE_GPIO",
  pwm: "MCUJS_FEATURE_PWM",
  i2c: "MCUJS_FEATURE_I2C",
  spi: "MCUJS_FEATURE_SPI",
  adc: "MCUJS_FEATURE_ADC",
  neopixel: "MCUJS_FEATURE_NEOPIXEL",
  process: "MCUJS_FEATURE_PROCESS",
  require: "MCUJS_FEATURE_REQUIRE",
  fs: "MCUJS_FEATURE_FS",
  image: "MCUJS_FEATURE_IMAGE",
  keyboard: "MCUJS_FEATURE_KEYBOARD",
  mouse: "MCUJS_FEATURE_MOUSE",
  graphics: "MCUJS_FEATURE_GRAPHICS",
  screen: "MCUJS_FEATURE_SCREEN",
});

function cString(value) {
  return JSON.stringify(value);
}

function pinMask(pins) {
  const mask = pins.reduce((value, pin) => value | (1n << BigInt(pin)), 0n);
  return `0x${mask.toString(16)}ULL`;
}

function branchFor(boardId, first) {
  const descriptor = boardDescriptors[boardId];
  const manifest = manifestFor(boardId);

  const lines = [`#${first ? "if" : "elif"} defined(${boardMacros[boardId]})`];
  lines.push(`#define MCUJS_RUNTIME_BOARD_ID ${cString(boardId)}`);
  for (const [feature, macro] of Object.entries(featureMacros)) {
    lines.push(`#define ${macro} ${descriptor.features[feature] ? 1 : 0}`);
  }
  lines.push(`#define MCUJS_HAS_DVI ${descriptor.features.dvi ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_ONBOARD_LED ${descriptor.features.onboardLed ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_ONBOARD_NEOPIXEL ${descriptor.features.onboardNeopixel ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_LED_PIN ${descriptor.board.devices.led?.type === "gpio" ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_ADC_VSYS ${descriptor.capabilities.adc?.vsys === true ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_ADC_VOLTAGE ${descriptor.capabilities.adc?.voltage?.supported === true ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_ADC_TEMPERATURE ${descriptor.capabilities.adc?.temperature?.supported === true ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_SAFE_MODE ${descriptor.features.safeMode ? 1 : 0}`);
  lines.push(`#define MCUJS_REGISTRY_STORAGE_READY ${descriptor.features.storageReady ? 1 : 0}`);
  lines.push(`#define MCUJS_RUNTIME_GPIO_PIN_MASK ${pinMask(descriptor.capabilities.gpio?.pins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_GPIO_OUTPUT_PIN_MASK ${pinMask(descriptor.capabilities.gpio?.outputPins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_PWM_PIN_MASK ${pinMask(descriptor.capabilities.pwm?.pins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_ADC_PIN_MASK ${pinMask(descriptor.capabilities.adc?.pins ?? [])}`);
  const adcChannels = (descriptor.capabilities.adc?.channels ?? []).map(({ channel }) => channel);
  if (descriptor.capabilities.adc?.vsys === true) adcChannels.push(3);
  lines.push(`#define MCUJS_RUNTIME_ADC_CHANNEL_MASK ${pinMask(adcChannels)}`);
  lines.push(`#define MCUJS_REGISTRY_ADC_TEMP_RAW_CHANNEL ${descriptor.capabilities.adc?.temperature?.rawChannel === true ? 1 : 0}`);
  lines.push(`#define MCUJS_RUNTIME_NEOPIXEL_PIN_MASK ${pinMask(descriptor.capabilities.neopixel?.pins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_PWM_MIN_HZ ${descriptor.capabilities.pwm?.frequency.minHz ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_PWM_MAX_HZ ${descriptor.capabilities.pwm?.frequency.maxHz ?? 0}`);
  const i2c = descriptor.capabilities.i2c;
  lines.push(`#define MCUJS_RUNTIME_I2C_DEFAULT_BUS ${i2c?.defaultBus ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_I2C_DEFAULT_SDA ${i2c?.defaultRoute.sda ?? -1}`);
  lines.push(`#define MCUJS_RUNTIME_I2C_DEFAULT_SCL ${i2c?.defaultRoute.scl ?? -1}`);
  lines.push(`#define MCUJS_RUNTIME_I2C_MIN_HZ ${i2c?.frequency.minHz ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_I2C_MAX_HZ ${i2c?.frequency.maxHz ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_I2C_MAX_TRANSFER_BYTES ${i2c?.maxTransferBytes ?? 0}`);
  const i2cRoutes = descriptor.capabilities.i2c?.routes ?? [];
  if (i2cRoutes.length === 0) {
    lines.push("#define MCUJS_RUNTIME_I2C_ROUTES(X)");
  } else {
    lines.push("#define MCUJS_RUNTIME_I2C_ROUTES(X) " + String.fromCharCode(92));
    i2cRoutes.forEach(({ bus, sda, scl }, index) => {
      const suffix = index === i2cRoutes.length - 1 ? "" : ` ${String.fromCharCode(92)}`;
      lines.push(`    X(${bus}, ${sda}, ${scl})${suffix}`);
    });
  }
  const spiRoutes = descriptor.capabilities.spi?.routes ?? [];
  if (spiRoutes.length === 0) {
    lines.push("#define MCUJS_RUNTIME_SPI_ROUTES(X)");
  } else {
    lines.push("#define MCUJS_RUNTIME_SPI_ROUTES(X) " + String.fromCharCode(92));
    spiRoutes.forEach(({ bus, sck, mosi, miso }, index) => {
      const suffix = index === spiRoutes.length - 1 ? "" : ` ${String.fromCharCode(92)}`;
      lines.push(`    X(${bus}, ${sck}, ${mosi}, ${miso})${suffix}`);
    });
  }
  lines.push(`#define MCUJS_RUNTIME_SPI_MIN_HZ ${descriptor.capabilities.spi?.frequency.minHz ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_SPI_MAX_HZ ${descriptor.capabilities.spi?.frequency.maxHz ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_API_VERSION ${cString(manifest.apiVersion)}`);
  lines.push(`#define MCUJS_RUNTIME_BOARD_JSON ${cString(JSON.stringify(manifest.board))}`);
  lines.push(`#define MCUJS_RUNTIME_MANIFEST_JSON ${cString(JSON.stringify(manifest))}`);
  lines.push("#define MCUJS_RUNTIME_BUILTIN_MODULES(X) " + String.fromCharCode(92));
  descriptor.modules.forEach((name, index) => {
    const suffix = index === descriptor.modules.length - 1 ? "" : ` ${String.fromCharCode(92)}`;
    lines.push(`    X(${cString(name)})${suffix}`);
  });
  lines.push("#define MCUJS_RUNTIME_CAPABILITIES(X) " + String.fromCharCode(92));
  const capabilities = Object.entries(descriptor.capabilities);
  capabilities.forEach(([name, value], index) => {
    const suffix = index === capabilities.length - 1 ? "" : ` ${String.fromCharCode(92)}`;
    lines.push(`    X(${cString(name)}, ${cString(JSON.stringify(value))})${suffix}`);
  });
  return lines.join("\n");
}

function generateRuntimeRegistryHeader() {
  const branches = shippingBoardIds.map((boardId, index) => branchFor(boardId, index === 0));
  return `/* Generated by scripts/generate-runtime-registry.js. Do not edit. */
#ifndef MCUJS_RUNTIME_REGISTRY_DATA_H
#define MCUJS_RUNTIME_REGISTRY_DATA_H

${branches.join("\n")}
#else
#error "No explicit MCU.js board feature map selected"
#endif

#endif /* MCUJS_RUNTIME_REGISTRY_DATA_H */
`;
}

function generatedManifestText(boardId) {
  return `${JSON.stringify(manifestFor(boardId), null, 2)}\n`;
}

function checkGeneratedFiles() {
  const failures = [];
  const header = join(root, generatedHeaderPath);
  if (readFileSync(header, "utf8") !== generateRuntimeRegistryHeader()) failures.push(generatedHeaderPath);
  for (const boardId of shippingBoardIds) {
    const path = join(manifestDirectory, `${boardId}.json`);
    if (readFileSync(path, "utf8") !== generatedManifestText(boardId)) {
      failures.push(`runtime/manifests/${boardId}.json`);
    }
  }
  if (failures.length > 0) throw new Error(`stale generated registry files: ${failures.join(", ")}`);
}

function writeGeneratedFiles() {
  const header = join(root, generatedHeaderPath);
  mkdirSync(dirname(header), { recursive: true });
  mkdirSync(manifestDirectory, { recursive: true });
  writeFileSync(header, generateRuntimeRegistryHeader());
  for (const boardId of shippingBoardIds) {
    writeFileSync(join(manifestDirectory, `${boardId}.json`), generatedManifestText(boardId));
  }
}

if (require.main === module) {
  try {
    if (process.argv.includes("--check")) checkGeneratedFiles();
    else writeGeneratedFiles();
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}

module.exports = {
  checkGeneratedFiles,
  generateRuntimeRegistryHeader,
  generatedHeaderPath,
  generatedManifestText,
  writeGeneratedFiles,
};
