#!/usr/bin/env node
"use strict";

const { mkdirSync, readFileSync, writeFileSync } = require("node:fs");
const { dirname, join } = require("node:path");
const { boardDescriptors, manifestFor, shippingBoardIds } = require("../runtime/board-registry.js");

const root = join(__dirname, "..");
const generatedHeaderPath = "host/generated/runtime_registry_data.h";
const manifestDirectory = join(root, "runtime", "manifests");
const boardDocsPath = "docs/docs/hardware-boards.md";
const boardDocsStart = "<!-- BEGIN GENERATED BOARD REGISTRY -->";
const boardDocsEnd = "<!-- END GENERATED BOARD REGISTRY -->";

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
  lines.push(`#define MCUJS_REGISTRY_STORAGE_READY ${descriptor.capabilities.fs ? 1 : 0}`);
  const usbClasses = descriptor.capabilities.usb?.classes ?? [];
  lines.push(`#define MCUJS_USB_CDC ${usbClasses.includes("cdc") ? 1 : 0}`);
  lines.push(`#define MCUJS_USB_MSC ${usbClasses.includes("msc") ? 1 : 0}`);
  lines.push(`#define MCUJS_USB_KEYBOARD_HID ${usbClasses.includes("keyboardHid") ? 1 : 0}`);
  lines.push(`#define MCUJS_USB_MOUSE_HID ${usbClasses.includes("mouseHid") ? 1 : 0}`);
  lines.push(`#define MCUJS_RUNTIME_GPIO_PIN_MASK ${pinMask(descriptor.capabilities.gpio?.pins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_GPIO_OUTPUT_PIN_MASK ${pinMask(descriptor.capabilities.gpio?.outputPins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_PWM_PIN_MASK ${pinMask(descriptor.capabilities.pwm?.pins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_ADC_PIN_MASK ${pinMask(descriptor.capabilities.adc?.pins ?? [])}`);
  const adcChannels = (descriptor.capabilities.adc?.channels ?? []).map(({ channel }) => channel);
  if (descriptor.capabilities.adc?.vsys === true) adcChannels.push(3);
  lines.push(`#define MCUJS_RUNTIME_ADC_CHANNEL_MASK ${pinMask(adcChannels)}`);
  lines.push(`#define MCUJS_REGISTRY_ADC_TEMP_RAW_CHANNEL ${descriptor.capabilities.adc?.temperature?.rawChannel === true ? 1 : 0}`);
  const neopixel = descriptor.capabilities.neopixel;
  lines.push(`#define MCUJS_RUNTIME_NEOPIXEL_PIN_MASK ${pinMask(neopixel?.pins ?? [])}`);
  lines.push(`#define MCUJS_RUNTIME_NEOPIXEL_MAX_LENGTH ${neopixel?.maxLength ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_NEOPIXEL_ORDER_RGB ${neopixel?.orders.includes("RGB") ? 1 : 0}`);
  lines.push(`#define MCUJS_RUNTIME_NEOPIXEL_ORDER_GRB ${neopixel?.orders.includes("GRB") ? 1 : 0}`);
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
  const spi = descriptor.capabilities.spi;
  const spiModeMask = (spi?.modes ?? []).reduce(
    (mask, mode) => mask | (1 << mode), 0,
  );
  lines.push(`#define MCUJS_RUNTIME_SPI_DEFAULT_BUS ${spi?.defaultBus ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_SPI_DEFAULT_SCK ${spi?.defaultRoute.sck ?? -1}`);
  lines.push(`#define MCUJS_RUNTIME_SPI_DEFAULT_MOSI ${spi?.defaultRoute.mosi ?? -1}`);
  lines.push(`#define MCUJS_RUNTIME_SPI_DEFAULT_MISO ${spi?.defaultRoute.miso ?? -1}`);
  lines.push(`#define MCUJS_RUNTIME_SPI_MAX_TRANSFER_BYTES ${spi?.maxTransferBytes ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_SPI_MODE_MASK ${spiModeMask}u`);
  if (spiRoutes.length === 0) {
    lines.push("#define MCUJS_RUNTIME_SPI_ROUTES(X)");
  } else {
    lines.push("#define MCUJS_RUNTIME_SPI_ROUTES(X) " + String.fromCharCode(92));
    spiRoutes.forEach(({ bus, sck, mosi, miso }, index) => {
      const suffix = index === spiRoutes.length - 1 ? "" : ` ${String.fromCharCode(92)}`;
      lines.push(`    X(${bus}, ${sck}, ${mosi}, ${miso})${suffix}`);
    });
  }
  lines.push(`#define MCUJS_RUNTIME_SPI_MIN_HZ ${spi?.frequency.minHz ?? 0}`);
  lines.push(`#define MCUJS_RUNTIME_SPI_MAX_HZ ${spi?.frequency.maxHz ?? 0}`);
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

function checkmark(enabled) {
  return enabled ? "✓" : "—";
}

function onboardSummary(descriptor) {
  const devices = [];
  const led = descriptor.board.devices.led;
  if (led?.type === "managed") devices.push("LED (managed)");
  if (led?.type === "gpio") {
    devices.push(`LED (GPIO ${led.pin}, ${led.activeLow ? "active-low" : "active-high"})`);
  }
  const neopixel = descriptor.board.devices.neopixel;
  if (neopixel) {
    devices.push(`NeoPixel (GPIO ${neopixel.pin}, ${neopixel.length} × ${neopixel.order})`);
  }
  return devices.length > 0 ? devices.join(" + ") : "—";
}

function shortcutSummary(descriptor) {
  const shortcuts = [];
  if (descriptor.board.devices.led) shortcuts.push("`board.led()`");
  if (descriptor.board.devices.neopixel) shortcuts.push("`board.neopixel()`");
  return shortcuts.length > 0 ? shortcuts.join(" + ") : "—";
}

function semanticAliasSummary(descriptor) {
  const aliases = Object.entries(descriptor.board.pins)
    .filter(([name]) => !/^D\d+$/.test(name))
    .map(([name, pin]) => `\`${name}=${pin}\``);
  return aliases.length > 0 ? aliases.join(", ") : "—";
}

function generatedBoardDocsSection() {
  const supportedRows = shippingBoardIds.map((boardId) => {
    const descriptor = boardDescriptors[boardId];
    const { label, flash, notes } = descriptor.presentation;
    return `| \`${boardId}\` | ${label} | ${descriptor.board.chip} | ${flash} | ${notes} |`;
  });
  const featureRows = shippingBoardIds.map((boardId) => {
    const descriptor = boardDescriptors[boardId];
    const { features } = descriptor;
    const usbClasses = descriptor.capabilities.usb?.classes ?? [];
    const image = features.image ? (features.dvi ? "✓ + DVI" : "✓") : "—";
    return `| \`${boardId}\` | ${checkmark(features.gpio)} | ${checkmark(features.pwm)} | ${checkmark(features.i2c)} | ${checkmark(features.spi)} | ${checkmark(features.adc)} | ${checkmark(features.neopixel)} | ${image} | ${checkmark(usbClasses.includes("cdc"))} | ${checkmark(usbClasses.includes("msc"))} | ${checkmark(usbClasses.includes("keyboardHid"))} | ${checkmark(usbClasses.includes("mouseHid"))} | ${onboardSummary(descriptor)} |`;
  });
  const discoveryRows = shippingBoardIds.map((boardId) => {
    const descriptor = boardDescriptors[boardId];
    return `| \`${boardId}\` | ${semanticAliasSummary(descriptor)} | ${onboardSummary(descriptor)} | ${shortcutSummary(descriptor)} |`;
  });

  return `${boardDocsStart}
<!-- Generated by scripts/generate-runtime-registry.js. Do not edit this section. -->

## Supported boards

| Board ID | Board | Chip | Flash | Notes |
| --- | --- | --- | --- | --- |
${supportedRows.join("\n")}

## Authoritative 0.2 runtime feature matrix

The runtime descriptor in \`runtime/board-registry.js\` is authoritative for
firmware-enabled modules and release capability manifests. A check means the
corresponding firmware capability is enabled in that image; for module columns,
the module is registered. It does not claim an external device is attached.
\`NeoPixel\` describes the external driver, while the final column lists only
physically onboard devices.

| Target | GPIO | PWM | I2C | SPI | ADC | NeoPixel | Image/graphics | CDC | MSC | Keyboard HID | Mouse HID | Onboard devices |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
${featureRows.join("\n")}

## Runtime board discovery

The semantic aliases below omit numeric \`D*\` aliases for readability; the
complete immutable map is available through \`board.pins\` and the adjacent
release capability manifest. Optional onboard shortcuts are absent unless their
corresponding physical device is listed.

| Target | Semantic aliases | Onboard devices | Optional shortcuts |
| --- | --- | --- | --- |
${discoveryRows.join("\n")}
${boardDocsEnd}`;
}

function updateBoardDocs(content) {
  const start = content.indexOf(boardDocsStart);
  const end = content.indexOf(boardDocsEnd);
  if (start < 0 || end < start) throw new Error(`${boardDocsPath} is missing generated board registry markers`);
  return `${content.slice(0, start)}${generatedBoardDocsSection()}${content.slice(end + boardDocsEnd.length)}`;
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
  const docs = join(root, boardDocsPath);
  if (!readFileSync(docs, "utf8").includes(generatedBoardDocsSection())) failures.push(boardDocsPath);
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
  const docs = join(root, boardDocsPath);
  writeFileSync(docs, updateBoardDocs(readFileSync(docs, "utf8")));
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
  boardDocsPath,
  checkGeneratedFiles,
  generatedBoardDocsSection,
  generateRuntimeRegistryHeader,
  generatedHeaderPath,
  generatedManifestText,
  writeGeneratedFiles,
};
