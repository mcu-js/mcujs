'use strict';
// Compose the real metadata, slideshow and renderers over fake hardware and one clock.
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
function run(options={}) {
 const jpg=Buffer.from([255,216,255,192,0,11,8,0,1,0,1,1,1,17,0]);
 const bmp=Buffer.alloc(58);bmp.write('BM');bmp.writeUInt32LE(58,2);bmp.writeUInt32LE(54,10);bmp.writeUInt32LE(40,14);bmp.writeInt32LE(1,18);bmp.writeInt32LE(1,22);bmp.writeUInt16LE(1,26);bmp.writeUInt16LE(24,28);bmp.set([0,0,255],54);
 const data={'/app/a.jpg':jpg,'/app/b.bmp':bmp};const r={events:[],displays:[],filesOpened:0,filesClosed:0,decoderOpened:0,decoderClosed:0};
 let now=0,id=0,fd=0;const timers=new Map(),opened=new Map(),counts={},cache={};
 const fileApi={statSync(p){return {size:data[p].length};},openSync(p){r.filesOpened++;opened.set(++fd,data[p]);return fd;},closeSync(f){assert.ok(opened.has(f));opened.delete(f);if(++r.filesClosed===options.failFileClose)throw Error('file close failure');},readSync(f,b,o,n,pos){const d=opened.get(f),count=Math.max(0,Math.min(n,d.length-pos));b.set(d.subarray(pos,pos+count),o);return count;}};
 const devices={display:{open(){assert.ok(r.displays.every(s=>s.closedAt!==null));const s={openedAt:now,presentedAt:null,closedAt:null,closeCalls:0};r.displays.push(s);return {canvas:{width:1,height:1,getContext(){return {fillRect(){}};}},present(){s.presentedAt=now;},close(){s.closedAt=now;s.closeCalls++;if(options.failDisplayClose)throw Error('display close failure');}};}}};
 const jpeg={open(){r.decoderOpened++;let read=false;return {width:1,height:1,read(b){if(read)return null;read=true;now+=options.decodeCost||0;b.set([255,0,0]);return {x:0,y:0,width:1,height:1};},close(){r.decoderClosed++;if(options.failDecoderClose)throw Error('decoder close failure');}};}};
 function load(name) {
  if(cache[name])return cache[name].exports;
  const module={exports:{}};cache[name]=module;
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../examples/portable',name+'.js'),'utf8'),{module,Uint8Array,DataView,console:{log(...a){r.events.push(a.join(' '));}},
   setTimeout(fn,delay){const key=name+':'+delay;counts[key]=(counts[key]||0)+1;if(options.failTimer===key&&counts[key]===(options.failTimerNth||1))throw Error('timer failure');timers.set(++id,{fn,at:now+delay});return id;},
   clearTimeout(n){if(options.failClear===name)throw Error('cancel failure');timers.delete(n);},
   require(n){if(n==='fs')return fileApi;if(n==='devices')return devices;if(n==='jpeg')return jpeg;return load(path.posix.normalize(path.posix.join(path.posix.dirname(name),n)));}
  });return module.exports;
 }
 r.handle=load('images/slideshow')(['/app/a.jpg','/app/b.bmp']);
 r.advance=end=>{let turns=0;while(true){const e=[...timers].sort((a,b)=>a[1].at-b[1].at)[0];if(!e||e[1].at>end)break;assert.ok(++turns<10000);timers.delete(e[0]);now=Math.max(now,e[1].at);e[1].fn();}now=Math.max(now,end);};
 r.filesLive=()=>opened.size;r.pending=()=>timers.size;return r;
}
test('real JPEG completing just before its deadline stays visible for the full slideshow hold',()=>{
 const r=run({decodeCost:119000});r.advance(124000);assert.equal(r.displays.length,2);
 for(const d of r.displays){assert.ok(d.closedAt-d.presentedAt>=2000,JSON.stringify(d));assert.equal(d.closeCalls,1);}
 assert.ok(r.events.includes('SLIDESHOW_DONE'));assert.equal(r.pending(),0);assert.equal(r.filesLive(),0);
});
for(const options of [{failFileClose:1},{failFileClose:2},{failDecoderClose:true},{failDisplayClose:true},
 {failTimer:'jpeg/show:0'},{failTimer:'jpeg/show:0',failTimerNth:3},{failTimer:'jpeg/show:60000'},
 {failTimer:'sd-bmp/show:0'},{failTimer:'sd-bmp/show:60000'}])test('real inner failure stops slideshow: '+JSON.stringify(options),()=>{
 const r=run(options);r.advance(125000);assert.ok(r.events.some(e=>e.startsWith('SLIDESHOW_ERROR ')));assert.ok(!r.events.includes('SLIDESHOW_DONE'));
 if(!String(options.failTimer).startsWith('sd-bmp'))assert.ok(r.displays.length<=1);
 assert.equal(r.filesLive(),0);assert.equal(r.filesOpened,r.filesClosed);assert.equal(r.decoderOpened,r.decoderClosed);assert.ok(r.displays.every(d=>d.closeCalls===1));assert.equal(r.pending(),0);
});
for(const owner of ['images/slideshow','jpeg/show'])test('real composed cleanup survives cancellation failure in '+owner,()=>{
 const r=run({failClear:owner});r.advance(0);try{r.handle.close();}catch(e){assert.match(e.message,/cancel failure/);}r.advance(200000);
 assert.equal(r.filesLive(),0);assert.equal(r.decoderClosed,r.decoderOpened);assert.ok(r.displays.every(d=>d.closeCalls===1));assert.ok(r.displays.length<=1);assert.ok(!r.events.includes('SLIDESHOW_DONE'));
});
