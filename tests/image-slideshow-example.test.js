'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
function run(paths,options={}){
 const r={sessions:[],events:[],metadata:[],maxLive:0};let now=0,id=0,live=0;const timers=new Map(),module={exports:{}};
 function show(file){if(options.openError===file)throw Error('open failed');assert.equal(live,0,'previous display is closed first');live++;r.maxLive=Math.max(live,r.maxLive);const s={file,state:'loading',closed:0};r.sessions.push(s);return {status(){return s.state;},close(){s.closed++;live--;if(options.closeError)throw Error('close failed');}};}
 vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../examples/portable/images/slideshow.js'),'utf8'),{module,console:{log(x){r.events.push(x);}},setTimeout(fn,ms){if(++id===options.failTimer)throw Error('timer failed');timers.set(id,{fn,at:now+ms});return id;},clearTimeout(i){timers.delete(i);},require(name){
  if(name==='./info')return file=>{r.metadata.push(file);if(options.infoError===file)throw Error('bad header');return {format:file.endsWith('.bmp')?'bmp':'jpeg',width:2,height:3};};
  if(name==='../jpeg/show'||name==='../sd-bmp/show')return show;throw Error(name);
 }});
 try{r.handle=module.exports(paths);}catch(e){r.error=e;}
 r.advance=ms=>{const end=now+ms;let count=0;while(true){const e=[...timers].sort((a,b)=>a[1].at-b[1].at)[0];if(!e||e[1].at>end)break;assert.ok(++count<10000);now=e[1].at;timers.delete(e[0]);e[1].fn();}now=end;};r.pending=()=>timers.size;return r;
}
test('slideshow waits for real completion, holds, closes then advances once',()=>{
 const paths=['/app/a.jpg','/sd/b.bmp'],r=run(paths);paths[1]='/wrong.jpg';r.advance(0);assert.deepEqual(r.metadata,['/app/a.jpg']);r.advance(60000);assert.equal(r.sessions.length,1);assert.equal(r.sessions[0].closed,0);
 r.sessions[0].state='ready';r.advance(100);r.advance(1999);assert.equal(r.sessions.length,1);r.advance(1);assert.equal(r.sessions[0].closed,1);assert.equal(r.sessions[1].file,'/sd/b.bmp');
 r.sessions[1].state='ready';r.advance(2100);assert.equal(r.sessions[1].closed,1);assert.equal(r.maxLive,1);assert.equal(r.pending(),0);assert.ok(r.events.includes('SLIDESHOW_DONE'));r.handle.close();r.handle.close();assert.equal(r.sessions[1].closed,1);
});
test('cancellation stops current work and prevents later slides',()=>{const r=run(['/app/a.jpg','/app/b.jpg']);r.advance(0);r.handle.close();r.handle.close();r.advance(200000);assert.equal(r.sessions.length,1);assert.equal(r.sessions[0].closed,1);assert.equal(r.pending(),0);assert.ok(!r.events.includes('SLIDESHOW_DONE'));});
test('metadata, open and asynchronous decode errors advance without leaking handles',()=>{
 for(const kind of ['infoError','openError','decodeError']){const r=run(['/app/a.jpg','/app/b.bmp'],{[kind]:'/app/a.jpg'});r.advance(0);if(kind==='decodeError'){r.sessions[0].state='error';r.advance(100);}assert.equal(r.sessions.at(-1).file,'/app/b.bmp');assert.ok(r.events.some(e=>e.startsWith('SLIDE_ERROR ')));r.handle.close();assert.ok(r.sessions.every(s=>s.closed===1));assert.equal(r.pending(),0);}
});
test('a stalled renderer has a per-slide deadline and no endless retry',()=>{const r=run(['/app/a.jpg']);r.advance(122000);assert.equal(r.sessions[0].closed,1);assert.equal(r.pending(),0);assert.ok(r.events.some(e=>e.includes('timed out')));assert.ok(r.events.includes('SLIDESHOW_DONE'));});
test('invalid or oversized lists fail before metadata or renderer access',()=>{for(const paths of [[],null,'/a.jpg',[3],Array(9).fill('/a.jpg'),['']]){const r=run(paths);assert.ok(r.error);assert.equal(r.metadata.length,0);assert.equal(r.sessions.length,0);assert.equal(r.pending(),0);}});
test('timer or close failures stop safely without starting a competing display',()=>{
 for(const options of [{failTimer:1},{failTimer:2},{failTimer:3},{closeError:true}]){const r=run(['/app/a.jpg','/app/b.bmp'],options);r.advance(0);if(r.sessions[0])r.sessions[0].state='ready';r.advance(125000);assert.ok(r.sessions.every(s=>s.closed===1));assert.ok(r.sessions.length<=1);assert.equal(r.pending(),0);assert.ok(r.error||r.events.some(e=>e.startsWith('SLIDESHOW_ERROR ')));}
});
