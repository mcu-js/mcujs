const test = require('node:test');
const assert = require('node:assert/strict');
const {readFileSync} = require('node:fs');
const {join} = require('node:path');
const {createRequire} = require('node:module');
const {boardDescriptors, sdDefinitionsFor} = require('../runtime/board-registry');
const Ajv = createRequire(join(__dirname, '../docs/package.json'))('ajv/dist/2020').default;
const schema = require('../docs/static/schemas/mcujs-portable-api-0.2.schema.json');
const target = 'waveshare_rp2350_lcd_1.47_a';

test('SD hostTransfer matches the explicitly enabled RP2 build, not just an SD slot', () => {
  for (const [name, d] of Object.entries(boardDescriptors)) {
    const config = readFileSync(join(__dirname, '../board', name, 'board_config.cmake'), 'utf8');
    assert.match(config, /generated\/sd_config.cmake/);
    const enabled = sdDefinitionsFor(d).MCUJS_USB_SD_MSC === 1;
    assert.equal(enabled, ['waveshare_rp2040_pizero', target, 'waveshare_rp2350_touch_lcd_2.8', 'waveshare_esp32s3_epaper_1.54_v2'].includes(name), name);
    assert.equal(d.capabilities.fs.sd?.hostTransfer === true, enabled, name);
    const manifest = JSON.parse(readFileSync(join(__dirname, '../runtime/manifests', `${name}.json`)));
    assert.deepEqual(manifest.capabilities.fs, d.capabilities.fs, name);
  }
  const sticky = boardDescriptors.seeed_reterminal_sticky.capabilities.fs;
  assert.equal(sticky.sd.writable, false);
  assert.equal(sticky.sd.hostTransfer, false);
  assert.equal(sticky.hostTransfer, false);
});

test('SD capability schema accepts true/false support, never a live status string', () => {
  const validate = new Ajv({strict: false}).compile(schema.$defs.fsCapability);
  for (const hostTransfer of [true, false]) {
    const fs = structuredClone(boardDescriptors[target].capabilities.fs);
    fs.sd.hostTransfer = hostTransfer;
    assert.ok(validate(fs), JSON.stringify(validate.errors));
  }
  const fs = structuredClone(boardDescriptors[target].capabilities.fs);
  fs.sd.hostTransfer = 'mounted';
  assert.equal(validate(fs), false);
});
