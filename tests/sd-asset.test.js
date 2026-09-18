const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const registry = require('../runtime/board-registry');
test('every equipped board declares its selected SD policy and protects socket pins', () => {
  const equipped = new Set(['waveshare_rp2350_lcd_1.47_a', 'waveshare_rp2350_touch_lcd_2.8',
    'waveshare_rp2040_pizero', 'waveshare_esp32s3_epaper_1.54_v2', 'seeed_reterminal_sticky']);
  const rpPins = {
    'waveshare_rp2350_lcd_1.47_a': [10, 11, 12, 15],
    'waveshare_rp2350_touch_lcd_2.8': [19, 20, 21, 22, 23, 24],
    waveshare_rp2040_pizero: [18, 19, 20, 21],
  };
  for (const [name, d] of Object.entries(registry.boardDescriptors)) {
    assert.equal(Boolean(d.capabilities.fs.sd), equipped.has(name), name);
    if (!equipped.has(name)) continue;
    assert.equal(d.capabilities.fs.sd.root, '/sd');
    assert.equal(d.capabilities.fs.sd.hostTransfer, name !== 'seeed_reterminal_sticky');
    assert.equal(d.capabilities.fs.sd.writable, name !== 'seeed_reterminal_sticky');
    if (name === 'waveshare_rp2350_lcd_1.47_a') assert.deepEqual(d.capabilities.spi.buses, [0]);
    if (name === 'waveshare_rp2040_pizero') assert.deepEqual(d.capabilities.spi.buses, [1]);
    for (const cap of ['gpio', 'pwm', 'neopixel']) {
      for (const pin of rpPins[name] || []) assert.ok(!(d.capabilities[cap]?.pins || []).includes(pin), `${name}: ${cap} pin ${pin}`);
    }
  }
});
test('SD asset renderer uses loaded pixels, rejects corrupt data and closes after 60s', () => {
  const source=fs.readFileSync(path.join(__dirname,'../examples/portable/sd-asset/show.js'),'utf8');
  let data=JSON.stringify({width:2,height:2,palette:['black','aqua'],pixels:'0110'});
  let opened=0,closed=0,frames=0,finish; const rectangles=[];
  const ctx={fillStyle:'',fillRect(...r){rectangles.push([this.fillStyle,...r]);}};
  const display={canvas:{width:100,height:120,getContext:()=>ctx},present(){frames++},close(){closed++}};
  const sandbox={module:{exports:{}},console:{log(){}},require(n){return n==='fs'?{readFileSync(p){assert.equal(p,'/sd/proof.json');return data;}}:{display:{open(){opened++;return display;}}}},setTimeout(f,ms){assert.equal(ms,60000);finish=f;}};
  vm.runInNewContext(source,sandbox);
  sandbox.module.exports('/sd/proof.json');
  assert.equal(opened,1);assert.equal(frames,1);
  assert.deepEqual(rectangles.slice(1).map(x=>x[0]),['black','aqua','aqua','black']);
  finish();assert.equal(closed,1);
  data=JSON.stringify({width:2,height:2,palette:['black'],pixels:'01'});
  assert.throws(()=>sandbox.module.exports('/sd/proof.json'));
  assert.equal(opened,1);
});
