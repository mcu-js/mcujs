'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
function fixture(supported = true) {
  let now = 0, pressed = false, samples = 0, next = 1, fail = false, full = false;
  const timers = new Map(), logs = [], loads = [], cache = {};
  const cap = Object.freeze({ interface: 'button-events', readOnly: true, maxOpenHandles: 1, pollIntervalMs: 10, debounceMs: 30 });
  const board = { capability: () => supported ? Object.freeze({ button: cap }) : undefined,
    millis: () => now >>> 0, buttonPressed() { samples++; if (fail) throw Object.assign(new Error('sample failed'), { code: 'EIO' }); return pressed; } };
  const context = vm.createContext({ console: { error: (...a) => logs.push(a), log: (...a) => logs.push(a) },
    setInterval(fn, ms) { if (full) throw new Error('Maximum number of timers reached'); const id=next++; timers.set(id,{fn,ms,due:now+ms});return id; },
    clearInterval(id) { timers.delete(id); } });
  function load(name) {
    if (name==='board') return board;
    if (!cache[name]) {
      loads.push(name); const module={};
      const file=name==='mcujs:button'?'button':name;
      vm.runInContext('(function(require,module){'+fs.readFileSync(path.join(__dirname,'../lib/'+file+'.js'),'utf8')+'\n})',context)(load,module);
      cache[name]=module.exports;
    }
    return cache[name];
  }
  return { devices:load('devices'),load,cap,timers,logs,loads,
    get samples(){return samples;}, press(v){pressed=v;}, fail(v){fail=v;}, full(v){full=v;},
    clock(v){assert.equal(timers.size,0);now=v;},
    advance(ms){const end=now+ms;for(;;){const entry=[...timers].sort((a,b)=>a[1].due-b[1].due)[0];if(!entry||entry[1].due>end)break;now=entry[1].due;entry[1].due=now+entry[1].ms;entry[1].fn();}now=end;},
  };
}
test('button discovery is capability-gated, stable and passive',()=>{
  const absent=fixture(false);assert.equal(absent.devices.button,undefined);assert.equal(absent.samples,0);
  const f=fixture(),d=f.devices.button;assert.ok(d);assert.equal(d,f.devices.button);assert.ok(Object.isFrozen(d));
  assert.equal(d.capabilities,f.cap);assert.equal(d.state,'idle');assert.equal(f.samples,0);assert.equal(f.timers.size,0);
  assert.equal(f.loads.includes('events'),false);
});
test('open samples read-only state; exclusive ownership and close/reopen are bounded',()=>{
  const f=fixture(),d=f.devices.button;
  assert.throws(()=>d.open({pin:0}),{name:'TypeError'});assert.equal(f.samples,0);
  f.press(true);const h=d.open();assert.equal(h.state,'open');assert.equal(h.pressed,true);
  assert.equal(d.state,'busy');assert.equal(f.samples,1);assert.equal(f.timers.size,1);
  assert.throws(()=>d.open(),{name:'ResourceBusyError',code:'EBUSY'});h.close();h.close();assert.equal(h.state,'closed');
  assert.throws(()=>h.pressed,{code:'ENXIO'});assert.equal(d.state,'idle');assert.equal(f.timers.size,0);
  const next=d.open();h.close();assert.equal(d.state,'busy');assert.notEqual(next,h);next.close();
});
test('press/release debounce uses sampled monotonic time, ignores bounce and holds',()=>{
  const f=fixture(),h=f.devices.button.open(),edges=[];
  for(const type of ['press','release']) h.addEventListener(type,function(e){assert.equal(this,h);assert.equal(e.target,h);assert.equal(e.currentTarget,h);edges.push([e.type,h.pressed]);});
  f.advance(100);assert.deepEqual(edges,[]);
  f.press(true);f.advance(10);f.press(false);f.advance(10);assert.deepEqual(edges,[]);
  f.press(true);f.advance(39);assert.equal(h.pressed,false);f.advance(1);assert.deepEqual(edges,[['press',true]]);
  f.advance(100);assert.equal(edges.length,1);
  f.press(false);f.advance(10);f.press(true);f.advance(10);assert.equal(h.pressed,true);
  f.press(false);f.advance(40);assert.deepEqual(edges,[['press',true],['release',false]]);h.close();
});
test('initially held input emits no event and debounce survives uint32 clock wrap',()=>{
  const f=fixture();f.clock(0xfffffff0);f.press(true);const h=f.devices.button.open();let n=0;
  h.addEventListener('press',()=>n++);h.addEventListener('release',()=>n++);f.advance(10);assert.equal(n,0);
  f.press(false);f.advance(40);assert.equal(h.pressed,false);assert.equal(n,1);h.close();
});
test('close during delivery removes later listeners and releases external AbortSignal subscriptions',()=>{
  const f=fixture(),E=f.load('events'),controller=new E.AbortController();let later=0;
  for(let i=0;i<32;i++) {const h=f.devices.button.open();h.addEventListener('press',()=>h.close(),{signal:controller.signal});h.addEventListener('press',()=>later++,{signal:controller.signal});f.press(true);f.advance(40);h.close();f.press(false);}
  assert.equal(later,0);assert.equal(f.timers.size,0);assert.equal(controller.signal.aborted,false);
});
test('listener options, once, cancellation, duplicate identity and bounded pressure',()=>{
  const f=fixture(),E=f.load('events'),h=f.devices.button.open(),c=new E.AbortController();let n=0;
  function cb(){n++;}h.addEventListener('press',cb,{once:true});h.addEventListener('press',cb);
  f.press(true);f.advance(40);f.press(false);f.advance(40);f.press(true);f.advance(40);assert.equal(n,1);
  h.addEventListener('release',cb,{signal:c.signal});c.abort();f.press(false);f.advance(40);assert.equal(n,1);
  for(let i=0;i<16;i++) h.addEventListener('press',()=>n++);
  assert.throws(()=>h.addEventListener('press',()=>{}),{name:'RangeError'});
  f.press(true);f.advance(40);assert.equal(n,17);h.close();
  assert.throws(()=>h.addEventListener('press',cb),{code:'ENXIO'});
});
test('sampler and timer failures are explicit, release ownership and allow retry',()=>{
  const f=fixture(),d=f.devices.button;f.fail(true);assert.throws(()=>d.open(),{name:'Error',code:'EIO'});assert.equal(d.state,'unavailable');assert.equal(f.timers.size,0);
  f.fail(false);f.full(true);assert.throws(()=>d.open(),{code:'ERR_RESOURCE_EXHAUSTED'});assert.equal(f.timers.size,0);
  f.full(false);const h=d.open();let failure;
  h.addEventListener('error',e=>{failure=e.error;assert.equal(h.state,'error');assert.equal(d.state,'unavailable');});
  f.fail(true);f.advance(10);assert.equal(failure.code,'EIO');assert.equal(h.state,'error');assert.equal(f.timers.size,0);
  assert.throws(()=>h.pressed,{code:'ENXIO'});f.fail(false);const replacement=d.open();h.close();assert.equal(h.state,'error');assert.equal(d.state,'busy');replacement.close();
});
test('cancelled and once listeners do not exhaust tracking during sustained churn',()=>{
  const f=fixture(),E=f.load('events'),h=f.devices.button.open();let n=0;
  for(let i=0;i<64;i++) {const c=new E.AbortController();h.addEventListener('press',()=>n++,{signal:c.signal});c.abort();}
  for(let i=0;i<32;i++){h.addEventListener('press',()=>n++,{once:true});f.press(true);f.advance(40);f.press(false);f.advance(40);}
  assert.equal(n,32);h.close();assert.equal(f.timers.size,0);
});
test('close clears inherited EventTarget listeners and external signal subscriptions',()=>{
  const f=fixture(),E=f.load('events'),c=new E.AbortController();
  for(let i=0;i<32;i++){const h=f.devices.button.open();E.EventTarget.prototype.addEventListener.call(h,'press',()=>{}, {signal:c.signal});h.close();assert.throws(()=>h.addEventListener('press',()=>{}),{code:'ENXIO'});}
  assert.equal(f.timers.size,0);assert.equal(c.signal.aborted,false);
});
test('closing from an options getter cannot reattach subscriptions',()=>{
  const f=fixture(),E=f.load('events'),c=new E.AbortController();
  for(let i=0;i<32;i++){const h=f.devices.button.open();assert.throws(()=>h.addEventListener('press',()=>{}, {get once(){h.close();return false;},signal:c.signal}),{code:'ENXIO'});}
  assert.equal(f.timers.size,0);assert.equal(c.signal.aborted,false);
});
test('synthetic events do not rewrite input and listener exceptions do not stop subsequent delivery',()=>{
  const f=fixture(),E=f.load('events'),h=f.devices.button.open();let n=0;
  h.addEventListener('press',()=>{throw Error('listener fixture');});h.addEventListener('press',()=>n++);
  h.dispatchEvent(new E.Event('press'));assert.equal(h.pressed,false);assert.equal(n,1);
  f.press(true);f.advance(40);assert.equal(h.pressed,true);assert.equal(n,2);assert.equal(f.logs.length,2);h.close();
});
test('EventTarget.clear removes current registrations without closing targets or aborting signals',()=>{
  const f=fixture(),E=f.load('events'),t=new E.EventTarget(),c=new E.AbortController();let n=0;
  t.addEventListener('x',()=>{n++;E.EventTarget.clear(t);});t.addEventListener('x',()=>n++,{signal:c.signal});
  t.dispatchEvent(new E.Event('x'));assert.equal(n,1);assert.equal(c.signal.aborted,false);
  for(let i=0;i<16;i++)t.addEventListener('x'+i,()=>{}, {signal:c.signal});
  E.EventTarget.clear(t);t.addEventListener('x',()=>n++);t.dispatchEvent(new E.Event('x'));assert.equal(n,2);
  assert.throws(()=>E.EventTarget.clear({}),{name:'TypeError'});
});
module.exports={fixture};
