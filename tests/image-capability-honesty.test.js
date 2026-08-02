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
  generatedBoardDocsSection,
  generateRuntimeRegistryHeader,
} = require("../scripts/generate-runtime-registry.js");

const rp2040Boards = new Set([
  "pico",
  "waveshare_rp2040_zero",
  "waveshare_rp2040_pizero",
  "waveshare_rp2040_touch_lcd_1.28",
  "adafruit_feather_rp2040",
]);
const rp2350Boards = new Set([
  "pico2",
  "pico2_w",
  "waveshare_rp2350_lcd_1.47_a",
  "waveshare_rp2350_touch_lcd_1.69",
]);
const imageMethods = ["info", "decodeJPEG", "decodeBMP", "drawJPEG", "drawBMP"];

function source(path) {
  return readFileSync(join(root, path), "utf8");
}

function expectedImageCapability(maxInputBytes) {
  return {
    methods: imageMethods,
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
    maxInputBytes,
    destination: { pixelFormat: "rgb565", byteOrders: ["swapped", "native"] },
  };
}

test("shipping image capability exactly matches compiled current-image support", () => {
  assert.equal(shippingBoardIds.length, 10);
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const capability = descriptor.capabilities.image;
    const advertised = descriptor.features.image;

    assert.equal(Boolean(capability), advertised, `${boardId}.image capability`);
    assert.equal(descriptor.modules.includes("image"), advertised, `${boardId}.image module`);

    if (rp2040Boards.has(boardId)) {
      assert.deepEqual(capability, expectedImageCapability(16 * 1024), boardId);
    } else if (rp2350Boards.has(boardId)) {
      assert.deepEqual(capability, expectedImageCapability(192 * 1024), boardId);
    } else {
      assert.equal(boardId, "seeed_xiao_esp32s3");
      assert.equal(capability, undefined);
      assert.equal(descriptor.modules.includes("image"), false);
    }
  }
});

test("image capability is separate from display, graphics, and onboard inventory", () => {
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const image = descriptor.capabilities.image;
    if (!image) continue;

    for (const unrelated of ["graphics", "framebuffer", "screen", "display", "dvi", "onboard"]) {
      assert.equal(Object.hasOwn(image, unrelated), false, `${boardId}.image.${unrelated}`);
    }
    assert.equal(Object.hasOwn(descriptor.board.devices, "image"), false, `${boardId}.devices.image`);
  }

  const piZero = boardDescriptors.waveshare_rp2040_pizero;
  assert.equal(piZero.features.image, true);
  assert.equal(piZero.features.dvi, true);
  assert.notEqual(piZero.capabilities.image, undefined);

  const xiao = boardDescriptors.seeed_xiao_esp32s3;
  assert.equal(xiao.features.graphics, false);
  assert.equal(xiao.features.screen, false);
  assert.equal(xiao.capabilities.image, undefined);
});

test("generated native registry carries the exact image input ceiling", () => {
  const header = generateRuntimeRegistryHeader();
  assert.match(header, /#define MCUJS_RUNTIME_IMAGE_MAX_INPUT_BYTES 16384/);
  assert.match(header, /#define MCUJS_RUNTIME_IMAGE_MAX_INPUT_BYTES 196608/);

  const espBranch = header.slice(header.indexOf("#elif defined(MCUJS_BOARD_SEEED_XIAO_ESP32S3)"));
  assert.match(espBranch, /#define MCUJS_FEATURE_IMAGE 0/);
  assert.match(espBranch, /#define MCUJS_RUNTIME_IMAGE_MAX_INPUT_BYTES 0/);
  assert.doesNotMatch(espBranch, /X\("image"/);
  assert.doesNotMatch(espBranch, /X\("image",/);
});

test("native and Jerry registration gates match the image build graph", () => {
  const rpBindings = source("host/bindings/CMakeLists.txt");
  assert.match(rpBindings, /image\.c/);
  assert.match(rpBindings, /PICOJPEG_PATH.*picojpeg\.c/);

  const espBindings = source("platform/esp32/main/CMakeLists.txt");
  assert.doesNotMatch(espBindings, /image\.c/);
  assert.doesNotMatch(espBindings, /picojpeg\.c/);

  const imageSource = source("host/bindings/image.c");
  assert.match(imageSource, /#include "runtime_features\.h"/);
  assert.match(imageSource, /#define IMAGE_BUFFER_SIZE MCUJS_RUNTIME_IMAGE_MAX_INPUT_BYTES/);
  assert.doesNotMatch(imageSource, /PICO_RP2350/);
  for (const method of imageMethods) {
    assert.match(imageSource, new RegExp(`js_set_function\\(module, "${method}"`), method);
  }

  const requireSource = source("host/bindings/require.c");
  assert.match(
    requireSource,
    /#if MCUJS_FEATURE_IMAGE[\s\S]*\{"image", js_create_image_module\}[\s\S]*#endif/,
  );
});

test("release tables and portable discovery example expose image independently", () => {
  const docs = generatedBoardDocsSection();
  assert.ok(docs.includes("| Target | GPIO | PWM | I2C | SPI | ADC | NeoPixel | Image (max input) | Graphics | Screen/display | DVI |"));
  assert.match(docs, /`pico`[^\n]*✓ \(16 KiB\)/);
  assert.match(docs, /`pico2`[^\n]*✓ \(192 KiB\)/);
  assert.match(docs, /`seeed_xiao_esp32s3`[^\n]*\| — \| — \| — \| — \|/);

  const example = source("examples/board-discovery/index.js");
  assert.match(example, /modules\.has\('image'\)/);
  assert.match(example, /boardApi\.capability\('image'\)/);
  assert.doesNotMatch(example, /boardApi\.(?:name|chip)\s*={2,3}/);

  const apiReference = source("docs/docs/api-reference.md");
  assert.match(apiReference, /portable image API\s+normalization remains deferred to a later 0\.x release/i);

  const schema = JSON.parse(source("docs/static/schemas/mcujs-portable-api-0.2.schema.json"));
  const contract = schema["x-mcujs-contract"];
  assert.equal(contract.capabilities.image.schema, "#/$defs/imageCapability");
  assert.equal(contract.capabilities.image.portableApi, false);
  assert.equal(contract.capabilities.image.normalization, "deferredLater0x");
  assert.equal(Object.hasOwn(contract.modules, "image"), false);
});
