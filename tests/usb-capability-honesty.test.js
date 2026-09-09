const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const { join } = require("node:path");
const test = require("node:test");

const root = join(__dirname, "..");
const {
  boardDescriptors,
  shippingBoardIds,
} = require("../runtime/board-registry.js");
const {
  generateRuntimeRegistryHeader,
  generatedBoardDocsSection,
} = require("../scripts/generate-runtime-registry.js");

const rpBoardIds = shippingBoardIds.filter((boardId) => boardId !== "seeed_xiao_esp32s3");
const rpUsbClasses = ["cdc", "msc", "keyboardHid", "mouseHid"];
const espUsbClasses = ["cdc", "msc"];

function source(path) {
  return readFileSync(join(root, path), "utf8");
}

test("every shipping image advertises its exact enabled USB classes", () => {
  assert.equal(rpBoardIds.length, 10);
  for (const boardId of rpBoardIds) {
    const expected = boardId === "waveshare_rp2350_touch_lcd_2.8" ? ["cdc", "msc"] : rpUsbClasses;
    assert.deepEqual(boardDescriptors[boardId].capabilities.usb.classes, expected, boardId);
  }
  assert.deepEqual(
    boardDescriptors.seeed_xiao_esp32s3.capabilities.usb.classes,
    espUsbClasses,
  );
});

test("USB class descriptors agree with require-able keyboard and mouse modules", () => {
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const classes = descriptor.capabilities.usb.classes;
    assert.equal(classes.includes("cdc"), descriptor.features.console, `${boardId}.cdc`);
    assert.equal(
      classes.includes("msc"),
      descriptor.capabilities.fs?.hostTransfer === true,
      `${boardId}.msc`,
    );
    assert.equal(classes.includes("keyboardHid"), descriptor.features.keyboard, `${boardId}.keyboard feature`);
    assert.equal(classes.includes("keyboardHid"), descriptor.modules.includes("keyboard"), `${boardId}.keyboard module`);
    assert.equal(classes.includes("mouseHid"), descriptor.features.mouse, `${boardId}.mouse feature`);
    assert.equal(classes.includes("mouseHid"), descriptor.modules.includes("mouse"), `${boardId}.mouse module`);
  }
});

test("generated native registry exports exact per-image USB class switches", () => {
  const header = generateRuntimeRegistryHeader();
  for (const macro of [
    "MCUJS_USB_CDC",
    "MCUJS_USB_MSC",
    "MCUJS_USB_KEYBOARD_HID",
    "MCUJS_USB_MOUSE_HID",
  ]) {
    assert.match(header, new RegExp(`#define ${macro} [01]`), macro);
  }

  const espBranch = header.slice(header.indexOf("#elif defined(MCUJS_BOARD_SEEED_XIAO_ESP32S3)"));
  assert.match(espBranch, /#define MCUJS_USB_CDC 1/);
  assert.match(espBranch, /#define MCUJS_USB_MSC 1/);
  assert.match(espBranch, /#define MCUJS_USB_KEYBOARD_HID 0/);
  assert.match(espBranch, /#define MCUJS_USB_MOUSE_HID 0/);
});

test("RP TinyUSB and descriptors derive enabled classes and HID reports from registry switches", () => {
  const config = source("platform/rp2/usb/tusb_config.h");
  assert.match(config, /#include\s+"runtime_features\.h"/);
  assert.match(config, /#define\s+CFG_TUD_CDC\s+MCUJS_USB_CDC/);
  assert.match(config, /#define\s+CFG_TUD_MSC\s+MCUJS_USB_MSC/);
  assert.match(config, /#define\s+CFG_TUD_HID\s+\(MCUJS_USB_KEYBOARD_HID\s*\|\|\s*MCUJS_USB_MOUSE_HID\)/);

  const descriptors = source("platform/rp2/usb/usb_descriptors.c");
  assert.match(descriptors, /#if MCUJS_USB_KEYBOARD_HID[\s\S]*TUD_HID_REPORT_DESC_KEYBOARD/);
  assert.match(descriptors, /#if MCUJS_USB_MOUSE_HID[\s\S]*TUD_HID_REPORT_DESC_MOUSE/);
  assert.match(descriptors, /CFG_TUD_CDC \? TUD_CDC_DESC_LEN : 0/);
  assert.match(descriptors, /#if CFG_TUD_CDC[\s\S]*TUD_CDC_DESCRIPTOR/);
});

test("unavailable HID bindings are omitted rather than linked as throwing stubs", () => {
  for (const name of ["keyboard", "mouse"]) {
    const binding = source(`platform/rp2/bindings/${name}.c`);
    assert.doesNotMatch(binding, /Stub when HID is disabled/);
    assert.doesNotMatch(binding, /HID not enabled/);
  }

  const espSources = source("platform/esp32/main/CMakeLists.txt");
  assert.doesNotMatch(espSources, /bindings\/(keyboard|mouse)\.c/);

  const requireSource = source("host/bindings/require.c");
  assert.match(requireSource, /#if MCUJS_FEATURE_KEYBOARD[\s\S]*\{"keyboard", js_create_keyboard_module\}/);
  assert.match(requireSource, /#if MCUJS_FEATURE_MOUSE[\s\S]*\{"mouse", js_create_mouse_module\}/);
});

test("generated Docusaurus matrix distinguishes each configured USB class", () => {
  const docs = generatedBoardDocsSection();
  assert.ok(docs.includes("| Target | GPIO | PWM | I2C | SPI | ADC | NeoPixel | Image (max input) | Graphics | Screen/display | DVI | CDC | MSC | Keyboard HID | Mouse HID | Onboard devices |"));
  assert.match(docs, /A check means the\s+corresponding firmware capability is enabled in that image;/);
  assert.doesNotMatch(docs, /A check means the\s+module is registered/);
});
