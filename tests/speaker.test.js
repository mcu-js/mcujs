'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
function fixture(supported=true){
 const timers=new Map(),cache={};let id=0,owned=false,status=0,starts=0,failTimer=false;
 const native={open(){if(owned)throw Object.assign(Error(),{code:'EBUSY'});owned=true;return 1;},state(){return owned?'busy':'idle';},start(t,p,v){assert.equal(p,'/app/chime.wav');assert.equal(v,0.25);starts++;status=1;},poll(){return status;},stop(){status=0;},close(){owned=false;status=0;}};
 const context=vm.createContext({Promise,setTimeout(f){if(failTimer)throw Error('timers full');timers.set(++id,f);return id;},clearTimeout(id){timers.delete(id);}});
 function load(n){if(n==='board')return {capability(){return supported?{speaker:{interface:'wav'}}:{}}};if(n==='mcujs:speaker-native')return native;
 if(!cache[n]){const m={};vm.runInContext('(function(require,module){'+fs.readFileSync(path.join(__dirname,'../lib',n==='mcujs:speaker'?'speaker.js':n+'.js'),'utf8')+'\n})',context)(load,m);cache[n]=m.exports;}return cache[n];}
 return {load,native,timers,get starts(){return starts;},set failTimer(v){failTimer=v;},tick(s){status=s;for(const [i,f] of [...timers]){timers.delete(i);f();}}};
}
test('discovered speaker waits for native drain, rejects concurrent play and repeats',async()=>{
 const f=fixture(),d=f.load('devices').speaker;assert.ok(d,'speaker missing');const h=d.open();assert.throws(()=>d.open(),{code:'EBUSY'});
 let done=false,p=h.play('/app/chime.wav').then(()=>done=true);f.tick(1);await Promise.resolve();assert.equal(done,false);await assert.rejects(h.play('/app/chime.wav'),{code:'EBUSY'});f.tick(2);await p;assert.equal(f.timers.size,0);
 p=h.play('/app/chime.wav');f.tick(2);await p;assert.equal(f.starts,2);h.close();assert.equal(d.state,'idle');
});
test('speaker abort, stop and close reject once, clear service timers and permit fresh ownership',async()=>{
 const f=fixture(),d=f.load('devices').speaker,events=f.load('events');let h=d.open();
 const c=new events.AbortController();c.signal.addEventListener('abort',e=>e.stopImmediatePropagation());
 let p=h.play('/app/chime.wav',{signal:c.signal});const stale=[...f.timers.values()][0];c.abort();await assert.rejects(p,{name:'AbortError'});assert.equal(f.timers.size,0);stale();assert.equal(h.state,'open');
 p=h.play('/app/chime.wav');h.stop();h.stop();await assert.rejects(p,{code:'ABORT_ERR'});assert.equal(f.timers.size,0);
 p=h.play('/app/chime.wav');h.close();h.close();await assert.rejects(p,{code:'ABORT_ERR'});await assert.rejects(h.play('/app/chime.wav'),{code:'ENXIO'});assert.equal(d.state,'idle');
 h=d.open();p=h.play('/app/chime.wav');f.tick(2);await p;h.close();assert.equal(f.timers.size,0);
});
test('speaker validation, already-aborted signals and timer exhaustion never start hardware',async()=>{
 const f=fixture(),h=f.load('devices').speaker.open(),events=f.load('events');
 for(const v of [-1,2,NaN,Infinity,'0.25'])await assert.rejects(h.play('/app/chime.wav',{volume:v}));
 for(const p of ['',null,5,'a\0b'])await assert.rejects(h.play(p));
 for(const o of [null,4,{queue:true},{signal:{aborted:false}}])await assert.rejects(h.play('/app/chime.wav',o));
 const c=new events.AbortController();c.abort();await assert.rejects(h.play('/app/chime.wav',{signal:c.signal}));
 f.failTimer=true;await assert.rejects(h.play('/app/chime.wav'),{code:'ERR_RESOURCE_EXHAUSTED'});assert.equal(f.starts,0);assert.equal(f.timers.size,0);h.close();
});
test('speaker native read/underrun errors release operation and getter reentrancy cannot resurrect handles',async()=>{
 const f=fixture(),d=f.load('devices').speaker;let h=d.open();
 f.native.start=()=>{throw Object.assign(Error('bad WAV'),{code:'EINVAL'});};await assert.rejects(h.play('/app/chime.wav'),{code:'EINVAL'});assert.equal(h.state,'open');assert.equal(f.timers.size,0);
 f.native.start=()=>{};let p=h.play('/app/chime.wav');f.native.poll=()=>{throw Object.assign(Error('underrun'),{code:'EIO'});};f.tick(1);await assert.rejects(p,{code:'EIO'});assert.equal(f.timers.size,0);h.close();
 h=d.open();await assert.rejects(h.play('/app/chime.wav',{get volume(){h.close();return .25;}}),{code:'ENXIO'});assert.equal(d.state,'idle');
 assert.equal(fixture(false).load('devices').speaker,undefined);
});
