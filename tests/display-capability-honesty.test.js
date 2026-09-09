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

const graphicsMethods = [
  "createBuffer", "freeBuffer", "getBufferInfo", "getPointer",
  "fill", "setPixel", "fillRect", "color565",
];
const screenMethods = [
  "init", "fill", "setPixel", "fillRect", "drawLine", "drawCircle",
  "fillCircle", "drawText", "rgb", "color", "show", "getWidth",
  "getHeight", "getBufferHandle", "getByteOrder",
];
const screenConstants = [
  "BLACK", "WHITE", "RED", "GREEN", "BLUE", "CYAN", "MAGENTA",
  "YELLOW", "ORANGE", "GRAY",
];
const dviMethods = [
  "init", "start", "stop", "show", "fill", "isRunning",
  "getDrawBuffer", "getBufferSize", "swapAndShow",
];
const rpBoards = new Set(shippingBoardIds.filter((id) =>
  id !== "seeed_xiao_esp32s3" && id !== "waveshare_rp2350_touch_lcd_2.8"));
const onboardPanels = {
  "waveshare_rp2350_touch_lcd_2.8": {
    type: "lcd", controller: "ST7789T3", width: 240, height: 320,
  },
  "waveshare_rp2040_touch_lcd_1.28": {
    type: "lcd", controller: "GC9A01A", width: 240, height: 240,
  },
  "waveshare_rp2350_lcd_1.47_a": {
    type: "lcd", controller: "ST7789V3", width: 172, height: 320,
  },
  "waveshare_rp2350_touch_lcd_1.69": {
    type: "lcd", controller: "ST7789V2", width: 240, height: 280,
  },
};

function source(path) {
  return readFileSync(join(root, path), "utf8");
}

function factorySetNames(nativeSource, setter) {
  return [...nativeSource.matchAll(new RegExp(`js_set_${setter}\\(module, "([^"]+)"`, "g"))]
    .map((match) => match[1]);
}

function expectedGraphics() {
  return {
    methods: graphicsMethods,
    buffer: {
      pixelFormat: "rgb565",
      bytesPerPixel: 2,
      maxWidth: 320,
      maxHeight: 320,
      maxActiveBuffers: 1,
    },
    color565ByteOrder: "swapped",
    pointerAccess: true,
  };
}

function expectedScreen() {
  return {
    methods: screenMethods,
    constants: screenConstants,
    framebuffer: {
      pixelFormat: "rgb565",
      bytesPerPixel: 2,
      maxWidth: 320,
      maxHeight: 240,
      maxActiveBuffers: 1,
      byteOrders: ["native", "swapped"],
    },
    driver: {
      requiredMethods: ["show"],
      optionalMethods: ["init"],
      externalBuffer: true,
    },
  };
}

function expectedDvi() {
  return {
    methods: dviMethods,
    properties: ["width", "height", "byteOrder"],
    framebuffer: {
      pixelFormat: "rgb565",
      byteOrder: "native",
      maxWidth: 160,
      maxHeight: 120,
      doubleBuffered: true,
    },
    output: { width: 640, height: 480, refreshHz: 60 },
  };
}

test("shipping graphics, screen/display, and DVI descriptors match current-image exposure", () => {
  assert.equal(shippingBoardIds.length, 11);
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const graphics = descriptor.capabilities.graphics;
    const screen = descriptor.capabilities.screen;
    const dvi = descriptor.capabilities.dvi;
    const isRp = rpBoards.has(boardId);
    const hasDvi = boardId === "waveshare_rp2040_pizero";

    assert.equal(descriptor.features.graphics, isRp, `${boardId}.graphics feature`);
    assert.equal(descriptor.modules.includes("graphics"), isRp, `${boardId}.graphics module`);
    assert.deepEqual(graphics, isRp ? expectedGraphics() : undefined, `${boardId}.graphics`);

    assert.equal(descriptor.features.screen, isRp, `${boardId}.screen feature`);
    assert.equal(descriptor.modules.includes("screen"), isRp, `${boardId}.screen module`);
    assert.deepEqual(screen, isRp ? expectedScreen() : undefined, `${boardId}.screen`);
    assert.equal(descriptor.modules.includes("display"), false, `${boardId}.display is not a module`);

    assert.equal(descriptor.features.dvi, hasDvi, `${boardId}.dvi feature`);
    assert.equal(descriptor.modules.includes("dvi"), hasDvi, `${boardId}.dvi module`);
    assert.deepEqual(dvi, hasDvi ? expectedDvi() : undefined, `${boardId}.dvi`);
  }
});

test("graphics, screen/display, DVI, image, and onboard panel inventory are independent", () => {
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const panel = descriptor.board.devices.display;
    assert.deepEqual(panel, onboardPanels[boardId], `${boardId}.onboard display`);

    for (const capabilityName of ["graphics", "screen", "dvi"]) {
      const capability = descriptor.capabilities[capabilityName];
      if (!capability) continue;
      for (const unrelated of ["image", "onboard", "panel", "boardName", "chip"]) {
        assert.equal(
          Object.hasOwn(capability, unrelated),
          false,
          `${boardId}.${capabilityName}.${unrelated}`,
        );
      }
    }
  }

  assert.ok(boardDescriptors.pico.capabilities.graphics);
  assert.ok(boardDescriptors.pico.capabilities.screen);
  assert.equal(boardDescriptors.pico.board.devices.display, undefined);
  assert.ok(boardDescriptors.waveshare_rp2040_pizero.capabilities.dvi);
  assert.equal(boardDescriptors.waveshare_rp2040_pizero.board.devices.display, undefined);
  assert.ok(boardDescriptors["waveshare_rp2350_lcd_1.47_a"].board.devices.display);
  assert.equal(boardDescriptors["waveshare_rp2350_lcd_1.47_a"].capabilities.dvi, undefined);
  assert.equal(boardDescriptors.seeed_xiao_esp32s3.capabilities.graphics, undefined);
  assert.equal(boardDescriptors.seeed_xiao_esp32s3.capabilities.screen, undefined);
  assert.equal(boardDescriptors.seeed_xiao_esp32s3.capabilities.dvi, undefined);
});

test("native factories expose only the audited methods and unsupported DVI has no stub", () => {
  const graphics = source("host/bindings/graphics.c");
  const screen = source("host/bindings/screen.c");
  const dvi = source("platform/rp2/bindings/dvi.c");
  const requireSource = source("host/bindings/require.c");

  assert.deepEqual(factorySetNames(graphics, "function"), graphicsMethods);
  assert.deepEqual(factorySetNames(screen, "function"), screenMethods);
  assert.deepEqual(factorySetNames(screen, "number"), screenConstants);
  assert.deepEqual(factorySetNames(dvi, "function"), dviMethods);
  assert.deepEqual(factorySetNames(dvi, "number"), ["width", "height"]);
  assert.deepEqual(factorySetNames(dvi, "string"), ["byteOrder"]);

  assert.match(requireSource, /#if MCUJS_FEATURE_GRAPHICS[\s\S]*\{"graphics", js_create_graphics_module\}/);
  assert.match(requireSource, /#if MCUJS_FEATURE_SCREEN[\s\S]*\{"screen", js_create_screen_module\}/);
  assert.match(requireSource, /#if MCUJS_HAS_DVI[\s\S]*\{"dvi", js_create_dvi_module\}/);
  assert.match(graphics, /js_register_global\("graphics", module\)/);
  assert.match(screen, /js_register_global\("screen", module\)/);
  assert.match(dvi, /js_register_global\("DVI", module\)/);
  assert.doesNotMatch(dvi, /Stub implementations when DVI is not available/);
  assert.doesNotMatch(dvi, /No-op: DVI not available/);
});

test("build graph compiles display sources only into supported images", () => {
  const rpBindings = source("host/bindings/CMakeLists.txt");
  const rpPlatform = source("platform/rp2/platform.cmake");
  const espBindings = source("platform/esp32/main/CMakeLists.txt");

  assert.match(rpBindings, /graphics\.c/);
  assert.match(rpBindings, /screen\.c/);
  assert.match(rpPlatform, /if\(MCUJS_HAS_DVI\)[\s\S]*bindings\/dvi\.c[\s\S]*endif\(\)/);
  assert.doesNotMatch(espBindings, /host\/bindings\/(graphics|screen)\.c/);
  assert.doesNotMatch(espBindings, /bindings\/dvi\.c/);
});

test("generated registry, schema, docs, and portable example derive display discovery", () => {
  const header = generateRuntimeRegistryHeader();
  const esp = header.slice(header.indexOf("#elif defined(MCUJS_BOARD_SEEED_XIAO_ESP32S3)"));
  assert.match(header, /#define MCUJS_RUNTIME_GRAPHICS_MAX_WIDTH 320/);
  assert.match(header, /#define MCUJS_RUNTIME_GRAPHICS_MAX_HEIGHT 320/);
  assert.match(header, /#define MCUJS_RUNTIME_SCREEN_MAX_WIDTH 320/);
  assert.match(header, /#define MCUJS_RUNTIME_SCREEN_MAX_HEIGHT 240/);
  assert.match(header, /#define MCUJS_RUNTIME_DVI_MAX_WIDTH 160/);
  assert.match(header, /#define MCUJS_RUNTIME_DVI_MAX_HEIGHT 120/);
  assert.match(esp, /#define MCUJS_RUNTIME_GRAPHICS_MAX_WIDTH 0/);
  assert.match(esp, /#define MCUJS_RUNTIME_SCREEN_MAX_WIDTH 0/);
  assert.match(esp, /#define MCUJS_RUNTIME_DVI_MAX_WIDTH 0/);
  assert.doesNotMatch(esp, /X\("(?:graphics|screen|dvi)"/);

  const schema = JSON.parse(source("docs/static/schemas/mcujs-portable-api-0.2.schema.json"));
  const contract = schema["x-mcujs-contract"];
  for (const name of ["graphics", "screen", "dvi"]) {
    assert.equal(contract.capabilities[name].schema, `#/$defs/${name}Capability`);
    assert.equal(contract.capabilities[name].portableApi, false);
    assert.equal(contract.capabilities[name].normalization, "deferredLater0x");
    assert.equal(Object.hasOwn(contract.modules, name), false);
  }

  const docs = generatedBoardDocsSection();
  assert.match(docs, /graphics buffers, screen\/display[\s\S]*DVI are independent/);
  assert.match(docs, /`waveshare_rp2040_pizero`[^\n]*\| ✓ \| ✓ \| ✓ \|/);
  assert.match(docs, /`seeed_xiao_esp32s3`[^\n]*\| — \| — \| — \|/);

  const example = source("examples/board-discovery/index.js");
  for (const name of ["graphics", "screen", "dvi"]) {
    assert.match(example, new RegExp(`modules\\.has\\('${name}'\\)`), name);
    assert.match(example, new RegExp(`boardApi\\.capability\\('${name}'\\)`), name);
  }
  assert.doesNotMatch(example, /boardApi\.(?:name|chip)\s*={2,3}/);

  const apiReference = source("docs/docs/api-reference.md");
  assert.match(apiReference, /portable graphics and display API\s+normalization remains deferred\s+to a later 0\.x release/i);
});
