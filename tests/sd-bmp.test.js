'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

function bitmap(topDown = false) {
  // Independent 2x2 fixture: top red/green, bottom blue/white. Two padding bytes.
  const b = Buffer.alloc(70);
  b.write('BM'); b.writeUInt32LE(70, 2); b.writeUInt32LE(54, 10);
  b.writeUInt32LE(40, 14); b.writeInt32LE(2, 18); b.writeInt32LE(topDown ? -2 : 2, 22);
  b.writeUInt16LE(1, 26); b.writeUInt16LE(24, 28); b.writeUInt32LE(16, 34);
  const top = [0,0,255, 0,255,0, 99,99], bottom = [255,0,0, 255,255,255, 88,88];
  b.set(topDown ? top : bottom,54); b.set(topDown ? bottom : top,62); return b;
}
function run(data, options = {}) {
  const draws=[], reads=[], events=[], timers=[]; let closed=0, displays=0, pos=0;
  const context={fillStyle:'',fillRect(x,y,w,h){draws.push([this.fillStyle,x,y,w,h]);}};
  const fakeFs={
    openSync(p,flags){assert.equal(flags,'r');return 1;},
    closeSync(fd){assert.equal(fd,1);closed++;},
    readSync(fd,buffer,offset,length,position){
      assert.equal(fd,1); assert.ok(buffer instanceof Uint8Array); reads.push({length,bufferBytes:buffer.byteLength});
      assert.ok(length<=1920); const at=position===undefined||position===null ? pos : position;
      const n=Math.max(0,Math.min(length, data.length-at, options.shortRead || length));
      buffer.set(data.subarray(at,at+n),offset);if(position===undefined||position===null)pos+=n; return n;
    }
  };
  const module={exports:{}};
  const sandbox={module,exports:module.exports,Uint8Array,DataView,console:{log(...x){events.push(x.join(' '));}},
    setTimeout(fn,delay){timers.push({fn,delay});return timers.length;},clearTimeout(){},
    require(name) {
      if(name==='fs') return fakeFs;
      if(name==='devices') return {display:{open(){
        displays++;
        return {canvas:{width:options.width||2,height:options.height||2,getContext(){return context;}},close(){events.push('CLOSED');}};
      }}};
      throw Error(name);
    }};
  const source=fs.readFileSync(path.join(__dirname,'../examples/portable/sd-bmp/show.js'),'utf8');
  vm.runInNewContext(source,sandbox); let error;
  try {module.exports('/sd/picture.bmp');for(let i=0;i<30 && timers.some(t=>t.delay<60000);i++){const n=timers.findIndex(t=>t.delay<60000);timers.splice(n,1)[0].fn();}}catch(e){error=e;}
  return {draws,reads,events,closed,displays,error};
}
for(const topDown of [false,true]) test('bounded BMP decoding respects BGR, padding and '+(topDown?'top-down':'bottom-up')+' rows',()=>{
  const r=run(bitmap(topDown),{shortRead:3});assert.equal(r.error,undefined);
  assert.equal(r.closed,1);assert.ok(r.events.some(e=>e.startsWith('BMP_READY ')));
  assert.deepEqual(r.draws.filter(x=>x[0]!=='black'),[['#ff0000',0,0,1,1],['#00ff00',1,0,1,1],['#0000ff',0,1,1,1],['#ffffff',1,1,1,1]]);
  assert.ok(r.reads.every(x=>x.bufferBytes<=54));
});
test('malformed header closes before opening display',()=>{const b=bitmap();b.writeUInt32LE(1,30);const r=run(b);assert.ok(r.error);assert.equal(r.closed,1);assert.equal(r.displays,0);});
test('truncated pixel data never emits success and releases file/display',()=>{const r=run(bitmap().subarray(0,64));assert.equal(r.closed,1);assert.ok(!r.events.some(e=>e.startsWith('BMP_READY ')));assert.ok(r.error||r.events.some(e=>e.startsWith('BMP_ERROR ')));assert.ok(r.events.includes('CLOSED'));});

test('portrait BMP rotates clockwise into a landscape canvas without scaling',()=>{
  const b=Buffer.alloc(62);b.write('BM');b.writeUInt32LE(62,2);b.writeUInt32LE(54,10);b.writeUInt32LE(40,14);
  b.writeInt32LE(1,18);b.writeInt32LE(2,22);b.writeUInt16LE(1,26);b.writeUInt16LE(24,28);
  b.set([255,0,0,99,0,0,255,99],54);
  const r=run(b,{width:2,height:1});assert.equal(r.error,undefined);
  assert.deepEqual(r.draws.filter(x=>x[0]!=='black'),[['#ff0000',1,0,1,1],['#0000ff',0,0,1,1]]);
  assert.equal(r.closed,1);assert.ok(r.events.some(e=>e.startsWith('BMP_READY ')));
});
