'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
function run(options={}) {
 const r={calls:[],frames:[],events:[],closed:0,opened:0};let now=0,id=0,active=0;const timers=new Map(),module={exports:{}};
 const ctx={fillStyle:'',fillRect(...a){r.calls.push(['rect',this.fillStyle,...a]);}};
 const canvas={width:172,height:320,getContext(){return ctx;}};
 function draw(kind,file,target,x,y){assert.equal(target,canvas);assert.equal(active,0,'only one decoder controller');if(options.failDraw)throw Error('draw failed');active++;
  const row={kind,file,x,y,closeCalls:0};r.calls.push(row);return {status(){return options.stall?'loading':options.failStatus?'error':'ready';},close(){active--;row.closeCalls++;if(options.failClose)throw Error('close failed');}};
 }
 vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../examples/portable/images/features.js'),'utf8'),{module,console:{log(...a){r.events.push(a.join(' '));}},
 setTimeout(fn,ms){if(++id===options.failTimer)throw Error('timer failed');timers.set(id,{fn,at:now+ms});return id;},clearTimeout(n){if(options.failClear)throw Error('clear failed');timers.delete(n);},
 require(n){if(n==='./info')return p=>{if(options.failInfo)throw Error('bad image');return p.endsWith('.jpg')?{format:'jpeg',width:172,height:320}:{format:'bmp',width:options.badIcon?16:32,height:32};};
 if(n==='../jpeg/show')return {draw:(...a)=>draw('jpeg',...a)};if(n==='../sd-bmp/show')return {draw:(...a)=>draw('bmp',...a)};
 if(n==='devices')return {display:{open(){r.opened++;return {canvas,present(){if(options.failPresent)throw Error('present failed');r.frames.push(r.calls.slice());},close(){r.closed++;}};}}};throw Error(n);}});
 try {r.handle=module.exports('/app/background.jpg','/app/icon.bmp');}catch(e){r.error=e;}
 r.advance=end=>{let steps=0;while(true){const e=[...timers].sort((a,b)=>a[1].at-b[1].at)[0];if(!e||e[1].at>end)break;assert.ok(++steps<10000);now=e[1].at;timers.delete(e[0]);e[1].fn();}now=end;};r.pending=()=>timers.size;return r;
}
test('four scenes preserve corners, centered placement, all clipped edges and mixed overlays on one display',()=>{
 const r=run();r.advance(20000);assert.equal(r.error,undefined);assert.equal(r.opened,1);assert.equal(r.closed,0);assert.equal(r.frames.length,4);
 const draws=r.calls.filter(x=>x.kind);assert.equal(draws.length,15);assert.ok(draws.every(x=>x.closeCalls===1));
 assert.deepEqual(draws.slice(0,4).map(x=>[x.x,x.y]),[[0,0],[140,0],[0,288],[140,288]]);
 assert.deepEqual([draws[4].x,draws[4].y],[70,144]);assert.deepEqual(draws.slice(5,9).map(x=>[x.x,x.y]),[[-16,144],[156,144],[70,-16],[70,304]]);
 assert.equal(draws[9].kind,'jpeg');assert.deepEqual([draws[9].x,draws[9].y],[0,0]);assert.equal(draws.slice(10).length,5);
 assert.ok(r.calls.some(x=>Array.isArray(x)&&x[1]==='yellow'&&x[4]===172&&x[5]===2));
 for(const scene of ['corners','center','clipping','overlay'])assert.ok(r.events.includes('IMAGE_FEATURE_READY '+scene));assert.ok(r.events.includes('IMAGE_FEATURES_DONE'));
 r.handle.close();r.handle.close();r.advance(400000);assert.equal(r.closed,1);assert.equal(r.pending(),0);
});
test('cancel pending draw before any later scene, then stale callbacks remain inert',()=>{
 const r=run({stall:true});r.advance(200);r.handle.close();r.handle.close();r.advance(400000);assert.equal(r.closed,1);assert.equal(r.calls.filter(x=>x.kind).length,1);assert.equal(r.frames.length,0);assert.equal(r.pending(),0);
});
for(const options of [{failInfo:true},{badIcon:true},{failDraw:true},{failStatus:true},{failClose:true},{failPresent:true},...[1,2,3,4].map(failTimer=>({failTimer}))])test('feature diagnostic fails closed: '+JSON.stringify(options),()=>{
 const r=run(options);r.advance(400000);assert.ok(r.error||r.events.some(x=>x.startsWith('IMAGE_FEATURE_ERROR ')));assert.ok(!r.events.includes('IMAGE_FEATURES_DONE'));assert.equal(r.closed,r.opened);assert.equal(r.pending(),0);assert.ok(r.calls.filter(x=>x.kind).every(x=>x.closeCalls===1));
});
test('stalled rendering hits overall deadline without false completion',()=>{const r=run({stall:true});r.advance(400000);assert.equal(r.closed,1);assert.equal(r.pending(),0);assert.ok(r.events.some(x=>x.includes('timed out')));assert.ok(!r.events.includes('IMAGE_FEATURES_DONE'));});
test('timer cancellation failure still attempts active draw and display cleanup',()=>{
 const r=run({stall:true,failClear:true});r.advance(100);assert.throws(()=>r.handle.close(),/clear failed/);r.advance(400000);assert.equal(r.closed,1);assert.ok(r.calls.filter(x=>x.kind).every(x=>x.closeCalls===1));assert.equal(r.frames.length,0);
});

test('maintained examples no longer import the deprecated public image module',()=>{
 const root=path.join(__dirname,'../examples');
 function walk(dir){for(const entry of fs.readdirSync(dir,{withFileTypes:true})){const file=path.join(dir,entry.name);if(entry.isDirectory())walk(file);else if(entry.name.endsWith('.js'))assert.doesNotMatch(fs.readFileSync(file,'utf8'),/require\(['"]image['"]\)/,file);}}
 walk(root);
});
