'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
function fixture(supported=true) {
 const cache={}, timers=new Map();let id=0, token=0, open=false, playing=false;
 const native={open(){if(open)throw Object.assign(Error('owned'),{code:'EBUSY'});open=true;return ++token;},
 start(t,f,d){assert.equal(t,token);playing=true;return f;},stop(t){assert.equal(t,token);playing=false;},close(t){assert.equal(t,token);playing=false;open=false;},playing(t){assert.equal(t,token);return playing;},state(){return open?'busy':'idle';}};
 const context=vm.createContext({console,Promise,setTimeout(fn){timers.set(++id,fn);return id;},clearTimeout(n){timers.delete(n);}});
 function load(name){if(name==='board')return {capability(){return supported?{buzzer:{interface:'tone'}}:{}}};if(name==='mcujs:buzzer-native')return native;
 if(!cache[name]) {const module={};context.module=module;context.require=load;vm.runInContext('(function(require,module){'+fs.readFileSync(path.join(__dirname,'../lib/',name==='mcujs:buzzer'?'buzzer.js':name+'.js'),'utf8')+'\n})',context)(load,module);cache[name]=module.exports;}return cache[name];}
 return {load,native,timers,get playing(){return playing;},finish(){playing=false;for(const [id,fn] of [...timers]){timers.delete(id);fn();}}};
}
test('one configured buzzer completes, aborts despite stopped event dispatch, and reopens safely',async()=>{
 const f=fixture(),d=f.load('devices').buzzer;assert.ok(d,'configured buzzer descriptor missing');
 const h=d.open();assert.equal(d.state,'busy');assert.throws(()=>d.open(),{code:'EBUSY'});
 let p=h.beep({frequency:1000,duration:20});assert.equal(f.playing,true);f.finish();await p;assert.equal(h.state,'open');
 const e=f.load('events'),c=new e.AbortController();c.signal.addEventListener('abort',event=>event.stopImmediatePropagation());
 p=h.beep({frequency:1000,duration:100,signal:c.signal});const rejected=assert.rejects(p,x=>x==='cancel');c.abort('cancel');await rejected;assert.equal(f.playing,false);assert.equal(f.timers.size,0);
 h.close();assert.equal(h.state,'closed');const next=d.open();await assert.rejects(h.beep({frequency:1000,duration:20}),{code:'ENXIO'});h.close();assert.equal(d.state,'busy');next.close();
});
test('unsupported boards omit buzzer; getters cannot resurrect closed handles',async()=>{
 assert.equal(fixture(false).load('devices').buzzer,undefined);
 const f=fixture(),h=f.load('devices').buzzer.open();
 await assert.rejects(h.beep({get frequency(){h.close();return 1000;},duration:20}),{code:'ENXIO'});assert.equal(f.playing,false);
});
