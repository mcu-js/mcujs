"use strict";

const { readFileSync } = require("node:fs");
const { join } = require("node:path");

const firmwareVersion = readFileSync(join(__dirname, "..", "version.txt"), "utf8").trim();

const featureNames = Object.freeze([
  "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi",
  "adc", "neopixel", "process", "require", "fs", "image", "keyboard", "mouse",
  "graphics", "screen", "dvi", "onboardLed", "onboardNeopixel", "onboardButton", "safeMode",
]);

const moduleOrder = Object.freeze([
  "board", "fs", "process", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
  "image", "keyboard", "mouse", "graphics", "screen", "dvi",
  "mcujs:module", "node:module",
]);

const boardPresentation = Object.freeze({
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
});

const featureModule = Object.freeze({
  fs: "fs", process: "process", gpio: "gpio", pwm: "pwm", i2c: "i2c",
  spi: "spi", adc: "adc", neopixel: "neopixel", image: "image",
  keyboard: "keyboard", mouse: "mouse", graphics: "graphics", screen: "screen",
  dvi: "dvi",
});

const rpUsbClasses = Object.freeze(["cdc", "msc", "keyboardHid", "mouseHid"]);
const espUsbClasses = Object.freeze(["cdc", "msc"]);

function featureMap(...enabledNames) {
  const features = Object.fromEntries(featureNames.map((name) => [name, false]));
  for (const name of enabledNames) {
    if (!Object.hasOwn(features, name)) throw new Error(`unknown firmware feature: ${name}`);
    if (features[name]) throw new Error(`duplicate firmware feature: ${name}`);
    features[name] = true;
  }
  return Object.freeze(features);
}

function pinsBetween(first, last) {
  return Array.from({ length: last - first + 1 }, (_, index) => first + index);
}

function pinAliases(exposedPins, additions = {}) {
  const pins = {};
  for (const pin of exposedPins) pins[`D${pin}`] = pin;
  return { ...pins, ...additions };
}

function gpioCapability(pins, outputPins = pins) {
  return { pins: [...pins], outputPins: [...outputPins], modes: ["input", "output", "inputPullup", "inputPulldown"] };
}

function rpPwmSlice(pin) {
  return pin < 32 ? ((pin >> 1) & 7) : 8 + ((pin >> 1) & 3);
}

function pwmCapability(pins, chip) {
  const rp2350 = chip === "RP2350";
  const slices = new Set(pins.map((pin) => rpPwmSlice(pin)));
  const outputs = new Set(pins.map((pin) => (rpPwmSlice(pin) * 2) + (pin & 1)));
  return {
    pins: [...pins],
    maxOutputs: outputs.size,
    timerCount: slices.size,
    duty: { min: 0, max: 1, unit: "ratio" },
    frequency: {
      minHz: rp2350 ? 10 : 8,
      maxHz: rp2350 ? 2048 : 1600,
      resolutionVaries: true,
    },
  };
}

function adcCapability(pins, aliases, options = {}) {
  const channels = pins.map((pin, channel) => ({
    channel,
    pin,
    aliases: aliases[pin] ? [aliases[pin]] : [],
  }));
  return {
    resolutionBits: 12,
    pins: [...pins],
    channels,
    voltage: { supported: true, calibrated: options.calibrated ?? false, minVolts: 0, maxVolts: 3.3 },
    temperature: { supported: true, rawChannel: options.rawChannel ?? true },
    vsys: options.vsys ?? false,
  };
}

function i2cCapability(routes, defaultBus = routes[0].bus, maxHz = 1000000) {
  const defaultRoute = routes.find((route) => route.bus === defaultBus);
  return {
    buses: [...new Set(routes.map((route) => route.bus))],
    routes: routes.map((route) => ({ ...route })),
    defaultBus,
    defaultRoute: { ...defaultRoute },
    frequency: { minHz: 1, maxHz },
    maxTransferBytes: 256,
  };
}

function spiCapability(routes, defaultBus = routes[0].bus, options = {}) {
  const defaultRoute = routes.find((route) => route.bus === defaultBus);
  return {
    buses: [...new Set(routes.map((route) => route.bus))],
    routes: routes.map((route) => ({ ...route })),
    defaultBus,
    defaultRoute: { ...defaultRoute },
    frequency: { minHz: 1, maxHz: options.maxHz ?? 62500000 },
    maxTransferBytes: options.maxTransferBytes ?? 256,
    modes: [0],
    bitsPerWord: [8],
    bitOrders: ["msb"],
    fullDuplex: true,
    dma: options.dma ?? true,
    ...(options.dma ? { compatibilityExtensions: ["writeBufferDMA"] } : {}),
  };
}

function neopixelCapability(pins, maxLength = 256) {
  return { pins: [...pins], maxLength, orders: ["RGB", "GRB"] };
}

function imageCapability(chip) {
  return {
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
    maxInputBytes: chip === "RP2350" ? 192 * 1024 : 16 * 1024,
    destination: { pixelFormat: "rgb565", byteOrders: ["swapped", "native"] },
  };
}

function graphicsCapability() {
  return {
    methods: [
      "createBuffer", "freeBuffer", "getBufferInfo", "getPointer",
      "fill", "setPixel", "fillRect", "color565",
    ],
    buffer: {
      pixelFormat: "rgb565", bytesPerPixel: 2,
      maxWidth: 320, maxHeight: 320, maxActiveBuffers: 1,
    },
    color565ByteOrder: "swapped",
    pointerAccess: true,
  };
}

function screenCapability() {
  return {
    methods: [
      "init", "fill", "setPixel", "fillRect", "drawLine", "drawCircle",
      "fillCircle", "drawText", "rgb", "color", "show", "getWidth",
      "getHeight", "getBufferHandle", "getByteOrder",
    ],
    constants: [
      "BLACK", "WHITE", "RED", "GREEN", "BLUE", "CYAN", "MAGENTA",
      "YELLOW", "ORANGE", "GRAY",
    ],
    framebuffer: {
      pixelFormat: "rgb565", bytesPerPixel: 2,
      maxWidth: 320, maxHeight: 240, maxActiveBuffers: 1,
      byteOrders: ["native", "swapped"],
    },
    driver: {
      requiredMethods: ["show"], optionalMethods: ["init"], externalBuffer: true,
    },
  };
}

function dviCapability() {
  return {
    methods: [
      "init", "start", "stop", "show", "fill", "isRunning",
      "getDrawBuffer", "getBufferSize", "swapAndShow",
    ],
    properties: ["width", "height", "byteOrder"],
    framebuffer: {
      pixelFormat: "rgb565", byteOrder: "native",
      maxWidth: 160, maxHeight: 120, doubleBuffered: true,
    },
    output: { width: 640, height: 480, refreshHz: 60 },
  };
}

function usbCapability(classes) {
  return { classes: [...classes] };
}

function modulesFor(features) {
  return moduleOrder.filter((name) => {
    if (name === "board") return features.board;
    if (name === "mcujs:module" || name === "node:module") return features.require;
    return features[featureModule[name]];
  });
}

const rpFeatureMaps = Object.freeze({
  pico: featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen", "onboardLed", "onboardButton",
  ),
  pico2: featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen", "onboardLed",
  ),
  pico2_w: featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen", "onboardLed",
  ),
  waveshare_rp2040_zero: featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen", "onboardNeopixel",
  ),
  waveshare_rp2040_pizero: featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen", "dvi",
  ),
  "waveshare_rp2040_touch_lcd_1.28": featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen",
  ),
  "waveshare_rp2350_lcd_1.47_a": featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen", "onboardNeopixel",
  ),
  "waveshare_rp2350_touch_lcd_1.69": featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen",
  ),
  "waveshare_rp2350_touch_lcd_2.8": featureMap(
    "moduleLoader", "console", "timers", "board", "process", "require", "fs",
  ),
  adafruit_feather_rp2040: featureMap(
    "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
    "process", "require", "fs", "image", "keyboard", "mouse", "graphics", "screen", "onboardLed", "onboardNeopixel",
  ),
});

function rpDescriptor({
  name,
  chip,
  exposedPins,
  aliases,
  devices = {},
  features,
  gpioPins = exposedPins,
  gpioOutputPins = gpioPins,
  pwmPins = gpioPins,
  adcPins = [26, 27, 28],
  adcAliases = { 26: "A0", 27: "A1", 28: "A2" },
  adcOptions = {},
  i2cRoutes,
  i2cDefaultBus = 0,
  spiRoutes,
  spiDefaultBus = 0,
  neopixelPins = gpioPins,
}) {
  const capabilities = {
    ...(features.gpio ? { gpio: gpioCapability(gpioPins, gpioOutputPins) } : {}),
    ...(features.pwm ? { pwm: pwmCapability(pwmPins, chip) } : {}),
    fs: { implementation: "fat", writable: true, hostTransfer: true },
    usb: usbCapability(rpUsbClasses.filter((name) =>
      name === "keyboardHid" ? features.keyboard : name === "mouseHid" ? features.mouse : true)),
  };
  if (features.adc) capabilities.adc = adcCapability(adcPins, adcAliases, adcOptions);
  if (features.i2c) capabilities.i2c = i2cCapability(i2cRoutes, i2cDefaultBus);
  if (features.spi) capabilities.spi = spiCapability(spiRoutes, spiDefaultBus, { dma: true });
  if (features.neopixel) capabilities.neopixel = neopixelCapability(neopixelPins);
  if (features.image) capabilities.image = imageCapability(chip);
  if (features.graphics) capabilities.graphics = graphicsCapability();
  if (features.screen) capabilities.screen = screenCapability();
  if (features.dvi) capabilities.dvi = dviCapability();
  return {
    board: { name, chip, firmwareVersion, exposedPins: [...exposedPins], pins: { ...aliases }, devices: structuredClone(devices) },
    features,
    modules: modulesFor(features),
    capabilities,
  };
}

const picoExposed = [...pinsBetween(0, 22), 25, 26, 27, 28];
const picoBusAliases = { A0: 26, A1: 27, A2: 28, SDA: 4, SCL: 5, SCK: 18, MOSI: 19, MISO: 16 };
const picoI2c = [
  { bus: 0, sda: 4, scl: 5 },
  { bus: 0, sda: 8, scl: 9 },
  { bus: 1, sda: 6, scl: 7 },
];
const picoSpi = [{ bus: 0, sck: 18, mosi: 19, miso: 16 }, { bus: 1, sck: 10, mosi: 11, miso: 12 }];

const boardDescriptors = {
  pico: rpDescriptor({
    name: "pico", chip: "RP2040", exposedPins: picoExposed,
    features: rpFeatureMaps.pico,
    aliases: pinAliases(picoExposed, { ...picoBusAliases, LED: 25 }),
    devices: {
      led: { type: "gpio", pin: 25, activeLow: false },
      button: { type: "managed", name: "BOOTSEL", activeLow: true, readOnly: true },
    },
    i2cRoutes: picoI2c, spiRoutes: picoSpi, adcOptions: { vsys: true },
  }),
  pico2: rpDescriptor({
    name: "pico2", chip: "RP2350", exposedPins: picoExposed,
    features: rpFeatureMaps.pico2,
    aliases: pinAliases(picoExposed, { ...picoBusAliases, LED: 25 }),
    devices: { led: { type: "gpio", pin: 25, activeLow: false } },
    i2cRoutes: picoI2c, spiRoutes: picoSpi, adcOptions: { vsys: true },
  }),
  pico2_w: rpDescriptor({
    name: "pico2_w", chip: "RP2350", exposedPins: [...pinsBetween(0, 22), 26, 27, 28],
    features: rpFeatureMaps.pico2_w,
    aliases: pinAliases([...pinsBetween(0, 22), 26, 27, 28], picoBusAliases),
    devices: { led: { type: "managed" } }, i2cRoutes: picoI2c, spiRoutes: picoSpi,
    adcOptions: { vsys: false },
  }),
  waveshare_rp2040_zero: rpDescriptor({
    name: "waveshare_rp2040_zero", chip: "RP2040",
    features: rpFeatureMaps.waveshare_rp2040_zero,
    exposedPins: [...pinsBetween(0, 16), 26, 27, 28, 29],
    aliases: pinAliases([...pinsBetween(0, 16), 26, 27, 28, 29], {
      A0: 26, A1: 27, A2: 28, A3: 29, SDA: 4, SCL: 5, SCK: 10, MOSI: 11, MISO: 12, NEOPIXEL: 16,
    }),
    devices: { neopixel: { type: "neopixel", pin: 16, length: 1, order: "RGB" } },
    i2cRoutes: picoI2c,
    spiRoutes: [{ bus: 1, sck: 10, mosi: 11, miso: 12 }],
    spiDefaultBus: 1,
    adcPins: [26, 27, 28, 29], adcAliases: { 26: "A0", 27: "A1", 28: "A2", 29: "A3" },
  }),
  waveshare_rp2040_pizero: rpDescriptor({
    name: "waveshare_rp2040_pizero", chip: "RP2040", exposedPins: pinsBetween(0, 21),
    features: rpFeatureMaps.waveshare_rp2040_pizero,
    aliases: pinAliases(pinsBetween(0, 21), { SDA: 2, SCL: 3, SCK: 18, MOSI: 19, MISO: 16 }),

    i2cRoutes: [{ bus: 0, sda: 0, scl: 1 }, { bus: 1, sda: 2, scl: 3 }],
    i2cDefaultBus: 1, spiRoutes: picoSpi,
  }),
  "waveshare_rp2040_touch_lcd_1.28": rpDescriptor({
    name: "waveshare_rp2040_touch_lcd_1.28", chip: "RP2040",
    features: rpFeatureMaps["waveshare_rp2040_touch_lcd_1.28"],
    exposedPins: pinsBetween(0, 28),
    aliases: pinAliases(pinsBetween(0, 28), {
      A0: 26, A1: 27, A2: 28, SDA: 6, SCL: 7, SCK: 10, MOSI: 11, MISO: 12,
    }),
    devices: { display: { type: "lcd", controller: "GC9A01A", width: 240, height: 240 } },
    i2cRoutes: [{ bus: 0, sda: 4, scl: 5 }, { bus: 1, sda: 6, scl: 7 }],
    i2cDefaultBus: 1,
    spiRoutes: [{ bus: 1, sck: 10, mosi: 11, miso: 12 }],
    spiDefaultBus: 1,
    gpioOutputPins: [...pinsBetween(0, 20), 22, 25, 26, 27, 28],
    pwmPins: [...pinsBetween(0, 5), 14, 15, 16, 17, 18, 19, 20, 26, 27, 28],
    neopixelPins: [...pinsBetween(0, 5), 14, 15, 16, 17, 18, 19, 20, 26, 27, 28],
  }),
  "waveshare_rp2350_lcd_1.47_a": rpDescriptor({
    name: "waveshare_rp2350_lcd_1.47_a", chip: "RP2350",
    features: rpFeatureMaps["waveshare_rp2350_lcd_1.47_a"],
    exposedPins: [...pinsBetween(0, 9), 14, 16, 17, 18, 19, 20, 21, 22],
    aliases: pinAliases([...pinsBetween(0, 9), 14, 16, 17, 18, 19, 20, 21, 22], {
      SDA: 4, SCL: 5, SCK: 18, MOSI: 19, MISO: 0, NEOPIXEL: 22,
    }),
    devices: {
      neopixel: { type: "neopixel", pin: 22, length: 1, order: "GRB" },
      display: { type: "lcd", controller: "ST7789V3", width: 172, height: 320 },
    },

    i2cRoutes: [{ bus: 0, sda: 4, scl: 5 }, { bus: 1, sda: 6, scl: 7 }],
    spiRoutes: [{ bus: 0, sck: 18, mosi: 19, miso: 0 }],
    pwmPins: [...pinsBetween(0, 9), 14, 22],
    neopixelPins: [...pinsBetween(0, 9), 14, 22],
  }),
  "waveshare_rp2350_touch_lcd_1.69": rpDescriptor({
    name: "waveshare_rp2350_touch_lcd_1.69", chip: "RP2350",
    features: rpFeatureMaps["waveshare_rp2350_touch_lcd_1.69"],
    exposedPins: [0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 24, 25, 26, 27, 28],
    aliases: pinAliases([0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 24, 25, 26, 27, 28], {
      A0: 26, A1: 27, A2: 28, SDA: 6, SCL: 7, SCK: 10, MOSI: 11, MISO: 12,
    }),
    devices: { display: { type: "lcd", controller: "ST7789V2", width: 240, height: 280 } },
    i2cRoutes: [{ bus: 0, sda: 4, scl: 5 }, { bus: 1, sda: 6, scl: 7 }],
    i2cDefaultBus: 1,
    spiRoutes: [{ bus: 1, sck: 10, mosi: 11, miso: 12 }],
    spiDefaultBus: 1,
    gpioOutputPins: [0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 22, 25, 26, 27, 28],
    pwmPins: [0, 1, 4, 5, 16, 17, 18, 19, 26, 27, 28],
    neopixelPins: [0, 1, 4, 5, 16, 17, 18, 19, 26, 27, 28],
  }),
  "waveshare_rp2350_touch_lcd_2.8": rpDescriptor({
    name: "waveshare_rp2350_touch_lcd_2.8", chip: "RP2350",
    features: rpFeatureMaps["waveshare_rp2350_touch_lcd_2.8"],
    exposedPins: [], aliases: {},
    devices: { display: { type: "lcd", controller: "ST7789T3", width: 240, height: 320 } },
  }),
  adafruit_feather_rp2040: rpDescriptor({
    name: "adafruit_feather_rp2040", chip: "RP2040",
    features: rpFeatureMaps.adafruit_feather_rp2040,
    exposedPins: [0, 1, 2, 3, 4, 8, 9, 10, 11, 12, 13, 16, 18, 19, 20, 24, 25, 26, 27, 28, 29],
    aliases: pinAliases([0, 1, 2, 3, 4, 8, 9, 10, 11, 12, 13, 16, 18, 19, 20, 24, 25, 26, 27, 28, 29], {
      A0: 26, A1: 27, A2: 28, A3: 29, SDA: 2, SCL: 3, SCK: 18, MOSI: 19, MISO: 20, LED: 13, NEOPIXEL: 16,
    }),
    devices: {
      led: { type: "gpio", pin: 13, activeLow: false },
      neopixel: { type: "neopixel", pin: 16, length: 1, order: "GRB" },
    },
    i2cRoutes: [{ bus: 0, sda: 24, scl: 25 }, { bus: 1, sda: 2, scl: 3 }],
    i2cDefaultBus: 1,
    spiRoutes: [{ bus: 0, sck: 18, mosi: 19, miso: 20 }, { bus: 1, sck: 10, mosi: 11, miso: 12 }],
    adcPins: [26, 27, 28, 29], adcAliases: { 26: "A0", 27: "A1", 28: "A2", 29: "A3" },
  }),
};

const xiaoPins = [1, 2, 3, 4, 5, 6, 7, 8, 9, 21];
const xiaoFeatures = featureMap(
  "moduleLoader", "console", "timers", "board", "gpio", "pwm", "i2c", "spi", "adc", "neopixel",
  "process", "require", "fs", "onboardLed", "onboardButton", "safeMode",
);
boardDescriptors.seeed_xiao_esp32s3 = {
  board: {
    name: "seeed_xiao_esp32s3", chip: "ESP32-S3", firmwareVersion,
    exposedPins: xiaoPins,
    pins: {
      D0: 1, D1: 2, D2: 3, D3: 4, D4: 5, D5: 6,
      D8: 7, D9: 8, D10: 9,
      A0: 1, A1: 2, A2: 3, A3: 4, A4: 5, A5: 6, A6: 7, A7: 8, A8: 9,
      SDA: 5, SCL: 6, SCK: 7, MISO: 8, MOSI: 9, LED: 21,
    },
    devices: {
      led: { type: "gpio", pin: 21, activeLow: true },
      button: { type: "managed", name: "BOOT", activeLow: true, readOnly: true },
    },
  },
  features: xiaoFeatures,
  modules: modulesFor(xiaoFeatures),
  capabilities: {
    gpio: gpioCapability(xiaoPins),
    pwm: {
      pins: pinsBetween(1, 9), maxOutputs: 8, timerCount: 4,
      duty: { min: 0, max: 1, unit: "ratio" },
      frequency: { minHz: 10, maxHz: 1000000, resolutionVaries: true },
    },
    adc: adcCapability(pinsBetween(1, 9), Object.fromEntries(pinsBetween(1, 9).map((pin) => [pin, `A${pin - 1}`])), {
      calibrated: true, rawChannel: false, vsys: false,
    }),
    boot: { safeMode: true },
    i2c: i2cCapability([{ bus: 0, sda: 5, scl: 6 }, { bus: 1, sda: 3, scl: 4 }], 0),
    spi: spiCapability([{ bus: 0, sck: 7, mosi: 9, miso: 8 }, { bus: 1, sck: 4, mosi: 6, miso: 5 }], 0, {
      dma: false, maxHz: 40000000, maxTransferBytes: 64,
    }),
    neopixel: neopixelCapability(pinsBetween(1, 9)),
    fs: { implementation: "fat", writable: true, hostTransfer: true },
    usb: usbCapability(espUsbClasses),
  },
};

for (const [boardId, descriptor] of Object.entries(boardDescriptors)) {
  const presentation = boardPresentation[boardId];
  if (!presentation) throw new Error(`Missing presentation metadata for MCU.js board: ${boardId}`);
  descriptor.presentation = Object.freeze({ ...presentation });
}
for (const boardId of Object.keys(boardPresentation)) {
  if (!boardDescriptors[boardId]) throw new Error(`Presentation metadata names unknown MCU.js board: ${boardId}`);
}

const shippingBoardIds = Object.freeze(Object.keys(boardDescriptors));

function manifestFor(boardId) {
  const descriptor = boardDescriptors[boardId];
  if (!descriptor) throw new Error(`Unknown MCU.js board: ${boardId}`);
  return structuredClone({
    formatVersion: 1,
    apiVersion: "0.2",
    board: descriptor.board,
    capabilities: descriptor.capabilities,
  });
}

module.exports = {
  boardDescriptors: Object.freeze(boardDescriptors),
  featureNames,
  shippingBoardIds,
  manifestFor,
};
