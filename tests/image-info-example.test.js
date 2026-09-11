'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

// Literal header-only fixture: no SOS or entropy data is needed for metadata.
const JPEG = [255, 216, 255, 192, 0, 11, 8, 0, 9, 0, 17, 1, 1, 17, 0];
function harness(bytes, options = {}) {
  const state = { reads: [], closes: 0, opened: false, allocations: 0 };
  const data = Uint8Array.from(bytes);
  const fakeFs = {
    openSync(file, flags) {
      assert.equal(file, options.path || '/app/photo.jpg');
      assert.equal(flags, 'r');
      if (options.openError) throw new Error('open failed');
      state.opened = true;
      return 7;
    },
    statSync(file) {
      assert.equal(file, options.path || '/app/photo.jpg');
      if (options.statError) throw new Error('stat failed');
      return { size: options.size === undefined ? data.length : options.size };
    },
    readSync(fd, buffer, offset, length, position) {
      assert.equal(fd, 7);
      assert.ok(state.opened);
      assert.ok(buffer instanceof Uint8Array);
      assert.ok(buffer.length <= 128);
      assert.ok(Number.isInteger(position) && position >= 0);
      assert.ok(offset >= 0 && offset + length <= buffer.length);
      state.reads.push({ buffer, offset, length, position });
      assert.ok(state.reads.length <= 128, 'hard read-call budget');
      if (options.readError) throw new Error('read failed');
      if (options.zeroRead) return 0;
      const n = Math.min(length, options.chunk || length, data.length - position);
      if (n <= 0) return 0;
      buffer.set(data.subarray(position, position + n), offset);
      return n;
    },
    closeSync(fd) {
      assert.equal(fd, 7);
      assert.ok(state.opened);
      state.closes++;
      state.opened = false;
      if (options.closeError) throw new Error('close failed');
    }
  };
  function ByteArray(length) {
    state.allocations++;
    assert.ok(length <= 128);
    // Nonzero underlying byteOffset exposes accidental backing-buffer indexing.
    return new Uint8Array(new ArrayBuffer(length + 13), 7, length);
  }
  const sandbox = {
    exports: {}, Uint8Array: ByteArray,
    require(name) {
      assert.equal(name, 'fs', 'metadata must not acquire a display or decoder');
      return fakeFs;
    }
  };
  sandbox.module = { exports: sandbox.exports };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,
    '../examples/portable/images/info.js'), 'utf8'), sandbox);
  function run() {
    try {
      return JSON.parse(JSON.stringify(sandbox.module.exports(options.path || '/app/photo.jpg')));
    } finally {
      assert.equal(state.opened, false, 'no leaked descriptor');
      assert.equal(state.closes, options.openError ? 0 : 1, 'close exactly once');
      assert.ok(state.allocations <= 1, 'one bounded byte buffer');
      assert.ok(new Set(state.reads.map(read => read.buffer)).size <= 1);
    }
  }
  return { run, state };
}

test('baseline JPEG dimensions are header metadata, without a decoder', () => {
  assert.deepEqual(harness(JPEG).run(), { format: 'jpeg', width: 17, height: 9 });
});

const BMP = [
  66, 77, 54, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
  40, 0, 0, 0, 17, 0, 0, 0, 9, 0, 0, 0, 1, 0, 24, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
];
function changed(bytes, offset, values) {
  const copy = bytes.slice();
  copy.splice(offset, values.length, ...values);
  return copy;
}
function rejects(name, bytes, options = {}) {
  // Do not let an AssertionError from the fs/resource contract masquerade as
  // a successful malformed-input rejection.
  test(name, () => assert.throws(harness(bytes, options).run,
    error => error.name === 'Error'));
}
rejects('short physical SOF despite larger stat size', JPEG.slice(0, -1), { size: JPEG.length });
rejects('short physical BMP despite larger stat size', BMP.slice(0, -1), { size: BMP.length });

test('progressive SOF2 also describes dimensions', () => {
  assert.deepEqual(harness(changed(JPEG, 3, [194])).run(),
    { format: 'jpeg', width: 17, height: 9 });
});
test('APP payload containing fake SOF is skipped, not scanned', () => {
  const bytes = [255, 216, 255, 225, 0, 15,
    255, 192, 0, 11, 8, 0, 1, 0, 2, 1, 1, 17, 0, ...JPEG.slice(2)];
  const h = harness(bytes);
  assert.deepEqual(h.run(), { format: 'jpeg', width: 17, height: 9 });
  assert.ok(h.state.reads.every(read => read.position < 6 || read.position >= 19));
});
test('four distinct SOF components are valid header metadata', () => {
  const bytes = [255, 216, 255, 194, 0, 20, 8, 0, 9, 0, 17, 4,
    1, 17, 0, 2, 33, 1, 3, 18, 1, 4, 17, 0];
  assert.deepEqual(harness(bytes).run(), { format: 'jpeg', width: 17, height: 9 });
});
test('marker fill bytes within budget are accepted', () => {
  assert.deepEqual(harness([255, 216, 255, ...JPEG.slice(2)]).run(),
    { format: 'jpeg', width: 17, height: 9 });
});
test('SOF may end exactly at the 16384-byte scan boundary', () => {
  const payloadLength = 16384 - JPEG.length - 4;
  const segmentLength = payloadLength + 2;
  const bytes = [255, 216, 255, 225, segmentLength >> 8, segmentLength & 255,
    ...new Array(payloadLength).fill(0), ...JPEG.slice(2)];
  const h = harness(bytes);
  assert.deepEqual(h.run(), { format: 'jpeg', width: 17, height: 9 });
  assert.ok(h.state.reads.every(read => read.position + read.length <= 16384));
  assert.ok(h.state.reads.reduce((n, read) => n + read.length, 0) < 128);
  bytes.splice(6, 0, 0);
  bytes[4] = (segmentLength + 1) >> 8;
  bytes[5] = (segmentLength + 1) & 255;
  assert.throws(harness(bytes).run);
});
for (const [name, bytes] of [
  ['unknown signature', [1, 2, 3, 4]],
  ['empty file', []],
  ['truncated SOI', [255]],
  ['SOI only', [255, 216]],
  ['missing marker prefix', changed(JPEG, 2, [0])],
  ['truncated segment length', [255, 216, 255, 225, 0]],
  ['segment length below two', [255, 216, 255, 225, 0, 1]],
  ['truncated APP payload', [255, 216, 255, 225, 0, 20, ...JPEG.slice(2)]],
  ['truncated SOF component', JPEG.slice(0, -1)],
  ['zero height', changed(JPEG, 7, [0, 0])],
  ['zero width', changed(JPEG, 9, [0, 0])],
  ['unsupported precision', changed(JPEG, 6, [12])],
  ['zero components', changed(JPEG, 11, [0])],
  ['too many components', changed(JPEG, 11, [5])],
  ['inconsistent component length', changed(JPEG, 11, [2])],
  ['zero horizontal sampling', changed(JPEG, 13, [1])],
  ['zero vertical sampling', changed(JPEG, 13, [16])],
  ['duplicate component IDs', [255, 216, 255, 192, 0, 14, 8, 0, 9, 0, 17, 2,
    1, 17, 0, 1, 17, 1]],
  ['SOF after SOS', [255, 216, 255, 218, 0, 2, ...JPEG.slice(2)]],
  ['too many marker fill bytes', [255, 216, ...new Array(300).fill(255), ...JPEG.slice(3)]],
  ['too many length-framed segments', [255, 216,
    ...new Array(50).fill([255, 225, 0, 2]).flat(), ...JPEG.slice(2)]]
]) rejects(name, bytes);
for (const marker of [0, 1, 191, 193, 208, 215, 216, 217]) {
  rejects('invalid/unsupported standalone or frame marker ' + marker,
    [255, 216, 255, marker, ...JPEG.slice(2)]);
}
rejects('huge APP skip beyond file length', [255, 216, 255, 225, 255, 255]);
rejects('huge APP skip beyond header scan limit', [255, 216, 255, 225, 255, 255], { size: 100000 });
rejects('partial reads count toward hard read budget',
  [255, 216, ...new Array(40).fill([255, 225, 0, 2]).flat(), ...JPEG.slice(2)], { chunk: 1 });

for (const dib of [40, 52, 56, 108, 124]) {
  test('BMP bounded DIB header ' + dib, () => {
    const bytes = BMP.concat(new Array(dib - 40).fill(0));
    bytes[2] = bytes[10] = 14 + dib;
    bytes[14] = dib;
    assert.deepEqual(harness(bytes).run(), { format: 'bmp', width: 17, height: 9, bpp: 24 });
  });
}
for (const bpp of [1, 4, 8, 16, 24, 32]) {
  test('BMP metadata allows plausible bpp ' + bpp + ' without promising decoding', () => {
    const bytes = changed(changed(BMP, 28, [bpp]), 30, [1]);
    assert.deepEqual(harness(bytes).run(), { format: 'bmp', width: 17, height: 9, bpp });
  });
}
test('BMP top-down height is reported as absolute height', () => {
  assert.deepEqual(harness(changed(BMP, 22, [247, 255, 255, 255])).run(),
    { format: 'bmp', width: 17, height: 9, bpp: 24 });
});
for (const [name, bytes] of [
  ['truncated file header', BMP.slice(0, 13)],
  ['truncated info header', BMP.slice(0, -1)],
  ['declared extended header truncated', changed(BMP, 14, [124])],
  ['unsupported DIB size', changed(BMP, 14, [64])],
  ['core DIB unsupported', changed(BMP, 14, [12])],
  ['huge DIB', changed(BMP, 14, [255, 255, 255, 255])],
  ['pixel offset inside header', changed(BMP, 10, [53])],
  ['pixel offset beyond file', changed(BMP, 10, [55])],
  ['unsigned huge pixel offset', changed(BMP, 10, [255, 255, 255, 255])],
  ['zero width', changed(BMP, 18, [0, 0, 0, 0])],
  ['negative width', changed(BMP, 18, [255, 255, 255, 255])],
  ['zero height', changed(BMP, 22, [0, 0, 0, 0])],
  ['zero planes', changed(BMP, 26, [0, 0])],
  ['multiple planes', changed(BMP, 26, [2, 0])],
  ['invalid bpp', changed(BMP, 28, [2, 0])]
]) rejects('BMP ' + name, bytes);

for (const [name, bytes, result] of [
  ['JPEG', JPEG, { format: 'jpeg', width: 17, height: 9 }],
  ['BMP', BMP, { format: 'bmp', width: 17, height: 9, bpp: 24 }]
]) {
  test(name + ' partial reads use explicit offsets and a nonzero-byteOffset view on /sd', () => {
    const h = harness(bytes, { chunk: 1, path: '/sd/photo' });
    assert.deepEqual(h.run(), result);
    assert.ok(h.state.reads.some(read => read.offset > 0));
  });
}
for (const [flag, message] of [
  ['openError', 'open failed'], ['statError', 'stat failed'],
  ['readError', 'read failed'], ['closeError', 'close failed']
]) {
  test(flag + ' propagates and releases any opened file exactly once', () => {
    assert.throws(harness(JPEG, { [flag]: true }).run, new RegExp(message));
  });
}
rejects('unexpected EOF during read', JPEG, { zeroRead: true });
for (const size of [-1, 1.5, Infinity, NaN, '15']) {
  rejects('invalid stat size ' + String(size), JPEG, { size });
}

test('callable helper reads the committed JPEG and all maintained BMP depths through real fs', () => {
  const info = require('../examples/portable/images/info');
  assert.equal(typeof info, 'function');
  const root = path.join(__dirname, '../examples/images');
  assert.deepEqual(info(path.join(root, 'test_172x320.jpg')), {format:'jpeg',width:172,height:320});
  for (const bpp of [16,24,32])
    assert.deepEqual(info(path.join(root, 'test_172x320_'+bpp+'bit.bmp')), {format:'bmp',width:172,height:320,bpp});
});
