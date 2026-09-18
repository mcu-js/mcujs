const test = require('node:test');
const assert = require('node:assert/strict');
const {boardDescriptors, manifestFor, validateSdConfiguration, sdDefinitionsFor} = require('../runtime/board-registry');

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
