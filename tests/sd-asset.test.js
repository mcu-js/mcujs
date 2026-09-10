const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const registry = require('../runtime/board-registry');
test('only 1.47 advertises SD and its bus/pins stay outside application routes', () => {
  for (const [name, d] of Object.entries(registry.boardDescriptors)) {
    assert.equal(Boolean(d.capabilities.fs.sd), name === 'waveshare_rp2350_lcd_1.47_a');
    if (!d.capabilities.fs.sd) continue;
    assert.equal(d.capabilities.fs.sd.root, '/sd');
    assert.equal(d.capabilities.fs.sd.hostTransfer, false);
    assert.deepEqual(d.capabilities.spi.buses, [0]);
    for (const cap of ['gpio', 'pwm', 'neopixel']) {
      for (const pin of [10, 11, 12, 15]) assert.ok(!d.capabilities[cap].pins.includes(pin));
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
