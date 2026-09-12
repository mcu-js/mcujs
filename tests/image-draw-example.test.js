'use strict';
// Real lib/canvas.js, with an independent pixel-center rectangle backend.
// JPEG bytes/MCUs are mocked only at the existing fs/jpeg module seams.
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const root = path.join(__dirname, '..');
function load(file, requireModule, extra = {}) {
  const module = {exports: {}};
  vm.runInNewContext(fs.readFileSync(path.join(root, file), 'utf8'),
    {module, require: requireModule, Uint8Array, DataView, ...extra}, {filename: file});
  return module.exports;
}
function target(width = 5, height = 4) {
  const r = {opened: 0, closed: 0, presented: 0, draws: [], pixels: Array.from({length: height}, () => Array(width).fill('#123456'))};
  const api = load('lib/canvas.js', name => {
    if (name === 'events') return require('../lib/events.js');
    assert.equal(name, 'mcujs:canvas-native');
    return {open() {
      r.opened++;
      return {width, height, maxTouchPoints: 0, setLifecycle() {}, getState() {return 'open';},
        close() {r.closed++;}, present() {r.presented++;}, stopPointer() {},
        draw(commands, mode, rgba) {
          if (r.failDraw) throw Error('draw failure');
          assert.equal(mode, 'fill'); assert.equal(commands[0], 4); assert.equal(commands.length, 5);
          const [, x, y, w, h] = commands;
          const color = '#' + Array.from(rgba).slice(0, 3).map(n => Math.round(n * 255).toString(16).padStart(2, '0')).join('');
          r.draws.push([color, x, y, w, h]);
          // Scan the finite target, not the image: signed32 offscreen origins stay cheap.
          for (let py = 0; py < height; py++) for (let px = 0; px < width; px++) {
            if (px + .5 >= x && px + .5 < x + w && py + .5 >= y && py + .5 < y + h) r.pixels[py][px] = color;
          }
        }};
    }};
  });
  r.canvas = api.canvas;
  return r;
}
function bitmap(bits = 24, topDown = false) {
  const top = bits === 16 ? [0,248,224,7,224,7,99,99] : bits === 32 ?
    [0,0,255,0,0,255,0,1,0,255,0,255] : [0,0,255,0,255,0,0,255,0,99,99,99];
  const bottom = bits === 16 ? [31,0,255,255,16,132,88,88] : bits === 32 ?
    [255,0,0,127,255,255,255,0,132,130,132,200] : [255,0,0,255,255,255,132,130,132,88,88,88];
  const offset = bits === 16 ? 66 : 54, data = Buffer.alloc(offset + top.length * 2);
  data.write('BM'); data.writeUInt32LE(data.length, 2); data.writeUInt32LE(offset, 10);
  data.writeUInt32LE(40, 14); data.writeInt32LE(3, 18); data.writeInt32LE(topDown ? -2 : 2, 22);
  data.writeUInt16LE(1, 26); data.writeUInt16LE(bits, 28); data.writeUInt32LE(bits === 16 ? 3 : 0, 30);
  if (bits === 16) data.set([0,248,0,0,224,7,0,0,31,0,0,0], 54);
  data.set(topDown ? top : bottom, offset); data.set(topDown ? bottom : top, offset + top.length);
  return data;
}
function renderer(format, options = {}) {
  const data = options.data || (format === 'jpeg' ? Buffer.alloc(513, 17) : bitmap(options.bits, options.topDown));
  const r = {opened: 0, closed: 0, decoderOpened: 0, decoderClosed: 0, deviceRequires: 0,
    reads: [], mcus: [], buffers: [], logs: [], timers: new Map(), turn: 0};
  let next = 0;
  const fakeFs = {
    statSync() {return {size: options.size === undefined ? data.length : options.size};},
    openSync() {r.opened++; return r.opened;},
    closeSync() {r.closed++; if (options.failFileClose) throw Error('file close failure');},
    readSync(fd, buffer, offset, length, position) {
      r.reads.push({turn: r.turn, length, position, buffer});
      if (options.failRead === r.reads.length) throw Error('read failure');
      const n = Math.max(0, Math.min(length, data.length - position));
      buffer.set(data.subarray(position, position + n), offset); return n;
    }
  };
  r.show = load('examples/portable/' + (format === 'jpeg' ? 'jpeg' : 'sd-bmp') + '/show.js', name => {
    if (name === 'fs') return fakeFs;
    if (name === 'devices') {r.deviceRequires++; throw Error('borrowed renderer must not acquire display');}
    assert.equal(name, 'jpeg');
    return {open(input) {
      assert.deepEqual(Buffer.from(input), data); assert.equal(r.closed, r.opened);
      if (options.failDecoderOpen) throw Error('decoder open failure');
      r.decoderOpened++; let index = 0;
      const blocks = options.blocks || [
        {x: 0, y: 0, width: 3, height: 1, data: [255,0,0,0,255,0,0,255,0]},
        {x: 0, y: 1, width: 3, height: 1, data: [0,0,255,255,255,255,132,130,132]}];
      return {width: options.imageWidth || 3, height: options.imageHeight || 2,
        read(buffer) {
          assert.equal(buffer.length, 768); r.mcus.push({turn: r.turn, buffer});
          if (options.failDecode === index) throw Error('decode failure');
          const b = blocks[index++]; if (!b) return null;
          buffer.set(b.data); return b;
        }, close() {r.decoderClosed++; if (options.failDecoderClose) throw Error('decoder close failure');}};
    }};
  }, {console: {log(line) {r.logs.push(line);}},
    Uint8Array: function (n) {r.buffers.push(n); return new Uint8Array(n);},
    setTimeout(fn, delay) {
      if (++next === options.failTimer) throw Error('timer failure');
      r.timers.set(next, {fn, delay}); return next;
    }, clearTimeout(id) {r.timers.delete(id); if (options.failClear) throw Error('cancel failure');}});
  r.step = function (delay = 0) {
    const entry = [...r.timers].find(([, t]) => t.delay === delay);
    if (!entry) return false;
    r.timers.delete(entry[0]); r.turn++; entry[1].fn(); return true;
  };
  r.finish = function () {let n = 0; while (r.step()) assert.ok(++n < 1000);};
  return r;
}
const runs = [['#ff0000',1,1,1,1], ['#00ff00',2,1,2,1], ['#0000ff',1,2,1,1], ['#ffffff',2,2,1,1], ['#848284',3,2,1,1]];
function clean(r, t) {
  assert.equal(r.closed, r.opened); assert.equal(r.decoderClosed, r.decoderOpened);
  assert.equal(r.timers.size, 0); assert.equal(r.deviceRequires, 0);
  assert.equal(t.opened, 1); assert.equal(t.closed, 0); assert.equal(t.presented, 0);
}
for (const format of ['bmp', 'jpeg']) test(format + ' draw layers exact runs on caller Canvas and finishes without ownership effects', () => {
  const r = renderer(format), t = target();
  const h = r.show.draw('/image.bmp', t.canvas, 1, 1);
  assert.equal(h.status(), 'loading'); assert.deepEqual(t.draws, []);
  r.finish(); assert.equal(h.status(), 'ready'); assert.deepEqual(t.draws, runs);
  assert.deepEqual(t.pixels, [Array(5).fill('#123456'),
    ['#123456','#ff0000','#00ff00','#00ff00','#123456'],
    ['#123456','#0000ff','#ffffff','#848284','#123456'], Array(5).fill('#123456')]);
  clean(r, t); h.close(); h.close(); assert.equal(h.status(), 'closed'); clean(r, t);
});

for (const format of ['bmp', 'jpeg']) {
  test(format + ' delegates negative edges and larger-than-target images without rotation', () => {
    const cases = [
      [-1,-1, [['#ffffff','#848284']]],
      [0,0, [['#ff0000','#00ff00']]],
      [1,0, [['#123456','#ff0000']]],
      [-2,0, [['#00ff00','#123456']]],
      [0,1, [['#123456','#123456']]],
      [2147483647,2147483647, [['#123456','#123456']]],
      [-2147483648,-2147483648, [['#123456','#123456']]]
    ];
    for (const [x,y,expected] of cases) {
      const r = renderer(format), t = target(2,1);
      const h = r.show.draw('/image', t.canvas, x, y); r.finish();
      assert.equal(h.status(), 'ready'); assert.deepEqual(t.pixels, expected);
      assert.deepEqual(t.draws, runs.map(([c,rx,ry,w,h]) => [c,rx-1+x,ry-1+y,w,h]));
      clean(r,t);
    }
  });
  test(format + ' two sequential layers retain background and the first image', () => {
    const r = renderer(format), t = target(5,3);
    const first = r.show.draw('/first', t.canvas, 0, 0); r.finish();
    assert.equal(first.status(), 'ready');
    const second = r.show.draw('/second', t.canvas, 2, 1); r.finish();
    assert.equal(second.status(), 'ready');
    assert.deepEqual(t.pixels, [
      ['#ff0000','#00ff00','#00ff00','#123456','#123456'],
      ['#0000ff','#ffffff','#ff0000','#00ff00','#00ff00'],
      ['#123456','#123456','#0000ff','#ffffff','#848284']]);
    first.close(); second.close(); clean(r,t);
  });
  test(format + ' invalid Canvas and explicit signed32 coordinates fail before acquisition', () => {
    const r = renderer(format), t = target();
    for (const bad of [undefined,null,NaN,Infinity,-Infinity,1.5,'1',true,{},2147483648,-2147483649,Symbol('x'),1n]) {
      assert.throws(() => r.show.draw('/bad',t.canvas,bad,0));
      assert.throws(() => r.show.draw('/bad',t.canvas,0,bad));
    }
    const shape = {width: 5,height: 4,getContext() {throw Error('context failure');}};
    for (const bad of [undefined,null,{}, {...shape,width:0}, {...shape,height:NaN},
      {...shape,width:Infinity}, {...shape,height:1.5}, {...shape,width:'5'},
      {...shape,getContext:0}, {...shape,getContext() {return null;}},
      {...shape,getContext() {return {};}},shape]) assert.throws(() => r.show.draw('/bad',bad,0,0));
    assert.throws(() => r.show.draw('/bad',t.canvas));
    assert.equal(r.opened,0); assert.equal(r.decoderOpened,0); assert.deepEqual(t.draws,[]); clean(r,t);
  });
  test(format + ' cancellation releases only own resources, even if queued callbacks run late', () => {
    for (const turns of (format === 'jpeg' ? [0,1,4,5] : [0,1])) {
      const r = renderer(format), t = target();
      const h = r.show.draw('/image',t.canvas,0,0);
      for (let n=0;n<turns;n++) assert.equal(r.step(),true);
      const late = [...r.timers.values()], before = t.draws.length;
      h.close(); h.close(); late.forEach(t => t.fn()); r.finish();
      assert.equal(h.status(),'closed'); assert.equal(t.draws.length,before);
      assert.ok(!r.logs.some(s=>s.includes('_READY'))); clean(r,t);
    }
  });
  test(format + ' read/draw/close/scheduling errors never acquire or close caller display', () => {
    const faults = [{failRead:1},{failRead:2},{failFileClose:true},
      ...Array.from({length:format === 'jpeg' ? 8 : 2},(_,i)=>({failTimer:i+1})),{failDraw:true}];
    if (format === 'jpeg') faults.push({failDecoderOpen:true},{failDecode:0},{failDecode:1},{failDecoderClose:true});
    for (const fault of faults) {
      const r = renderer(format,fault), t = target(); t.failDraw = fault.failDraw;
      let h, error;
      try {h = r.show.draw('/image',t.canvas,0,0); r.finish();} catch (e) {error=e;}
      assert.ok(error || h.status()==='error', JSON.stringify(fault));
      assert.ok(!r.logs.some(s=>s.includes('_READY')), JSON.stringify(fault)); clean(r,t);
      if (h) {h.close(); clean(r,t);}
    }
  });
  test(format + ' throwing timer cancellation still independently attempts resource cleanup', () => {
    for (const turns of (format === 'jpeg' ? [0,4] : [0])) {
      const r = renderer(format,{failClear:true}), t = target();
      const h = r.show.draw('/image',t.canvas,0,0);
      for (let n=0;n<turns;n++) r.step();
      assert.throws(()=>h.close(),/cancel failure/); clean(r,t);
      h.close(); clean(r,t);
    }
  });
}
for (const bits of [16,24,32]) for (const topDown of [false,true]) {
  test('BMP ' + bits + ' bits topDown=' + topDown + ' retains exact colors and one reusable row per turn', () => {
    const r = renderer('bmp',{bits,topDown}), t = target();
    const h = r.show.draw('/image',t.canvas,1,1); r.finish();
    assert.equal(h.status(),'ready'); assert.deepEqual(t.draws,runs);
    const rows = r.reads.filter(x=>x.turn>0);
    assert.equal(rows.length,2); assert.equal(new Set(rows.map(x=>x.turn)).size,2);
    assert.equal(new Set(rows.map(x=>x.buffer)).size,1);
    assert.deepEqual(r.buffers,bits===16 ? [54,12,8] : [54,12]); clean(r,t);
  });
}
test('JPEG keeps bounded input and MCU turns, then cancels its 120s render deadline', () => {
  const r = renderer('jpeg',{data:Buffer.alloc(16384,17)}), t = target();
  const h = r.show.draw('/image',t.canvas,1,1);
  assert.deepEqual([...r.timers.values()].map(t=>t.delay),[120000,0]);
  r.finish(); assert.equal(h.status(),'ready');
  assert.deepEqual(r.buffers,[16384,768]); assert.equal(r.reads.length,65);
  assert.ok(r.reads.every(x=>x.length<=256));
  assert.equal(new Set(r.reads.map(x=>x.turn)).size,r.reads.length);
  assert.equal(new Set(r.mcus.map(x=>x.turn)).size,r.mcus.length);
  assert.equal(new Set(r.mcus.map(x=>x.buffer)).size,1); clean(r,t);
});
test('JPEG deadline errors while loading or decoding without affecting the caller', () => {
  for (const turns of [0,4,5]) {
    const r=renderer('jpeg'), t=target(), h=r.show.draw('/image',t.canvas,0,0);
    for(let n=0;n<turns;n++) r.step();
    assert.equal(r.step(120000),true); assert.equal(h.status(),'error');
    assert.ok(r.logs.includes('JPEG_ERROR JPEG timed out')); clean(r,t);
  }
});
test('JPEG non-origin partial MCUs retain source coordinates in borrowed mode', () => {
  const r=renderer('jpeg',{imageWidth:4,imageHeight:3,blocks:[
    {x:2,y:1,width:2,height:2,data:[255,0,0,0,255,0,0,0,255,255,255,255]},
    {x:0,y:0,width:1,height:1,data:[132,130,132]}]}), t=target(3,2);
  const h=r.show.draw('/image',t.canvas,-1,-1); r.finish(); assert.equal(h.status(),'ready');
  assert.deepEqual(t.draws,[['#ff0000',1,0,1,1],['#00ff00',2,0,1,1],
    ['#0000ff',1,1,1,1],['#ffffff',2,1,1,1],['#848284',-1,-1,1,1]]);
  assert.deepEqual(t.pixels,[['#123456','#ff0000','#00ff00'],['#123456','#0000ff','#ffffff']]); clean(r,t);
});
test('borrowed decoders retain malformed/truncated input rejection and input ceilings', () => {
  const badBmp=bitmap(); badBmp.writeInt32LE(641,18);
  for(const [format,options] of [
    ['jpeg',{size:0}],['jpeg',{size:16385}],['jpeg',{size:512}],['jpeg',{size:514}],
    ['bmp',{data:badBmp}],['bmp',{data:bitmap().subarray(0,60)}]]) {
    const r=renderer(format,options), t=target(); let h,error;
    try {h=r.show.draw('/bad',t.canvas,0,0);r.finish();} catch(e) {error=e;}
    assert.ok(error || h.status()==='error'); assert.ok(!r.logs.some(s=>s.includes('_READY'))); clean(r,t);
  }
});

for (const format of ['bmp','jpeg']) test(format+' borrowed completion marker does not claim presentation',()=>{
 const r=renderer(format),t=target();const h=r.show.draw('/image',t.canvas,0,0);r.finish();
 assert.ok(r.logs.some(line=>line.startsWith(format.toUpperCase()+'_DRAW_READY ')));
 assert.ok(!r.logs.some(line=>line.startsWith(format.toUpperCase()+'_READY ')));clean(r,t);h.close();
});
