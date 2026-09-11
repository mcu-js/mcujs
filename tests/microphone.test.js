'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
function fixture(supported=true){
 const timers=new Map(),cache={};let id=0,owned=false,status=0,starts=0,stops=0,failTimer=false;
 const pcm=new Uint8Array([0,0,255,127,0,128,1,0]);
 const native={open(){if(owned)throw Object.assign(Error(),{code:'EBUSY'});owned=true;return 1;},state(){return owned?'busy':'idle';},start(t,d){assert.equal(d,1000);starts++;status=1;},poll(){return status;},result(){assert.equal(status,2);return pcm;},stop(){stops++;status=0;},close(){owned=false;status=0;}};
 const context=vm.createContext({Promise,Uint8Array,setTimeout(f){if(failTimer)throw Error('timers full');timers.set(++id,f);return id;},clearTimeout(id){timers.delete(id);}});
 function load(n){if(n==='board')return {capability(){return supported?{microphone:{interface:'pcm'}}:{}}};if(n==='mcujs:microphone-native')return native;
 if(!cache[n]){const m={};vm.runInContext('(function(require,module){'+fs.readFileSync(path.join(__dirname,'../lib',n==='mcujs:microphone'?'microphone.js':n+'.js'),'utf8')+'\n})',context)(load,m);cache[n]=m.exports;}return cache[n];}
 return {load,native,pcm,timers,get starts(){return starts;},get stops(){return stops;},set failTimer(v){failTimer=v;},tick(s){status=s;for(const [i,f] of [...timers]){timers.delete(i);f();}}};
}
test('microphone discovery/open is silent; explicit bounded record returns native PCM and can repeat',async()=>{
 const f=fixture(),d=f.load('devices').microphone;assert.ok(d,'microphone missing');assert.equal(f.starts,0);const h=d.open();assert.equal(f.starts,0);assert.throws(()=>d.open(),{code:'EBUSY'});
 let done=false,p=h.record({duration:1000}).then(v=>{done=true;return v;});assert.equal(f.starts,1);f.tick(1);await Promise.resolve();assert.equal(done,false);
 await assert.rejects(h.record({duration:1000}),{code:'EBUSY'});f.tick(2);assert.strictEqual(await p,f.pcm);assert.equal(f.timers.size,0);
 p=h.record({duration:1000});f.tick(2);await p;assert.equal(f.starts,2);h.close();assert.equal(d.state,'idle');assert.equal(fixture(false).load('devices').microphone,undefined);
});
test('microphone abort, stop and close reject once, clear service timers and permit fresh ownership',async()=>{
 const f=fixture(),d=f.load('devices').microphone,events=f.load('events');let h=d.open();
 const c=new events.AbortController();c.signal.addEventListener('abort',e=>e.stopImmediatePropagation());
 let p=h.record({duration:1000,signal:c.signal});const stale=[...f.timers.values()][0];c.abort();await assert.rejects(p,{name:'AbortError'});assert.equal(f.timers.size,0);stale();assert.equal(h.state,'open');
 p=h.record({duration:1000});h.stop();h.stop();await assert.rejects(p,{code:'ABORT_ERR'});assert.equal(f.timers.size,0);
 p=h.record({duration:1000});h.close();h.close();await assert.rejects(p,{code:'ABORT_ERR'});await assert.rejects(h.record({duration:1000}),{code:'ENXIO'});assert.equal(d.state,'idle');
 h=d.open();p=h.record({duration:1000});f.tick(2);await p;h.close();assert.equal(f.timers.size,0);
});
test('microphone validation, already-aborted signals and timer exhaustion never start hardware',async()=>{
 const f=fixture(),h=f.load('devices').microphone.open(),events=f.load('events');
 for(const v of [-1,0,19,1001,20.5,NaN,Infinity,'1000'])await assert.rejects(h.record({duration:v}));

 for(const o of [null,4,{queue:true},{signal:{aborted:false}}])await assert.rejects(h.record(o));
 const c=new events.AbortController();c.abort();await assert.rejects(h.record({duration:1000,signal:c.signal}));
 f.failTimer=true;await assert.rejects(h.record({duration:1000}),{code:'ERR_RESOURCE_EXHAUSTED'});assert.equal(f.starts,0);assert.equal(f.timers.size,0);h.close();
});
test('microphone native read/read failure errors release operation and getter reentrancy cannot resurrect handles',async()=>{
 const f=fixture(),d=f.load('devices').microphone;let h=d.open();
 f.native.start=()=>{throw Object.assign(Error('capture init'),{code:'EINVAL'});};await assert.rejects(h.record({duration:1000}),{code:'EINVAL'});assert.equal(h.state,'open');assert.equal(f.timers.size,0);
 f.native.start=()=>{};let p=h.record({duration:1000});f.native.poll=()=>{throw Object.assign(Error('read failure'),{code:'EIO'});};f.tick(1);await assert.rejects(p,{code:'EIO'});assert.equal(f.timers.size,0);h.close();
 h=d.open();await assert.rejects(h.record({get duration(){h.close();return 1000;}}),{code:'ENXIO'});assert.equal(d.state,'idle');
 assert.equal(fixture(false).load('devices').microphone,undefined);
});

test('microphone cleanup errors still settle the operation and release JS timers',async()=>{
 const f=fixture(),h=f.load('devices').microphone.open();let p=h.record({duration:1000});
 f.native.stop=()=>{throw Object.assign(Error('rail shutdown failed'),{code:'EIO'});};
 f.tick(2);await assert.rejects(p,{code:'EIO'});assert.equal(f.timers.size,0);assert.equal(h.state,'open');
 f.native.stop=()=>{};h.close();
});

test('only the ePaper V2 manifest advertises the fixed bounded capture contract',()=>{
 const {manifestFor,boardDescriptors}=require('../runtime/board-registry');
 const target='waveshare_esp32s3_epaper_1.54_v2';
 assert.deepEqual(manifestFor(target).capabilities.devices?.microphone,{
  interface:'pcm',encoding:'pcm-s16le',channels:1,sampleRateHz:16000,
  maxOpenHandles:1,maxConcurrentRecordings:1,maxBufferBytes:32000,
  duration:{minMs:20,maxMs:1000},shutdown:'native-rail-off'
 });
 for(const board of Object.keys(boardDescriptors))if(board!==target)assert.equal(manifestFor(board).capabilities.devices?.microphone,undefined);
});
