const assert = require("node:assert/strict");
const { execFileSync } = require("node:child_process");
const { readFileSync } = require("node:fs");
const { join } = require("node:path");
const test = require("node:test");

const root = join(__dirname, "..");
const {
  boardDescriptors,
  featureNames,
  shippingBoardIds,
  manifestFor,
} = require("../runtime/board-registry.js");
const {
  generatedBoardDocsSection,
  generateRuntimeRegistryHeader,
  generatedHeaderPath,
} = require("../scripts/generate-runtime-registry.js");
const {
  validatePortableApiManifest,
} = require("../scripts/validate-portable-api-manifest.js");

const expectedBoards = [
  "pico",
  "pico2",
  "pico2_w",
  "waveshare_rp2040_zero",
  "waveshare_rp2040_pizero",
  "waveshare_rp2040_touch_lcd_1.28",
  "waveshare_rp2350_lcd_1.47_a",
  "waveshare_rp2350_touch_lcd_1.69",
  "waveshare_rp2350_touch_lcd_2.8",
  "adafruit_feather_rp2040",
  "seeed_xiao_esp32s3",
];

const moduleCapability = {
  fs: "fs",
  gpio: "gpio",
  pwm: "pwm",
  i2c: "i2c",
  spi: "spi",
  adc: "adc",
  neopixel: "neopixel",
  image: "image",
  jpeg: "jpeg",
  graphics: "graphics",
  screen: "screen",
  dvi: "dvi",
};

const capabilityGatedBoardExports = {
  safeMode: (descriptor) => descriptor.capabilities.boot?.safeMode === true,
  storageReady: (descriptor) => Object.hasOwn(descriptor.capabilities, "fs"),
};

const expectedPresentation = {
  pico: { label: "Raspberry Pi Pico", flash: "2MB", notes: "Onboard LED" },
  pico2: { label: "Raspberry Pi Pico 2", flash: "4MB", notes: "Onboard LED" },
  pico2_w: { label: "Raspberry Pi Pico 2 W", flash: "4MB", notes: "CYW43 LED support" },
  waveshare_rp2040_zero: { label: "Waveshare RP2040-Zero", flash: "2MB", notes: "Onboard NeoPixel" },
  waveshare_rp2040_pizero: { label: "Waveshare RP2040-PiZero", flash: "16MB", notes: "DVI/HDMI output" },
  "waveshare_rp2040_touch_lcd_1.28": { label: "Waveshare RP2040 Touch LCD 1.28", flash: "4MB", notes: "Round LCD, touch, IMU" },
  "waveshare_rp2350_lcd_1.47_a": { label: "Waveshare RP2350-LCD-1.47-A", flash: "16MB", notes: "LCD, NeoPixel" },
  "waveshare_rp2350_touch_lcd_1.69": { label: "Waveshare RP2350-Touch-LCD-1.69", flash: "16MB", notes: "LCD, touch, IMU, buzzer" },
  "waveshare_rp2350_touch_lcd_2.8": { label: "Waveshare RP2350-Touch-LCD-2.8", flash: "16MB", notes: "Initial runtime/USB/filesystem port; LCD hardware only, experimental Canvas opt-in; touch/audio/SD/sensors unsupported" },
  adafruit_feather_rp2040: { label: "Adafruit Feather RP2040", flash: "8MB", notes: "NeoPixel, STEMMA QT" },
  seeed_xiao_esp32s3: { label: "Seeed Studio XIAO ESP32-S3", flash: "8MB", notes: "Native USB, onboard LED" },
};

const expectedOnboardDevices = {
  pico: {
    led: { type: "gpio", pin: 25, activeLow: false },
    button: { type: "managed", name: "BOOTSEL", activeLow: true, readOnly: true },
  },
  pico2: { led: { type: "gpio", pin: 25, activeLow: false } },
  pico2_w: { led: { type: "managed" } },
  waveshare_rp2040_zero: { neopixel: { type: "neopixel", pin: 16, length: 1, order: "RGB" } },
  waveshare_rp2040_pizero: {},
  "waveshare_rp2040_touch_lcd_1.28": {
    display: { type: "lcd", controller: "GC9A01A", width: 240, height: 240 },
  },
  "waveshare_rp2350_lcd_1.47_a": {
    neopixel: { type: "neopixel", pin: 22, length: 1, order: "GRB" },
    display: { type: "lcd", controller: "ST7789V3", width: 172, height: 320 },
  },
  "waveshare_rp2350_touch_lcd_1.69": {
    display: { type: "lcd", controller: "ST7789V2", width: 240, height: 280 },
  },
  "waveshare_rp2350_touch_lcd_2.8": {
    display: { type: "lcd", controller: "ST7789T3", width: 240, height: 320 },
  },
  adafruit_feather_rp2040: {
    led: { type: "gpio", pin: 13, activeLow: false },
    neopixel: { type: "neopixel", pin: 16, length: 1, order: "GRB" },
  },
  seeed_xiao_esp32s3: {
    led: { type: "gpio", pin: 21, activeLow: true },
    button: { type: "managed", name: "BOOT", activeLow: true, readOnly: true },
  },
};

test("only Pico RP2040 and XIAO advertise a managed read-only onboard button", () => {
  const names = { pico: "BOOTSEL", seeed_xiao_esp32s3: "BOOT" };
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    assert.equal(descriptor.features.onboardButton, Object.hasOwn(names, boardId), boardId);
    if (names[boardId]) {
      assert.deepEqual(descriptor.board.devices.button, {
        type: "managed", name: names[boardId], activeLow: true, readOnly: true,
      });
      const manifest = manifestFor(boardId);
      assert.equal(validatePortableApiManifest(manifest).valid, true);
      manifest.board.devices.button.pin = 0;
      assert.equal(validatePortableApiManifest(manifest).valid, false, "managed buttons do not expose pins");
    } else {
      assert.equal("button" in descriptor.board.devices, false, boardId);
    }
  }
  const xiao = boardDescriptors.seeed_xiao_esp32s3;
  assert.equal(xiao.board.exposedPins.includes(0), false);
  for (const name of ["gpio", "pwm", "adc", "neopixel"]) {
    assert.equal(xiao.capabilities[name].pins.includes(0), false, name);
  }
  assert.equal(boardDescriptors.pico.features.dvi, false, "BOOTSEL must not race DVI core 1");
});

test("all shipping boards have explicit, closed feature maps", () => {
  assert.equal(
    featureNames.includes("storageReady"),
    false,
    "dynamic storageReady() availability must derive from the static fs capability",
  );
  assert.deepEqual(shippingBoardIds, expectedBoards);
  assert.deepEqual(Object.keys(boardDescriptors), [...expectedBoards, "waveshare_esp32s3_epaper_1.54_v2", "seeed_reterminal_sticky"]);

  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    assert.equal(descriptor.board.name, boardId);
    assert.deepEqual(
      Object.keys(descriptor.features).sort(),
      [...featureNames].sort(),
      `${boardId} must explicitly select every firmware feature`,
    );
    for (const [name, enabled] of Object.entries(descriptor.features)) {
      assert.equal(typeof enabled, "boolean", `${boardId}.${name}`);
    }
  }
});

test("all eleven board identities, presentation rows, and onboard inventories are explicit", () => {
  assert.deepEqual(Object.keys(expectedPresentation), expectedBoards);
  assert.deepEqual(Object.keys(expectedOnboardDevices), expectedBoards);
  const version = readFileSync(join(root, "version.txt"), "utf8").trim();
  for (const boardId of expectedBoards) {
    const descriptor = boardDescriptors[boardId];
    assert.deepEqual(descriptor.presentation, expectedPresentation[boardId], `${boardId}.presentation`);
    assert.deepEqual(descriptor.board.devices, expectedOnboardDevices[boardId], `${boardId}.devices`);
    assert.equal(descriptor.board.firmwareVersion, version, `${boardId}.firmwareVersion`);
  }
});

test("Docusaurus board tables are generated exactly from the shared registry", () => {
  const docs = readFileSync(join(root, "docs/docs/hardware-boards.md"), "utf8");
  assert.ok(
    docs.includes(generatedBoardDocsSection()),
    "docs/docs/hardware-boards.md does not contain the generated board tables",
  );
});

test("shell release metadata agrees exactly with the shared board registry", () => {
  const rows = execFileSync(
    "bash",
    ["-c", `source "${join(root, "scripts/lib/boards.sh")}"; for board in "\${MCUJS_RELEASE_BOARDS[@]}"; do printf '%s\\t%s\\t%s\\t%s\\t%s\\n' "$board" "$(mcujs_board_label "$board")" "$(mcujs_board_chip "$board")" "$(mcujs_board_flash "$board")" "$(mcujs_board_features "$board")"; done`],
    { encoding: "utf8" },
  ).trim().split("\n");
  assert.equal(rows.length, expectedBoards.length);
  for (const row of rows) {
    const [boardId, label, chip, flash, notes] = row.split("\t");
    assert.equal(chip, boardDescriptors[boardId].board.chip, `${boardId}.chip`);
    assert.deepEqual({ label, flash, notes }, boardDescriptors[boardId].presentation, `${boardId}.presentation`);
  }
});

test("release packaging covers every shipping capability descriptor", () => {
  const releaseBoardIds = execFileSync(
    "bash",
    ["-c", `source "${join(root, "scripts/lib/boards.sh")}"; printf '%s\\n' "\${MCUJS_RELEASE_BOARDS[@]}"`],
    { encoding: "utf8" },
  ).trim().split("\n");
  assert.deepEqual(releaseBoardIds, shippingBoardIds);
});

test("every descriptor produces a strict, semantically valid release manifest", () => {
  for (const boardId of shippingBoardIds) {
    const manifest = manifestFor(boardId);
    const result = validatePortableApiManifest(manifest);
    assert.equal(result.valid, true, `${boardId}: ${JSON.stringify(result.errors)}`);
    assert.equal(manifest.board.name, boardId);
    assert.equal(manifest.apiVersion, "0.2");
    assert.equal("features" in manifest, false);
  }
});

test("module availability, capabilities, and onboard inventory cannot contradict", () => {
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const modules = new Set(descriptor.modules);
    assert.ok(modules.has("board"));
    assert.ok(modules.has("mcujs:module"));
    assert.ok(modules.has("node:module"));

    for (const [moduleName, capabilityName] of Object.entries(moduleCapability)) {
      assert.equal(
        modules.has(moduleName),
        Object.hasOwn(descriptor.capabilities, capabilityName),
        `${boardId}.${moduleName}`,
      );
    }
    assert.equal(modules.has("process"), descriptor.features.process);
    assert.equal(modules.has("keyboard"), descriptor.features.keyboard);
    assert.equal(modules.has("mouse"), descriptor.features.mouse);

    assert.equal(
      Object.hasOwn(descriptor.board.devices, "neopixel"),
      descriptor.features.onboardNeopixel,
      `${boardId} onboard NeoPixel inventory`,
    );
    assert.equal(
      Object.hasOwn(descriptor.board.devices, "led"),
      descriptor.features.onboardLed,
      `${boardId} onboard LED inventory`,
    );
  }
});

test("capability-gated board exports exactly match their advertised gates", () => {
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    for (const [exportName, gateAdvertised] of Object.entries(capabilityGatedBoardExports)) {
      const advertised = exportName === "storageReady"
        ? Object.hasOwn(descriptor.capabilities, "fs")
        : descriptor.features[exportName];
      assert.equal(
        advertised,
        gateAdvertised(descriptor),
        `${boardId}.board.${exportName}`,
      );
    }
  }
});

test("filesystem, MSC transfer, and storageReady support share one static registry contract", () => {
  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const filesystem = descriptor.capabilities.fs;
    const usbClasses = descriptor.capabilities.usb?.classes ?? [];

    assert.equal(Boolean(filesystem), descriptor.modules.includes("fs"), `${boardId}.fs module`);
    assert.equal(
      filesystem?.hostTransfer === true,
      usbClasses.includes("msc"),
      `${boardId}.MSC transfer`,
    );
    assert.equal(
      Object.hasOwn(descriptor.features, "storageReady"),
      false,
      `${boardId}.storageReady must not be a duplicated feature flag`,
    );
    if (filesystem) {
      assert.equal(Object.hasOwn(filesystem, "ready"), false, `${boardId}.fs.ready is dynamic`);
      assert.equal(Object.hasOwn(filesystem, "owner"), false, `${boardId}.fs.owner is dynamic`);
    }
  }

  const generator = readFileSync(join(root, "scripts/generate-runtime-registry.js"), "utf8");
  assert.equal(generator.includes("features.storageReady"), false);
});

test("full and constrained feature maps remain intentionally different", () => {
  const full = boardDescriptors.pico;
  assert.equal(full.features.image, true);
  assert.equal(full.capabilities.image.maxInputBytes, 16 * 1024);
  assert.equal(full.features.keyboard, true);
  assert.equal(full.features.mouse, true);
  assert.deepEqual(full.capabilities.usb.classes, [
    "cdc",
    "msc",
    "keyboardHid",
    "mouseHid",
  ]);

  const constrained = boardDescriptors.seeed_xiao_esp32s3;
  assert.equal(constrained.features.image, false);
  assert.equal("image" in constrained.capabilities, false);
  assert.equal(constrained.features.keyboard, false);
  assert.equal(constrained.features.mouse, false);
  assert.equal(constrained.features.graphics, false);
  assert.equal(constrained.features.screen, false);
  assert.equal(constrained.features.onboardNeopixel, false);
  assert.equal("neopixel" in constrained.board.devices, false);
  assert.deepEqual(constrained.board.exposedPins, [1, 2, 3, 4, 5, 6, 7, 8, 9, 21]);
  assert.deepEqual(constrained.capabilities.usb.classes, ["cdc", "msc"]);
});

test("XIAO ESP32-S3 pin aliases exactly match the exposed header", () => {
  const xiao = boardDescriptors.seeed_xiao_esp32s3;
  assert.deepEqual(xiao.board.pins, {
    D0: 1, D1: 2, D2: 3, D3: 4, D4: 5, D5: 6,
    D8: 7, D9: 8, D10: 9,
    A0: 1, A1: 2, A2: 3, A3: 4, A4: 5, A5: 6, A6: 7, A7: 8, A8: 9,
    SDA: 5, SCL: 6, SCK: 7, MISO: 8, MOSI: 9, LED: 21,
  });
  assert.equal(Object.hasOwn(xiao.board.pins, "D6"), false);
  assert.equal(Object.hasOwn(xiao.board.pins, "D7"), false);
  assert.equal(Object.hasOwn(xiao.board.pins, "D21"), false);
});

test("external NeoPixel capability is independent from onboard inventory", () => {
  const onboardBoards = new Set([
    "waveshare_rp2040_zero",
    "waveshare_rp2350_lcd_1.47_a",
    "adafruit_feather_rp2040",
  ]);

  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const capability = descriptor.capabilities.neopixel;
    if (boardId === "waveshare_rp2350_touch_lcd_2.8") {
      assert.equal(descriptor.features.neopixel, false);
      assert.equal(capability, undefined);
      assert.equal(descriptor.modules.includes("neopixel"), false);
      assert.equal(descriptor.board.devices.neopixel, undefined);
      continue;
    }
    assert.equal(descriptor.features.neopixel, true, `${boardId} external driver`);
    assert.ok(descriptor.modules.includes("neopixel"), `${boardId} module`);
    assert.deepEqual(capability.orders, ["RGB", "GRB"], `${boardId} orders`);
    assert.equal(capability.maxLength, 256, `${boardId} maximum length`);
    assert.ok(capability.pins.length > 0, `${boardId} driver pins`);
    for (const pin of capability.pins) {
      assert.ok(
        descriptor.capabilities.gpio.outputPins.includes(pin),
        `${boardId} NeoPixel pin ${pin} is not an advertised output`,
      );
    }

    const onboard = descriptor.board.devices.neopixel;
    assert.equal(Boolean(onboard), onboardBoards.has(boardId), `${boardId} onboard inventory`);
    assert.equal("onboard" in capability, false, `${boardId} capability leaked inventory`);
    if (onboard) {
      assert.ok(capability.pins.includes(onboard.pin), `${boardId} onboard pin`);
      assert.ok(capability.orders.includes(onboard.order), `${boardId} onboard order`);
      assert.ok(onboard.length <= capability.maxLength, `${boardId} onboard length`);
    }
  }

  const xiao = boardDescriptors.seeed_xiao_esp32s3;
  assert.equal(xiao.features.onboardNeopixel, false);
  assert.equal("neopixel" in xiao.board.devices, false);
  assert.ok(xiao.modules.includes("neopixel"));
  assert.ok(xiao.capabilities.neopixel);
});

test("ADC channels, aliases, calibration, and temperature metadata are truthful for every board", () => {
  const vsysBoards = new Set(["pico", "pico2"]);

  for (const boardId of shippingBoardIds) {
    const descriptor = boardDescriptors[boardId];
    const adc = descriptor.capabilities.adc;
    assert.equal(Boolean(adc), descriptor.features.adc, `${boardId}.adc availability`);
    if (!adc) continue;

    assert.equal(adc.resolutionBits, 12, `${boardId}.adc.resolutionBits`);
    assert.equal(adc.voltage.supported, true, `${boardId}.adc.voltage.supported`);
    assert.equal(
      adc.voltage.calibrated,
      boardId === "seeed_xiao_esp32s3",
      `${boardId}.adc.voltage.calibrated`,
    );
    assert.deepEqual(
      adc.temperature,
      { supported: true, rawChannel: boardId !== "seeed_xiao_esp32s3" },
      `${boardId}.adc.temperature`,
    );
    assert.equal(adc.vsys, vsysBoards.has(boardId), `${boardId}.adc.vsys`);

    assert.deepEqual(
      adc.channels.map(({ pin }) => pin),
      adc.pins,
      `${boardId}.adc channel/pin order`,
    );
    for (const channel of adc.channels) {
      const expectedChannel = boardId === "seeed_xiao_esp32s3"
        ? channel.pin - 1
        : channel.pin - 26;
      assert.equal(channel.channel, expectedChannel, `${boardId}.adc GPIO/channel mapping`);
      for (const alias of channel.aliases) {
        assert.equal(
          descriptor.board.pins[alias],
          channel.pin,
          `${boardId}.adc alias ${alias}`,
        );
      }
    }
  }
});

test("reserved implementation pins are absent from public RP maps", () => {
  for (const boardId of ["pico", "pico2", "pico2_w"]) {
    const pins = boardDescriptors[boardId].board.exposedPins;
    assert.equal(pins.includes(23), false, `${boardId} internal SMPS pin`);
    assert.equal(pins.includes(24), false, `${boardId} internal VBUS sense pin`);
    assert.equal(pins.includes(29), false, `${boardId} internal VSYS/wireless pin`);
  }

  const piZero = boardDescriptors.waveshare_rp2040_pizero;
  for (let pin = 22; pin <= 29; pin += 1) {
    assert.equal(piZero.board.exposedPins.includes(pin), false, `DVI pin ${pin}`);
  }
  assert.equal(piZero.features.adc, false);
  assert.equal("adc" in piZero.capabilities, false);
});

test("RP PWM limits count distinct reachable hardware outputs and slices", () => {
  const expected = {
    pico: { maxOutputs: 16, timerCount: 8 },
    pico2: { maxOutputs: 16, timerCount: 8 },
    pico2_w: { maxOutputs: 16, timerCount: 8 },
    waveshare_rp2040_zero: { maxOutputs: 16, timerCount: 8 },
    waveshare_rp2040_pizero: { maxOutputs: 16, timerCount: 8 },
    "waveshare_rp2040_touch_lcd_1.28": { maxOutputs: 11, timerCount: 6 },
    "waveshare_rp2350_lcd_1.47_a": { maxOutputs: 11, timerCount: 6 },
    "waveshare_rp2350_touch_lcd_1.69": { maxOutputs: 9, timerCount: 5 },
    adafruit_feather_rp2040: { maxOutputs: 11, timerCount: 6 },
  };

  for (const [boardId, limits] of Object.entries(expected)) {
    const pwm = boardDescriptors[boardId].capabilities.pwm;
    const outputs = new Set();
    const slices = new Set();
    for (const pin of pwm.pins) {
      const slice = pin < 32 ? ((pin >> 1) & 7) : 8 + ((pin >> 1) & 3);
      outputs.add(`${slice}:${pin & 1}`);
      slices.add(slice);
    }
    assert.equal(pwm.maxOutputs, outputs.size, `${boardId}.pwm.maxOutputs`);
    assert.equal(pwm.timerCount, slices.size, `${boardId}.pwm.timerCount`);
    assert.deepEqual(
      { maxOutputs: pwm.maxOutputs, timerCount: pwm.timerCount },
      limits,
      `${boardId}.pwm physical limits`,
    );
  }
});

test("PWM descriptors expose exact ratio units and truthful physical limits", () => {
  for (const boardId of shippingBoardIds) {
    if (boardId === "waveshare_rp2350_touch_lcd_2.8") {
      assert.equal(boardDescriptors[boardId].capabilities.pwm, undefined);
      continue;
    }
    assert.deepEqual(
      boardDescriptors[boardId].capabilities.pwm.duty,
      { min: 0, max: 1, unit: "ratio" },
      `${boardId}.pwm.duty`,
    );
  }

  const pwm = boardDescriptors.seeed_xiao_esp32s3.capabilities.pwm;
  assert.deepEqual(pwm.pins, [1, 2, 3, 4, 5, 6, 7, 8, 9]);
  assert.equal(pwm.maxOutputs, 8, "ESP32-S3 LEDC low-speed channel count");
  assert.equal(pwm.timerCount, 4, "ESP32-S3 LEDC low-speed timer count");
  assert.deepEqual(pwm.frequency, {
    minHz: 10,
    maxHz: 1000000,
    resolutionVaries: true,
  });
});

test("the checked-in C registry is generated exactly from board descriptors", () => {
  const generated = generateRuntimeRegistryHeader();
  const checkedIn = readFileSync(join(root, generatedHeaderPath), "utf8");
  assert.equal(checkedIn, generated);
});
