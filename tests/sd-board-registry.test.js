const test = require('node:test');
const assert = require('node:assert/strict');
const {boardDescriptors, manifestFor, validateSdConfiguration, sdDefinitionsFor} = require('../runtime/board-registry');
const {generateSdConfigHeader, generateSdConfigCmake} = require('../scripts/generate-runtime-registry');
const {execFileSync} = require('node:child_process');
const {mkdtempSync, writeFileSync, rmSync, readFileSync} = require('node:fs');
const {tmpdir} = require('node:os');
const {join, resolve} = require('node:path');

test('actual board CMake configurations select SD sources and compiler gates', () => {
  const temp = mkdtempSync(join(tmpdir(), 'mcujs-sd-cmake-'));
  try {
    for (const id of Object.keys(boardDescriptors)) {
      const enabled = ['waveshare_rp2040_pizero', 'waveshare_rp2350_lcd_1.47_a',
        'waveshare_rp2350_touch_lcd_2.8', 'waveshare_esp32s3_epaper_1.54_v2', 'seeed_reterminal_sticky'].includes(id);
      const exported = enabled && id !== 'seeed_reterminal_sticky';
      writeFileSync(join(temp, 'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.13)\nproject(sd_config NONE)\ninclude("${resolve(__dirname, '../board', id, 'board_config.cmake')}")\nif(NOT MCUJS_HAS_SD STREQUAL "${enabled ? 'ON' : 'OFF'}")\nmessage(FATAL_ERROR "wrong SD source selection")\nendif()\nget_directory_property(defs COMPILE_DEFINITIONS)\nif(NOT "MCUJS_HAS_SD=${Number(enabled)}" IN_LIST defs OR NOT "MCUJS_USB_SD_MSC=${Number(exported)}" IN_LIST defs)\nmessage(FATAL_ERROR "missing SD compiler gates")\nendif()\n`);
      execFileSync('cmake', ['-S', temp, '-B', join(temp, id)], {stdio: 'pipe'});
    }
    assert.doesNotMatch(readFileSync(resolve(__dirname, '../board/waveshare_rp2040_pizero/board_config.h'), 'utf8'), /#define MCUJS_SPI0_/);
  } finally { rmSync(temp, {recursive: true, force: true}); }
});

test('generated SD config compiles all board policies and selects RP sources', () => {
  const header = generateSdConfigHeader();
  const expected = {
    waveshare_rp2040_pizero: [1, 0, 18, 19, 20, 21, 5000000, 0, 1],
    'waveshare_rp2350_lcd_1.47_a': [1, 1, 10, 11, 12, 15, 10000000, 0, 1],
    'waveshare_rp2350_touch_lcd_2.8': [1, -1, 19, 20, 21, 24, 10000000, 0, 1],
    'waveshare_esp32s3_epaper_1.54_v2': [1, -1, -1, -1, -1, -1, 4000000, 0, 1],
    seeed_reterminal_sticky: [1, 2, 13, 14, 12, 8, 4000000, 1, 0],
  };
  const keys = ['MCUJS_HAS_SD', 'MCUJS_SD_SPI_BUS', 'MCUJS_SD_SCK_PIN', 'MCUJS_SD_MOSI_PIN',
    'MCUJS_SD_MISO_PIN', 'MCUJS_SD_CS_PIN', 'MCUJS_SD_BAUD_HZ', 'MCUJS_SD_READONLY', 'MCUJS_USB_SD_MSC'];
  for (const id of Object.keys(boardDescriptors)) {
    const values = expected[id] ?? [0, -1, -1, -1, -1, -1, 0, 1, 0];
    const macro = `MCUJS_BOARD_${id.toUpperCase().replaceAll('.', '_')}`;
    const assertions = keys.map((key, i) => `_Static_assert(${key} == ${values[i]}, "${id} ${key}");`).join('\n');
    execFileSync('cc', ['-std=c11', '-Werror', '-x', 'c', '-fsyntax-only', '-'], {
      input: `#define ${macro} 1\n${header}\n${assertions}\n`,
    });
  }
  assert.match(header, /#define MCUJS_SD_SDMMC_CLK_PIN 39/);
  assert.match(header, /#define MCUJS_SD_SDMMC_CMD_PIN 41/);
  assert.match(header, /#define MCUJS_SD_SDMMC_D0_PIN 40/);
  assert.match(header, /#define MCUJS_SD_SHARED_CS_PIN 15/);
  const cmake = generateSdConfigCmake();
  for (const id of Object.keys(expected).filter(id => id.includes('_rp'))) {
    assert.match(cmake, new RegExp(`MCUJS_BOARD_NAME STREQUAL "${id.replaceAll('.', '\\.')}"[\\s\\S]*?set\\(MCUJS_HAS_SD ON\\)`));
  }
});

test('selected transport/policy derives SD capabilities without live state', () => {
  const enabled = ['waveshare_rp2040_pizero', 'waveshare_rp2350_lcd_1.47_a',
    'waveshare_rp2350_touch_lcd_2.8', 'waveshare_esp32s3_epaper_1.54_v2', 'seeed_reterminal_sticky'];
  for (const [id, descriptor] of Object.entries(boardDescriptors)) {
    const sd = manifestFor(id).capabilities.fs.sd;
    assert.equal(Boolean(sd), enabled.includes(id), id);
    if (!sd) continue;
    assert.deepEqual(sd, {root: '/sd', implementation: 'fat', writable: id !== 'seeed_reterminal_sticky',
      hostTransfer: id !== 'seeed_reterminal_sticky', removable: true, formats: ['fat16', 'fat32']});
    assert.equal(Object.hasOwn(sd, 'mounted'), false);
    assert.equal(Object.hasOwn(sd, 'owner'), false);
    assert.equal(Object.hasOwn(sd, 'ready'), false);
    assert.equal(Object.hasOwn(descriptor.features, 'sd'), false);
  }
});

test('configuration rejects unsupported mappings, missing wiring, export and live-state claims', () => {
  const reject = (id, change, message) => {
    const descriptor = structuredClone(boardDescriptors[id]);
    change(descriptor);
    assert.throws(() => validateSdConfiguration(descriptor), message);
  };
  for (const descriptor of Object.values(boardDescriptors)) assert.doesNotThrow(() => validateSdConfiguration(descriptor));
  reject('pico', d => { d.policy.sd.transport = 'spi'; }, /no onboard SD/);
  reject('waveshare_rp2040_pizero', d => { delete d.hardware.sd.spi.cs; }, /wiring/);
  reject('waveshare_rp2040_pizero', d => { d.hardware.sd.spi.bus = 1; }, /SPI mapping/);
  reject('waveshare_rp2350_touch_lcd_2.8', d => { d.policy.sd.transport = 'spi'; }, /SPI mapping/);
  reject('waveshare_esp32s3_epaper_1.54_v2', d => { d.policy.sd.transport = 'spi'; }, /wiring/);
  reject('waveshare_esp32s3_epaper_1.54_v2', d => { delete d.hardware.sd.sdmmc.cmd; }, /wiring/);
  reject('seeed_reterminal_sticky', d => { d.policy.sd.usbMsc = true; }, /USB MSC/);
  reject('waveshare_rp2040_pizero', d => { d.policy.sd.transport = 'magic'; }, /transport/);
  reject('waveshare_rp2040_pizero', d => { d.policy.sd.baudHz = 0; }, /baud/);
  reject('pico', d => { d.policy.sd.usbMsc = true; }, /disabled/);
  reject('waveshare_rp2040_pizero', d => { d.capabilities.fs.sd.ready = true; }, /capability/);
  reject('waveshare_rp2040_pizero', d => { d.capabilities.fs.sd.writable = false; }, /capability/);
});

test('private socket pins and owned SPI controllers cannot leak through public routes or aliases', () => {
  const id = 'waveshare_rp2040_pizero';
  const base = boardDescriptors[id];
  assert.deepEqual(base.capabilities.spi.defaultRoute, {bus: 1, sck: 10, mosi: 11, miso: 12});
  for (const change of [
    d => d.board.exposedPins.push(18),
    d => { d.board.pins.SCK = 18; },
    d => d.capabilities.gpio.pins.push(18),
    d => d.capabilities.gpio.outputPins.push(19),
    d => d.capabilities.pwm.pins.push(20),
    d => d.capabilities.neopixel.pins.push(21),
    d => d.capabilities.spi.routes.push({bus: 0, sck: 2, mosi: 3, miso: 0}),
    d => { d.capabilities.spi.defaultRoute = {bus: 1, sck: 18, mosi: 11, miso: 12}; },
  ]) {
    const descriptor = structuredClone(base);
    change(descriptor);
    assert.throws(() => validateSdConfiguration(descriptor), /reserved/);
  }
  assert.equal(boardDescriptors['waveshare_rp2350_lcd_1.47_a'].board.pins.D14, undefined);
});

test('all 13 boards explicitly account for onboard SD hardware independently of firmware policy', () => {
  assert.equal(Object.keys(boardDescriptors).length, 13);
  const equipped = ['waveshare_rp2040_pizero', 'waveshare_rp2350_lcd_1.47_a',
    'waveshare_rp2350_touch_lcd_2.8', 'waveshare_esp32s3_epaper_1.54_v2', 'seeed_reterminal_sticky'];
  for (const [id, descriptor] of Object.entries(boardDescriptors)) {
    assert.equal(descriptor.hardware?.sd?.present, equipped.includes(id), id);
    assert.equal(typeof descriptor.policy?.sd?.transport, 'string', id);
  }
  // Literal wiring from manufacturer schematics, not the old disabled headers.
  assert.deepEqual(boardDescriptors.waveshare_rp2040_pizero.hardware.sd.spi,
    {bus: 0, sck: 18, mosi: 19, miso: 20, cs: 21});
  assert.deepEqual(boardDescriptors['waveshare_rp2350_lcd_1.47_a'].hardware.sd.spi,
    {bus: 1, sck: 10, mosi: 11, miso: 12, cs: 15});
  assert.deepEqual(boardDescriptors.seeed_reterminal_sticky.hardware.sd.spi,
    {bus: 2, sck: 13, mosi: 14, miso: 12, cs: 8});
  assert.equal(boardDescriptors.seeed_reterminal_sticky.hardware.sd.cardDetectPin, 11);
  assert.deepEqual(boardDescriptors['waveshare_rp2350_touch_lcd_2.8'].hardware.sd.spi,
    {bus: -1, sck: 19, mosi: 20, miso: 21, cs: 24});
  assert.equal(boardDescriptors['waveshare_esp32s3_epaper_1.54_v2'].hardware.sd.spi, null);
});
