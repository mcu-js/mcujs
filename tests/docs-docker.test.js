const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const image = 'sha256:' + 'a'.repeat(64);
function fixture(fn) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mcujs-docs-'));
  const source = path.join(dir, 'repo with spaces', 'docs');
  const bin = path.join(dir, 'bin');
  fs.mkdirSync(source, {recursive:true}); fs.mkdirSync(bin);
  for (const name of ['docker-build.sh', 'docker-entrypoint.sh']) {
    if (fs.existsSync(path.join(root, 'docs', name))) fs.copyFileSync(path.join(root, 'docs', name), path.join(source, name));
  }
  const log = path.join(dir, 'calls');
  fs.writeFileSync(log, '');
  fs.writeFileSync(path.join(bin, 'docker'), `#!${process.execPath}\nconst fs=require('node:fs'); const a=process.argv.slice(2); fs.appendFileSync(process.env.LOG,JSON.stringify(a)+'\\n'); if(a[0]==='image') { if(process.env.MISSING) process.exit(1); console.log('${image}'); } else if(a[0]==='run') process.exit(Number(process.env.FAIL||0)); else process.exit(99);\n`, {mode:0o755});
  const env = {...process.env, PATH:bin+':'+process.env.PATH, LOG:log};
  const run = (args, extra={}) => spawnSync('bash', [path.join(source,'docker-build.sh'),...args], {env:{...env,...extra},encoding:'utf8'});
  try { fn({dir,source,bin,env,run,calls:()=>fs.readFileSync(log,'utf8').trim().split('\n').filter(Boolean).map(JSON.parse)}); }
  finally { fs.rmSync(dir,{recursive:true,force:true}); }
}
// Docker is a recording fixture, never the daemon. Not website build evidence.
test('docs dispatch is offline, immutable, read-only and bounded', () => fixture(({dir,source,run,calls}) => {
  const out=path.join(dir,'output'); const r=run([out]); assert.equal(r.status,0,r.stderr);
  const c=calls(); assert.deepEqual(c.map(a=>a[0]),['image','run']); const a=c[1];
  for(const [key,value] of [['--network','none'],['--pull','never'],['--cap-drop','ALL'],['--memory','2g'],['--pids-limit','256']]) assert.equal(a[a.indexOf(key)+1],value);
  assert.ok(a.includes('--read-only')); assert.ok(a.includes(source+':/source:ro')); assert.ok(a.includes(image));
  assert.ok(a.includes('600')); assert.equal(a.filter(x=>x.includes('node_modules')).length,0);
}));
test('missing image does not run or create output',()=>fixture(({dir,run,calls})=>{
  const out=path.join(dir,'output'); const r=run([out],{MISSING:'1'}); assert.notEqual(r.status,0); assert.equal(calls().length,1); assert.equal(fs.existsSync(out),false);
}));
test('source output and occupied output are rejected',()=>fixture(({dir,source,run,calls})=>{
  assert.notEqual(run([path.join(source,'build')]).status,0);
  const out=path.join(dir,'output'); fs.mkdirSync(out); fs.writeFileSync(path.join(out,'keep'),'unchanged');
  assert.notEqual(run([out]).status,0); assert.equal(fs.readFileSync(path.join(out,'keep'),'utf8'),'unchanged'); assert.equal(calls().length,0);
}));
test('website failure propagates separately',()=>fixture(({dir,run})=>{assert.equal(run([path.join(dir,'output')],{FAIL:'42'}).status,42);}));

for (const mode of ['success', 'lock-mismatch', 'typecheck-failure', 'build-failure']) {
  test(`entrypoint ${mode}: source unchanged, only image dependencies, no install`,()=>fixture(({dir,source,bin,env})=>{
    const deps=path.join(dir,'image-dependencies'); const out=path.join(dir,'output');
    fs.mkdirSync(path.join(deps,'node_modules'),{recursive:true}); fs.mkdirSync(out);
    for(const name of ['package.json','package-lock.json']) {
      fs.writeFileSync(path.join(source,name),'{}\n'); fs.writeFileSync(path.join(deps,name),'{}\n');
    }
    if(mode==='lock-mismatch') fs.writeFileSync(path.join(deps,'package-lock.json'),'different\n');
    fs.writeFileSync(path.join(deps,'node_modules','image-only'),'locked fixture');
    fs.mkdirSync(path.join(source,'node_modules')); fs.writeFileSync(path.join(source,'node_modules','poison'),'do not copy');
    fs.mkdirSync(path.join(source,'build')); fs.writeFileSync(path.join(source,'build','stale'),'do not copy');
    fs.mkdirSync(path.join(source,'.docusaurus')); fs.writeFileSync(path.join(source,'.docusaurus','stale'),'do not copy');
    // npm is a local recorder, not a dependency install or a Docusaurus build.
    fs.writeFileSync(path.join(bin,'npm'),`#!${process.execPath}
const fs=require('node:fs'); const a=process.argv.slice(2);
if(fs.existsSync('node_modules/poison') || fs.existsSync('build/stale') || fs.existsSync('.docusaurus/stale') || !fs.existsSync('node_modules/image-only')) process.exit(90);
if(a.join(' ')==='run typecheck') process.exit(process.env.MODE==='typecheck-failure'?41:0);
if(a[0]==='run' && a[1]==='build' && a[2]==='--' && a[3]==='--out-dir') {
  if(process.env.MODE==='build-failure') process.exit(42);
  fs.mkdirSync(a[4]); fs.writeFileSync(require('node:path').join(a[4],'index.html'),'fixture only');
} else process.exit(99);
`,{mode:0o755});
    function snapshot(dir) {
      return fs.readdirSync(dir,{withFileTypes:true}).sort((a,b)=>a.name.localeCompare(b.name)).map(e=>[e.name,e.isDirectory()?snapshot(path.join(dir,e.name)):fs.readFileSync(path.join(dir,e.name),'hex')]);
    }
    const before=snapshot(source);
    const r=spawnSync('bash',[path.join(source,'docker-entrypoint.sh'),source,deps,out],{env:{...env,MODE:mode},encoding:'utf8'});
    assert.equal(r.status,{'success':0,'lock-mismatch':1,'typecheck-failure':41,'build-failure':42}[mode],r.stderr);
    assert.deepEqual(snapshot(source),before);
    assert.deepEqual(fs.readdirSync(out),mode==='success'?['index.html']:[]);
    if(mode==='success') assert.equal(fs.readFileSync(path.join(out,'index.html'),'utf8'),'fixture only');
  }));
}
