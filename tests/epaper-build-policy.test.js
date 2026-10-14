const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const os=require('node:os');
const path=require('node:path');
const {spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');
const features=['PARTIAL','DIFF','DITHER'];
const flags=['MCUJS_EXPERIMENTAL_CANVAS',...features.map(f=>`MCUJS_EXPERIMENTAL_EPAPER_${f}`)];
const v2='waveshare_esp32s3_epaper_1.54_v2';
// Execute production pre-IDF policy without installing/calling an SDK.
function configure(board,env={},defines=[]){
 const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'mcujs-epaper-policy-'));
 try {
  let policy=fs.readFileSync(path.join(root,'platform/esp32/CMakeLists.txt'),'utf8').split('set(COMPONENTS main)')[0];
  policy=policy.replace(/get_filename_component\(MCUJS_ROOT[^\n]*\)/,`set(MCUJS_ROOT "${root}")`)
   .replace('include(${CMAKE_CURRENT_LIST_DIR}/js-memory.cmake)',`include("${root}/platform/esp32/js-memory.cmake")`);
  const component=fs.readFileSync(path.join(root,'platform/esp32/main/CMakeLists.txt'),'utf8');
  const definitions=component.slice(component.indexOf('if(MCUJS_EXPERIMENTAL_CANVAS AND MCUJS_BOARD STREQUAL'),component.indexOf('\ntarget_compile_definitions(${COMPONENT_LIB} PRIVATE\n    MCUJS_VERSION='));
  const script=path.join(tmp,'policy.cmake');
  fs.writeFileSync(script,'# Script-mode boundary: SD compile definitions are unrelated to ePaper policy.\nfunction(add_compile_definitions)\nendfunction()\n'+policy+`
set(COMPONENT_LIB epaper)
function(target_compile_definitions)
 foreach(definition IN LISTS ARGN)
  if(definition MATCHES "^MCUJS_EXPERIMENTAL_EPAPER_")
   message(STATUS "DEFINITION=\${definition}")
  endif()
 endforeach()
endfunction()
`+definitions+'\n'+flags.map(f=>`message(STATUS "FLAG=${f}:\${${f}}")`).join('\n'));
  const clean={...process.env,MCUJS_BOARD:board};
  for(const f of flags)delete clean[f];
  const result=spawnSync('cmake',[...defines,'-P',script],{env:{...clean,...env},encoding:'utf8'});
  assert.ifError(result.error);
  const output=result.stdout+result.stderr;
  return {status:result.status,output,flags:flags.map(f=>new RegExp(`FLAG=${f}:ON`).test(output)),
   definitions:features.filter(f=>output.includes(`DEFINITION=MCUJS_EXPERIMENTAL_EPAPER_${f}=1`))};
 } finally {fs.rmSync(tmp,{recursive:true,force:true});}
}
test('normal V2 enables Canvas, partial, packed diff and dither; other ESP defaults unchanged',()=>{
 const r=configure(v2);assert.equal(r.status,0,r.output);assert.deepEqual(r.flags,[true,true,true,true]);assert.deepEqual(r.definitions,features);
 for(const board of ['seeed_xiao_esp32s3','seeed_reterminal_sticky']){
  const r=configure(board);assert.equal(r.status,0,r.output);assert.deepEqual(r.flags,[false,false,false,false]);assert.deepEqual(r.definitions,[]);
 }
});
test('every V2 feature combination reaches component definitions via environment and CMake escape hatches',()=>{
 for(let bits=0;bits<8;bits++){
  const enabled=features.map((f,i)=>!!(bits&(1<<i)));
  const env=Object.fromEntries(features.map((f,i)=>[`MCUJS_EXPERIMENTAL_EPAPER_${f}`,enabled[i]?'1':'0']));
  for(const r of [configure(v2,env),configure(v2,{},features.map((f,i)=>`-DMCUJS_EXPERIMENTAL_EPAPER_${f}=${enabled[i]?'ON':'OFF'}`))]){
   assert.equal(r.status,0,r.output);assert.deepEqual(r.flags,[true,...enabled]);assert.deepEqual(r.definitions,features.filter((f,i)=>enabled[i]));
  }
 }
 const r=configure(v2,{MCUJS_EXPERIMENTAL_EPAPER_DIFF:'0'},['-DMCUJS_EXPERIMENTAL_EPAPER_DIFF=ON']);
 assert.equal(r.status,0,r.output);assert.deepEqual(r.flags,[true,true,false,true]);
});
test('fresh Canvas-off V2 disables driver features; incompatible explicit features fail closed',()=>{
 for(const r of [configure(v2,{MCUJS_EXPERIMENTAL_CANVAS:'0'}),configure(v2,{},['-DMCUJS_EXPERIMENTAL_CANVAS=OFF'])]){
  assert.equal(r.status,0,r.output);assert.deepEqual(r.flags,[false,false,false,false]);assert.deepEqual(r.definitions,[]);
 }
 for(const f of features){
  for(const [board,env] of [[v2,{MCUJS_EXPERIMENTAL_CANVAS:'0'}],['seeed_reterminal_sticky',{MCUJS_EXPERIMENTAL_CANVAS:'1'}],['seeed_xiao_esp32s3',{}]]){
   const r=configure(board,{...env,[`MCUJS_EXPERIMENTAL_EPAPER_${f}`]:'1'});
   assert.notEqual(r.status,0,r.output);assert.match(r.output,/requires Canvas on ePaper V2/);
  }
 }
});
