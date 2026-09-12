'use strict';
// Actual feature/metadata/renderer/Canvas JS over fake fs, native JPEG and a pixel backend.
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
function run(options={}) {
 const jpg=Buffer.from([255,216,255,192,0,11,8,0,128,0,128,1,1,17,0]);
 const bmp=Buffer.alloc(3126);bmp.write('BM');bmp.writeUInt32LE(3126,2);bmp.writeUInt32LE(54,10);bmp.writeUInt32LE(40,14);bmp.writeInt32LE(32,18);bmp.writeInt32LE(32,22);bmp.writeUInt16LE(1,26);bmp.writeUInt16LE(24,28);
 for(let y=0;y<32;y++)for(let x=0;x<32;x++)bmp.set(y<16?(x<16?[0,0,255]:[0,255,0]):(x<16?[255,0,0]:[255,255,255]),54+(31-y)*96+x*3);
 const data={'/app/bg.jpg':jpg,'/app/icon.bmp':bmp};const r={frames:[],events:[],opened:0,closed:0,decoderClosed:0};
 let now=0,id=0,fd=0,closed=true,lifecycle;const timers=new Map(),files=new Map(),cache={},pixels=new Uint32Array(128*128);
 const fileApi={statSync(p){return {size:data[p].length};},openSync(p){files.set(++fd,data[p]);return fd;},closeSync(f){assert.ok(files.has(f));files.delete(f);},readSync(f,b,o,n,pos){const d=files.get(f),count=Math.max(0,Math.min(n,d.length-pos));b.set(d.subarray(pos,pos+count),o);return count;}};
 const native={open(){assert.ok(closed);r.opened++;closed=false;return {width:128,height:128,maxTouchPoints:0,stopPointer(){},setLifecycle(fn){lifecycle=fn;},getState(){return closed?'closed':'open';},draw(c,mode,rgba){assert.ok(!closed);assert.equal(mode,'fill');assert.equal(c[0],4);assert.equal(c.length,5);const [,x,y,w,h]=c,color=(Math.round(rgba[0]*255)<<16)|(Math.round(rgba[1]*255)<<8)|Math.round(rgba[2]*255);
 for(let yy=Math.max(0,y);yy<Math.min(128,y+h);yy++)for(let xx=Math.max(0,x);xx<Math.min(128,x+w);xx++)pixels[yy*128+xx]=color;},present(){assert.ok(!closed);if(options.failPresent)throw Error('present failure');r.frames.push(pixels.slice());},close(){assert.ok(!closed);closed=true;r.closed++;if(lifecycle)lifecycle();}};}};
 const canvasModule={exports:{}};vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../lib/canvas.js'),'utf8'),{module:canvasModule,exports:canvasModule.exports,require(n){if(n==='events')return require('../lib/events');assert.equal(n,'mcujs:canvas-native');return native;}});
 const jpeg={open(){let block=0;return {width:128,height:128,read(b){if(options.failDecode)throw Error('decode failure');if(block===64)return null;for(let i=0;i<768;i+=3)b.set([128,0,128],i);const n=block++;return {x:n%8*16,y:Math.floor(n/8)*16,width:16,height:16};},close(){r.decoderClosed++;}};}};
 function load(name){if(cache[name])return cache[name].exports;const module={exports:{}};cache[name]=module;
 vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../examples/portable',name+'.js'),'utf8'),{module,Uint8Array,DataView,console:{log(...a){r.events.push(a.join(' '));}},setTimeout(fn,delay){timers.set(++id,{fn,at:now+delay});return id;},clearTimeout(n){timers.delete(n);},require(n){if(n==='fs')return fileApi;if(n==='jpeg')return jpeg;if(n==='devices')return {display:{open(){return canvasModule.exports.connect('default');}}};return load(path.posix.normalize(path.posix.join(path.posix.dirname(name),n)));}});return module.exports;}
 r.handle=load('images/features')('/app/bg.jpg','/app/icon.bmp');r.advance=end=>{let turns=0;while(true){const e=[...timers].sort((a,b)=>a[1].at-b[1].at)[0];if(!e||e[1].at>end)break;assert.ok(++turns<10000);timers.delete(e[0]);now=e[1].at;e[1].fn();}now=end;};r.filesLive=()=>files.size;r.pending=()=>timers.size;return r;
}
test('real modules produce independent expected pixels for corners, center, four edges and overlay order',()=>{
 const r=run();r.advance(20000);assert.equal(r.frames.length,4);assert.equal(r.opened,1);assert.equal(r.closed,0);
 const expect=(stage,samples)=>{for(const [x,y,color] of samples)assert.equal(r.frames[stage][y*128+x],color,`stage ${stage}, pixel ${x},${y}`);};
 expect(0,[[0,0,0xff0000],[31,0,0x00ff00],[0,31,0x0000ff],[31,31,0xffffff],[50,50,0x0000ff],[96,96,0xff0000],[127,127,0xffffff]]);
 expect(1,[[20,20,0xff0000],[79,48,0x00ff00],[48,79,0x0000ff],[79,79,0xffffff]]);
 expect(2,[[0,48,0x00ff00],[0,64,0xffffff],[127,48,0xff0000],[127,64,0x0000ff],[48,0,0x0000ff],[64,0,0xffffff],[48,127,0xff0000],[64,127,0x00ff00],[64,64,0x00ff00]]);
 expect(3,[[0,0,0x800080],[20,20,0xffffff],[80,20,0x000000],[0,64,0xffff00],[127,65,0xffff00],[24,96,0xff0000],[31,110,0xff0000]]);
 assert.equal(r.decoderClosed,1);assert.equal(r.filesLive(),0);assert.ok(r.events.includes('IMAGE_FEATURES_DONE'));r.handle.close();r.advance(300000);assert.equal(r.closed,1);assert.equal(r.pending(),0);
});
for(const options of [{failDecode:true},{failPresent:true}])test('real nested failure closes owned display once: '+JSON.stringify(options),()=>{const r=run(options);r.advance(300000);assert.equal(r.closed,1);assert.equal(r.filesLive(),0);assert.equal(r.pending(),0);assert.ok(r.events.some(x=>x.startsWith('IMAGE_FEATURE_ERROR ')));assert.ok(!r.events.includes('IMAGE_FEATURES_DONE'));});
test('real feature controller cancels the pending first BMP and owns all final cleanup',()=>{const r=run();r.handle.close();r.handle.close();r.advance(300000);assert.equal(r.opened,1);assert.equal(r.closed,1);assert.equal(r.filesLive(),0);assert.equal(r.pending(),0);assert.equal(r.frames.length,0);});
