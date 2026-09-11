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
function colorBitmap(bpp, topDown = false) {
  // Literal RGB565 masks/pixels, or BGRX bytes with deliberately varying X.
  // Top: red, green, green. Bottom: blue, white, a mid-tone.
  const top = bpp === 16 ? [0,248, 224,7, 224,7, 99,99] : [0,0,255,0, 0,255,0,1, 0,255,0,255];
  const bottom = bpp === 16 ? [31,0, 255,255, 16,132, 88,88] : [255,0,0,127, 255,255,255,0, 132,130,132,200];
  const offset = bpp === 16 ? 66 : 54, b = Buffer.alloc(offset + top.length * 2);
  b.write('BM'); b.writeUInt32LE(b.length,2); b.writeUInt32LE(offset,10);
  b.writeUInt32LE(40,14); b.writeInt32LE(3,18); b.writeInt32LE(topDown ? -2 : 2,22);
  b.writeUInt16LE(1,26); b.writeUInt16LE(bpp,28); b.writeUInt32LE(bpp === 16 ? 3 : 0,30);
  b.writeUInt32LE(top.length * 2,34);
  if (bpp === 16) b.set([0,248,0,0, 224,7,0,0, 31,0,0,0],54);
  b.set(topDown ? top : bottom,offset); b.set(topDown ? bottom : top,offset + top.length);
  return b;
}
for (const bpp of [16,32]) for (const topDown of [false,true]) {
  test(bpp+'-bit BMP renders exact colors, coalesces equal colors and honors row order '+topDown,()=>{
    const r=run(colorBitmap(bpp,topDown),{width:3,height:2,shortRead:3});
    assert.equal(r.error,undefined);
    assert.deepEqual(r.draws.filter(x=>x[0]!=='black'),[
      ['#ff0000',0,0,1,1],['#00ff00',1,0,2,1],
      ['#0000ff',0,1,1,1],['#ffffff',1,1,1,1],['#848284',2,1,1,1],
    ]);
    assert.equal(r.closed,1);assert.ok(r.events.some(e=>e.startsWith('BMP_READY ')));
    assert.ok(r.reads.every(x=>x.bufferBytes<=54));
  });
}
function run(data, options = {}) {
  const draws=[], reads=[], events=[], timers=new Map(); let closed=0, displays=0, pos=0, next=0;
  const inputPath=options.path||'/sd/picture.bmp';
  const context={fillStyle:'',fillRect(x,y,w,h){
    if(options.failDraw===draws.length) throw Error('draw failure');
    draws.push([this.fillStyle,x,y,w,h]);
  }};
  const fakeFs={
    openSync(p,flags){assert.equal(p,inputPath);assert.equal(flags,'r');return 1;},
    closeSync(fd){assert.equal(fd,1);closed++;},
    readSync(fd,buffer,offset,length,position){
      assert.equal(fd,1); assert.ok(buffer instanceof Uint8Array); reads.push({length,bufferBytes:buffer.byteLength});
      assert.ok(length<=2560); const at=position===undefined||position===null ? pos : position;
      const n=Math.max(0,Math.min(length, data.length-at, options.shortRead || length));
      buffer.set(data.subarray(at,at+n),offset);if(position===undefined||position===null)pos+=n; return n;
    }
  };
  const module={exports:{}};
  const sandbox={module,exports:module.exports,Uint8Array,DataView,console:{log(...x){events.push(x.join(' '));}},
    setTimeout(fn,delay){
      if(++next===options.failTimer || (delay===60000 && options.failDeadline)) throw Error('timer failure');
      timers.set(next,{fn,delay});return next;
    },clearTimeout(id){if(options.failClear)throw Error('cancel failure');timers.delete(id);},
    require(name) {
      if(name==='fs') return fakeFs;
      if(name==='devices') return {display:{open(){
        displays++;
        return {canvas:{width:options.width||2,height:options.height||2,getContext(){return context;}},present(){if(options.failPresent)throw Error('present failure');events.push('PRESENTED');},close(){events.push('CLOSED');}};
      }}};
      throw Error(name);
    }};
  const source=fs.readFileSync(path.join(__dirname,'../examples/portable/sd-bmp/show.js'),'utf8');
  vm.runInNewContext(source,sandbox); let error;
  try {
    const handle=module.exports(inputPath);
    events.push('STATUS:'+ (typeof handle.status === 'function' ? handle.status() : null));
    if(options.cancelImmediately) {handle.close();handle.close();}
    for(let i=0;i<650;i++) {
      const entry=[...timers].find(([,t])=>t.delay<60000);if(!entry)break;
      timers.delete(entry[0]);entry[1].fn();
    }
    events.push('RENDER_STATUS:'+ (typeof handle.status === 'function' ? handle.status() : null));
    if(options.expire) {
      const entry=[...timers].find(([,t])=>t.delay===60000);
      if(entry){timers.delete(entry[0]);entry[1].fn();}
      handle.close();
    }
    events.push('FINAL_STATUS:'+ (typeof handle.status === 'function' ? handle.status() : null));
  }catch(e){error=e;}
  return {draws,reads,events,closed,displays,error,timerCount:timers.size};
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

for(const bpp of [16,32]) {
  test(bpp+'-bit paths, rotation, bounded reads and cancellation',()=>{
    for(const filePath of ['/app/picture.bmp','/sd/picture.bmp']) {
      const r=run(colorBitmap(bpp),{width:2,height:3,path:filePath,expire:true});
      assert.equal(r.error,undefined);
      assert.deepEqual(r.draws.filter(x=>x[0]!=='black'),[
        ['#ff0000',1,0,1,1],['#00ff00',1,1,1,2],
        ['#0000ff',0,0,1,1],['#ffffff',0,1,1,1],['#848284',0,2,1,1],
      ]);
      assert.equal(r.closed,1);assert.equal(r.events.filter(e=>e==='CLOSED').length,1);
      assert.equal(r.timerCount,0);
    }
    const cancelled=run(colorBitmap(bpp),{width:3,cancelImmediately:true});
    assert.equal(cancelled.closed,1);assert.equal(cancelled.timerCount,0);
    assert.equal(cancelled.draws.length,1);assert.ok(!cancelled.events.some(e=>e.startsWith('BMP_READY ')));
    const offset=bpp===16?66:54, stride=bpp===16?1280:2560;
    const b=Buffer.alloc(offset+stride);colorBitmap(bpp).copy(b,0,0,offset);
    b.writeUInt32LE(b.length,2);b.writeInt32LE(640,18);b.writeInt32LE(1,22);
    const max=run(b,{width:640,height:1,expire:true});assert.equal(max.error,undefined);
    assert.ok(max.events.some(e=>e.startsWith('BMP_READY ')));
    assert.equal(Math.max(...max.reads.map(x=>x.bufferBytes)),stride);
    assert.equal(max.reads.reduce((n,x)=>n+x.length,0),offset+stride);
  });
  test(bpp+'-bit truncations fail without success and close all acquired resources',()=>{
    const b=colorBitmap(bpp);
    for(let end=0;end<b.length;end++) {
      const r=run(b.subarray(0,end),{width:3,shortRead:3});
      assert.equal(r.closed,1);assert.equal(r.timerCount,0);
      assert.ok(r.error||r.events.some(e=>e.startsWith('BMP_ERROR ')));
      assert.ok(!r.events.some(e=>e.startsWith('BMP_READY ')));
      assert.equal(r.events.filter(e=>e==='CLOSED').length,r.displays);
    }
  });
  test(bpp+'-bit drawing and timer failures close the file and display',()=>{
    for(const fault of [{failDraw:0},{failDraw:1},{failTimer:1},{failTimer:2},{failDeadline:true}]) {
      const r=run(colorBitmap(bpp),{width:3,...fault});
      assert.equal(r.closed,1);assert.equal(r.timerCount,0);
      assert.equal(r.events.filter(e=>e==='CLOSED').length,1);
      assert.ok(r.error||r.events.some(e=>e.startsWith('BMP_ERROR ')));
      assert.ok(!r.events.some(e=>e.startsWith('BMP_READY ')));
    }
  });
}
test('committed 16/32-bit image fixtures decode to independent sampled colors',()=>{
  // Samples decoded independently with Pillow, not with this renderer.
  const expected={16:[[255,0,0],[131,0,123],[131,4,123],[0,0,255]],
    32:[[254,0,2],[129,0,126],[130,4,127],[1,0,252]]};
  const points=[[0,0],[0,159],[85,159],[171,319]];
  for(const bits of [16,32]) {
    const b=fs.readFileSync(path.join(__dirname,'../examples/images/test_172x320_'+bits+'bit.bmp'));
    assert.equal(b.readUInt16LE(28),bits);assert.equal(b.readUInt32LE(14),40);
    const r=run(b,{width:172,height:320,expire:true});assert.equal(r.error,undefined);
    assert.ok(r.events.some(e=>e.startsWith('BMP_READY ')));assert.equal(r.closed,1);
    assert.equal(r.timerCount,0);assert.ok(r.events.includes('CLOSED'));
    for(let i=0;i<points.length;i++) {
      const [x,y]=points[i];
      const d=r.draws.find(d=>d[0]!=='black' && x>=d[1] && x<d[1]+d[3] && y>=d[2] && y<d[2]+d[4]);
      assert.ok(d);
      const rgb=[1,3,5].map(j=>parseInt(d[0].slice(j,j+2),16));
      // Bit replication and Pillow's normalized integer scaling can differ by 1.
      rgb.forEach((c,j)=>assert.ok(Math.abs(c-expected[bits][i][j])<=(bits===16?1:0)));
    }
  }
});

test('BMP rejects ambiguous RGB555/BI_RGB, unsupported masks and invalid bounds before display open',()=>{
  const bad=[];
  for(const change of [
    b=>b.writeUInt32LE(0,30),b=>b.writeUInt32LE(0x7c00,54),
    b=>b.writeUInt32LE(0x03e0,58),b=>b.writeUInt32LE(0xf800,62),
    b=>b.writeInt32LE(0,18),b=>b.writeInt32LE(641,18),
    b=>b.writeInt32LE(0,22),b=>b.writeInt32LE(-2147483648,22),
    b=>b.writeUInt32LE(65,10),b=>b.writeUInt32LE(81,2),
    b=>b.writeUInt32LE(124,14),b=>b.writeUInt16LE(2,26),
  ]) {const b=colorBitmap(16);change(b);bad.push(b);}
  const unsupported32=colorBitmap(32);unsupported32.writeUInt32LE(3,30);bad.push(unsupported32);
  for(const b of bad) {
    const r=run(b,{width:3});assert.ok(r.error);assert.equal(r.closed,1);
    assert.equal(r.displays,0);assert.equal(r.timerCount,0);
  }
});

test('BMP controller exposes ready, error and close without log parsing',()=>{
 const r=run(bitmap(),{expire:true});assert.ok(r.events.includes('STATUS:loading'));assert.ok(r.events.includes('RENDER_STATUS:ready'));assert.ok(r.events.includes('FINAL_STATUS:closed'));
 assert.ok(run(bitmap(),{failDraw:1}).events.includes('FINAL_STATUS:error'));assert.ok(run(bitmap(),{cancelImmediately:true}).events.includes('FINAL_STATUS:closed'));
});

test('BMP ready status follows successful presentation, not only pixel writes',()=>{
 const r=run(bitmap(),{expire:true});assert.ok(r.events.indexOf('PRESENTED')>=0);assert.ok(r.events.indexOf('PRESENTED')<r.events.indexOf('RENDER_STATUS:ready'));
 const failed=run(bitmap(),{failPresent:true});assert.ok(failed.events.includes('FINAL_STATUS:error'));assert.ok(!failed.events.some(e=>e.startsWith('BMP_READY ')));assert.ok(failed.events.includes('CLOSED'));assert.equal(failed.timerCount,0);
});

test('BMP cancellation failures still attempt file and display release',()=>{
 const r=run(bitmap(),{cancelImmediately:true,failClear:true});assert.ok(r.error);assert.equal(r.closed,1);assert.equal(r.events.filter(e=>e==='CLOSED').length,1);
});
