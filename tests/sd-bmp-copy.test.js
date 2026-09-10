'use strict';
const test=require('node:test'), assert=require('node:assert/strict'), fs=require('node:fs'), vm=require('node:vm'), path=require('node:path');
function run(options={}) {
  const input=Uint8Array.from({length:7001},(_,i)=>i%256), out=[], events=[], closed=[], queue=[];
  let at=0, peak=0, opened=0;
  const api={existsSync(){return !!options.exists;},openSync(p,flags){opened++;return flags==='r'?1:2;},closeSync(fd){closed.push(fd);},
    readSync(fd,b,o,n){assert.equal(fd,1);peak=Math.max(peak,b.length);n=Math.min(n,317,input.length-at);b.set(input.subarray(at,at+n),o);at+=n;return n;},
    writeSync(fd,b,o,n){assert.equal(fd,2);if(options.fail)throw Object.assign(new Error('card removed'),{code:'EIO'});n=Math.min(n,113);out.push(...b.subarray(o,o+n));return n;}};
  const m={exports:{}};vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../examples/portable/sd-bmp/copy.js'),'utf8'),{module:m,Uint8Array,require(n){assert.equal(n,'fs');return api;},console:{log(x){events.push(x);}},setTimeout(fn){queue.push(fn);return queue.length;},clearTimeout(){}});
  let error;try{m.exports('/app/source.bmp','/sd/image.bmp');for(let i=0;i<100 && queue.length;i++)queue.shift()();}catch(e){error=e;}
  return {input,out,peak,events,closed,opened,error};
}
test('bounded copy preserves NUL and every high byte despite short reads/writes',()=>{const r=run();assert.deepEqual(r.out,[...r.input]);assert.equal(r.peak,1024);assert.deepEqual(r.closed,[2,1]);assert.ok(r.events.includes('COPY_READY /sd/image.bmp bytes=7001 buffer=1024'));});
test('copy refuses an existing destination before opening either file',()=>{const r=run({exists:true});assert.ok(r.error);assert.equal(r.opened,0);});
test('failed copy closes both handles without a success message',()=>{const r=run({fail:true});assert.deepEqual(r.closed,[2,1]);assert.ok(r.events.some(x=>x.startsWith('COPY_ERROR EIO')));assert.ok(!r.events.some(x=>x.startsWith('COPY_READY')));});
