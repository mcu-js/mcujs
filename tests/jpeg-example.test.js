'use strict';
// Example scheduling/ownership oracle. The native JPEG decoder has separate real-byte tests.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

function run(options = {}) {
  const bytes = options.bytes || Uint8Array.from([255,216,255,1,2,3,4,5,6,7,255,217]);
  const blocks = options.blocks || [
    {x:0,y:0,width:3,height:1,data:[255,0,0,0,255,0,0,255,0]},
    {x:0,y:1,width:3,height:1,data:[0,0,255,255,255,255,132,130,132]},
  ];
  const r = {draws:[],reads:[],events:[],decoderReads:[],buffers:[],fileClosed:0,
    decoderOpened:0,decoderClosed:0,displayOpened:0,displayClosed:0,presented:0};
  const timers = new Map(); let next = 0, turn = 0, blockIndex = 0, now = 0;
  const filename = options.path || '/sd/picture.jpg';
  const canvasContext = {fillStyle:'', fillRect(...args) {
    if (options.failDraw === r.draws.length) throw Error('draw failure');
    r.draws.push([this.fillStyle,...args]);
  }};
  const reader = {width:options.imageWidth || 3,height:options.imageHeight || 2,
    read(target) {
      assert.equal(target.byteLength,768); r.decoderReads.push(turn);
      if (options.failDecode === blockIndex) throw Error('decode failure');
      const b = blocks[blockIndex++]; if (!b) return null;
      target.set(b.data); return {x:b.x,y:b.y,width:b.width,height:b.height};
    },
    close() {r.decoderClosed++; if (options.failDecoderClose) throw Error('decoder close failure');}
  };
  const nativeImage = {open(input) {
    r.decoderOpened++; assert.deepEqual(Array.from(input),Array.from(bytes));
    assert.equal(r.fileClosed,1,'file is closed before decoding');
    assert.equal(r.displayOpened,0,'decoding has no display lease');
    if (options.failOpenJPEG) throw Error('bad JPEG'); return reader;
  }};
  if (options.oldFirmware) delete nativeImage.open;
  const fakeFs = {
    statSync(p) {assert.equal(p,filename);return {size:options.size === undefined ? bytes.length : options.size};},
    openSync(p,mode) {assert.equal(p,filename);assert.equal(mode,'r');r.events.push('FILE_OPEN');return 1;},
    closeSync(fd) {assert.equal(fd,1);r.fileClosed++;if(options.failFileClose)throw Error('file close failure');},
    readSync(fd,target,offset,length,position) {
      assert.equal(fd,1); assert.ok(length <= 256);r.reads.push({turn,length,position});
      if (options.failRead === r.reads.length) throw Error('read failure');
      const n = Math.max(0,Math.min(length,bytes.length-position,options.shortRead || length));
      target.set(bytes.subarray(position,position+n),offset);return n;
    }
  };
  function ByteArray(size) {r.buffers.push(size);return new Uint8Array(size);}
  const module = {exports:{}};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../examples/portable/jpeg/show.js'),'utf8'),{
    module,Uint8Array:ByteArray,console:{log(...args){r.events.push(args.join(' '));}},
    setTimeout(fn,delay) {
      if (++next === options.failTimer || (delay > 0 && options.failDeadline)) throw Error('timer failure');
      timers.set(next,{fn,delay,at:now+delay}); return next;
    },clearTimeout(id){if(options.failClear)throw Error('cancel failure');timers.delete(id);},
    require(name) {
      if(name === 'fs')return fakeFs;
      if(name === 'jpeg')return nativeImage;
      if(name === 'devices')return {display:{open(){
        if(options.failDisplayOpen)throw Error('display busy'); r.displayOpened++;
        return {canvas:{width:options.width || 3,height:options.height || 2,getContext(){return canvasContext;}},
          present(){if(options.failPresent)throw Error('present failure');r.presented++;},
          close(){r.displayClosed++;if(options.failDisplayClose)throw Error('display close failure');}};
      }}};
      throw Error('unexpected require '+name);
    }
  });
  try {
    const handle = module.exports(filename);
    r.initialStatus = typeof handle.status === 'function' ? handle.status() : null;
    if(options.cancelImmediately){handle.close();handle.close();}
    for(turn=1;turn<=10000;turn++) {
      let entry = [...timers].find(([,t])=>t.delay === 0);if(!entry)break;
      const expired = [...timers].find(([,t])=>t.delay > 0 && t.at <= now);
      if(expired) entry=expired;
      timers.delete(entry[0]);entry[1].fn(); now += options.turnMs || 0;
      if(options.cancelAfterTurn === turn){
        const cancelled=[...timers.values()];handle.close();handle.close();
        if(options.runCancelledCallbacks)cancelled.forEach(t=>t.fn());
      }
      if(options.expireAfterTurn === turn){
        const entry=[...timers].find(([,t])=>t.delay>0);
        if(entry){timers.delete(entry[0]);entry[1].fn();}
      }
    }
    assert.ok(turn<=10000,'bounded completion');
    r.renderStatus = typeof handle.status === 'function' ? handle.status() : null;
    if(options.expire) {
      const entry=[...timers].find(([,t])=>t.delay>0);
      if(entry){timers.delete(entry[0]);entry[1].fn();}handle.close();
    }
    r.finalStatus = typeof handle.status === 'function' ? handle.status() : null;
  } catch(error) {r.error=error;}
  r.remainingTimers=[...timers.values()].map(t=>({delay:t.delay,at:t.at,now}));r.timerCount=timers.size;return r;
}

test('JPEG blocks reach Canvas as exact RGB runs without decoder-owned display access',()=>{
  const r=run({shortRead:3,expire:true});assert.equal(r.error,undefined);
  assert.deepEqual(r.draws,[['black',0,0,3,2],['#ff0000',0,0,1,1],['#00ff00',1,0,2,1],
    ['#0000ff',0,1,1,1],['#ffffff',1,1,1,1],['#848284',2,1,1,1]]);
  assert.equal(r.presented,1);assert.equal(r.fileClosed,1);assert.equal(r.decoderClosed,1);
  assert.equal(r.displayClosed,1);assert.equal(r.timerCount,0);
  assert.ok(r.events.some(e=>e.startsWith('JPEG_READY ')));
  assert.equal(new Set(r.reads.map(x=>x.turn)).size,r.reads.length,'one file read per turn');
  assert.equal(new Set(r.decoderReads).size,r.decoderReads.length,'one MCU read per turn');
});

test('JPEG paths, clockwise rotation and centering do not depend on the storage mount',()=>{
  for(const filePath of ['/app/picture.jpg','/sd/picture.jpg']){
    const r=run({path:filePath,width:2,height:3,expire:true});assert.equal(r.error,undefined);
    assert.deepEqual(r.draws,[['black',0,0,2,3],['#ff0000',1,0,1,1],['#00ff00',1,1,1,2],
      ['#0000ff',0,0,1,1],['#ffffff',0,1,1,1],['#848284',0,2,1,1]]);
    assert.equal(r.timerCount,0);assert.equal(r.displayClosed,1);
  }
  const centered=run({width:7,height:6,expire:true});
  assert.deepEqual(centered.draws[1],['#ff0000',2,2,1,1]);
});
test('non-origin and partial MCU blocks use image coordinates, not decode order',()=>{
  const r=run({imageWidth:4,imageHeight:3,width:4,height:3,expire:true,blocks:[
    {x:2,y:1,width:2,height:2,data:[255,0,0,0,255,0,0,0,255,255,255,255]},
    {x:0,y:0,width:1,height:1,data:[132,130,132]},
  ]});
  assert.equal(r.error,undefined);
  assert.deepEqual(r.draws,[['black',0,0,4,3],['#ff0000',2,1,1,1],['#00ff00',3,1,1,1],
    ['#0000ff',2,2,1,1],['#ffffff',3,2,1,1],['#848284',0,0,1,1]]);
});
test('maximum JPEG input is read once in bounded turns with one reusable RGB block',()=>{
  const r=run({bytes:new Uint8Array(16384),expire:true});assert.equal(r.error,undefined);
  assert.deepEqual(r.buffers,[16384,768]);assert.equal(r.reads.length,65);
  assert.ok(r.reads.slice(0,-1).every(x=>x.length===256));
  assert.deepEqual(r.reads.at(-1),{turn:65,length:1,position:16384});
  assert.equal(r.decoderOpened,1);assert.equal(r.fileClosed,1);
  assert.equal(new Set(r.reads.map(x=>x.turn)).size,r.reads.length);
});
test('oversize, empty and unsupported firmware fail before a file or display is opened',()=>{
  for(const options of [{size:0},{size:16385},{size:NaN},{size:1.5},{oldFirmware:true}]){
    const r=run(options);assert.ok(r.error);assert.equal(r.fileClosed,0);
    assert.equal(r.decoderOpened,0);assert.equal(r.displayOpened,0);assert.equal(r.timerCount,0);
  }
});
test('file growth and truncation are detected before decoder or display acquisition',()=>{
  for(const size of [11,13]){
    const r=run({size});assert.equal(r.error,undefined);assert.equal(r.fileClosed,1);
    assert.equal(r.decoderOpened,0);assert.equal(r.displayOpened,0);assert.equal(r.timerCount,0);
    assert.ok(r.events.some(e=>e.startsWith('JPEG_ERROR ')));
    assert.ok(!r.events.some(e=>e.startsWith('JPEG_READY ')));
  }
});
test('cancellation and the overall deadline close resources even during loading or decoding',()=>{
  for(const options of [{cancelImmediately:true},...[1,2,3].flatMap(turn=>[
    {cancelAfterTurn:turn,runCancelledCallbacks:true},{expireAfterTurn:turn}])]){
    const r=run(options);assert.equal(r.error,undefined);assert.equal(r.fileClosed,1);
    assert.equal(r.decoderClosed,r.decoderOpened);assert.equal(r.displayClosed,r.displayOpened);
    assert.equal(r.timerCount,0);assert.ok(!r.events.some(e=>e.startsWith('JPEG_READY ')));
  }
});
test('JPEG read, decode, drawing, presentation and scheduling failures release all acquired resources',()=>{
  const faults=[{failRead:1},{failRead:2},{failOpenJPEG:true},{failDisplayOpen:true},
    {width:1,height:1},{failDecode:0},{failDecode:1},{failDraw:0},{failDraw:1},
    {failPresent:true},{failFileClose:true},{failDecoderClose:true},
    {failDeadline:true},...[1,2,3,4,5].map(failTimer=>({failTimer}))];
  for(const options of faults){
    const r=run(options);assert.equal(r.fileClosed,1,JSON.stringify(options));
    assert.equal(r.decoderClosed,r.decoderOpened-(options.failOpenJPEG?1:0),JSON.stringify(options));
    assert.equal(r.displayClosed,r.displayOpened);assert.equal(r.timerCount,0);
    assert.ok(r.error||r.events.some(e=>e.startsWith('JPEG_ERROR ')),JSON.stringify(options));
    assert.ok(!r.events.some(e=>e.startsWith('JPEG_READY ')),JSON.stringify(options));
  }
});

test('slow bounded drawing can complete after 60 seconds without being cancelled',()=>{
  const r=run({turnMs:20000,expire:true});
  assert.equal(r.error,undefined);
  assert.ok(r.events.some(e=>e.startsWith('JPEG_READY ')));
  assert.ok(!r.events.some(e=>e.startsWith('JPEG_ERROR ')));
  assert.equal(r.displayClosed,1);assert.equal(r.timerCount,0);
});
test('an unfinished deadline reports failure rather than silently disappearing',()=>{
  const r=run({expireAfterTurn:3});
  assert.ok(r.events.some(e=>e==='JPEG_ERROR JPEG timed out'));
  assert.ok(!r.events.some(e=>e.startsWith('JPEG_READY ')));
  assert.equal(r.decoderClosed,1);assert.equal(r.displayClosed,1);assert.equal(r.timerCount,0);
});

test('JPEG controller exposes completion without log parsing',()=>{
 const r=run({expire:true});assert.equal(r.initialStatus,'loading');assert.equal(r.renderStatus,'ready');assert.equal(r.finalStatus,'closed');
 assert.equal(run({failDecode:0}).finalStatus,'error');assert.equal(run({cancelImmediately:true}).finalStatus,'closed');
});

test('ready JPEG owns a fresh hold deadline, even near the render deadline',()=>{
 const r=run({turnMs:29750});assert.equal(r.renderStatus,'ready');
 assert.ok(r.remainingTimers.some(t=>t.delay===60000&&t.at===179000));
});
test('JPEG cancellation failures still attempt file, decoder and display release',()=>{
 for(const options of [{cancelImmediately:true},{cancelAfterTurn:3}]) {
  const r=run({...options,failClear:true});assert.ok(r.error);assert.equal(r.fileClosed,1);assert.equal(r.decoderClosed,r.decoderOpened);assert.equal(r.displayClosed,r.displayOpened);
 }
});
