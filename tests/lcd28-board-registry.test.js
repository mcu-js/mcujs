const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const { join } = require("node:path");
const test = require("node:test");
const { boardDescriptors } = require("../runtime/board-registry.js");
const id = "waveshare_rp2350_touch_lcd_2.8";

test("LCD 2.8 first port advertises only qualified core runtime and USB storage", () => {
  const descriptor = boardDescriptors[id];
  assert.ok(descriptor, "LCD 2.8 board must be registered");
  assert.equal(descriptor.board.chip, "RP2350");
  assert.deepEqual(descriptor.board.exposedPins, []);
  assert.deepEqual(descriptor.board.pins, {});
  assert.deepEqual(descriptor.board.devices, {
    display: { type: "lcd", controller: "ST7789T3", width: 240, height: 320 },
  });
  assert.deepEqual(Object.entries(descriptor.features).filter(([, enabled]) => enabled).map(([name]) => name),
    ["moduleLoader", "console", "timers", "board", "process", "require", "fs"]);
  assert.deepEqual(descriptor.modules, ["board", "fs", "process", "events", "devices", "mcujs:module", "node:module"]);
  assert.deepEqual(descriptor.capabilities, {
    fs: { appRoot: "/app", binary: { buffer: "Uint8Array", maxOpenFiles: 4, maxTransferBytes: 4096, maxPosition: 2147483647, flags: ["r", "w"] }, implementation: "fat", writable: true, hostTransfer: true },
    usb: { classes: ["cdc", "msc"] },
  });
});

test("LCD 2.8 native board configuration matches the vendor SPI1 panel wiring", () => {
  const config = readFileSync(join(__dirname, "..", "board", id, "board_config.h"), "utf8");
  for (const [name, value] of Object.entries({ SPI_BUS: 1, SCK_PIN: 10, MOSI_PIN: 11, CS_PIN: 13, DC_PIN: 14, RST_PIN: 15, BL_PIN: 16, WIDTH: 240, HEIGHT: 320, X_OFFSET: 0, Y_OFFSET: 0, SPI_MODE: 3 })) {
    assert.match(config, new RegExp(`#define\\s+MCUJS_LCD_${name}\\s+${value}(?:\\s|$)`));
  }
  assert.doesNotMatch(config, /#define\s+MCUJS_(?:LCD_MISO_PIN|I2C\d_|SPI\d_)/);
  assert.match(config, /#define\s+MCUJS_LED_PIN\s+255/);
  assert.match(config, /#define\s+MCUJS_NEOPIXEL_PIN\s+255/);
  assert.match(config, /#define\s+MCUJS_FLASH_SIZE\s+FLASH_SIZE_16MB/);
  assert.match(config, /#define\s+MCUJS_RAM_SIZE\s+\(520 \* 1024\)/);
  const cmake = readFileSync(join(__dirname, "..", "board", id, "board_config.cmake"), "utf8");
  assert.match(cmake, /set\(PICO_BOARD pico2 CACHE STRING/);
  assert.match(cmake, /set\(PICO_PLATFORM rp2350-arm-s CACHE STRING/);
  assert.match(cmake, /MCUJS_BOARD_WAVESHARE_RP2350_TOUCH_LCD_2_8=1/);
  assert.match(cmake, /set\(MCUJS_FLASH_SIZE 16777216\)/);
});
