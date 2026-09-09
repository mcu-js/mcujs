const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../examples/waveshare-epaper-1.54-v2/night-librarian.js'),'utf8');
test('one timer; varied waits; two blinks then cleaning; stop cancels; no hardware API',()=>{
 let tasks=new Map(),id=0,full=0,draws=0,random=[0,0.5,0.9,0.9,0.1,0.5],ri=0;
 const math=Object.create(Math);math.random=()=>random[(ri++)%random.length];
 const c={beginPath(){},moveTo(){},lineTo(){},closePath(){},fill(){draws++},fillRect(){draws++}};
 const env={module:{exports:{}},Math:math,require(n){assert.equal(n,'/wandering-library.js');return ()=>full++},setTimeout(f,ms){assert.equal(tasks.size,0);tasks.set(++id,{f,ms});return id},clearTimeout(i){tasks.delete(i)}};
 vm.runInNewContext(source,env);const a=env.module.exports({width:200,height:200,getContext(n){assert.equal(n,'2d');return c}});
 function tick(){const [i,t]=tasks.entries().next().value;tasks.delete(i);t.f();assert.equal(tasks.size,1);return t.ms;}
 assert.equal(full,1);assert.equal(tick(),4000);assert.equal(a.status().state,'blinking');
 assert.equal(tick(),1600);assert.equal(a.status().blinks,1);assert.equal(tick(),20000);
 assert.equal(tick(),1600);assert.equal(a.status().blinks,2);assert.equal(tick(),4000);assert.equal(full,2);
 assert.equal(a.status().cleanings,1);assert.ok(a.status().nextDelayMs>28000);assert.ok(draws>0);
 a.stop();assert.equal(tasks.size,0);assert.equal(a.status().running,false);
});
