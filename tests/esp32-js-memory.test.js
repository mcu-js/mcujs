const { test } = require('node:test');
const assert = require('node:assert/strict');
const { mkdtempSync, mkdirSync, writeFileSync, rmSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join, resolve } = require('node:path');
const { spawnSync } = require('node:child_process');
const root = resolve(__dirname, '..');

function configure(board, declaration) {
  const tmp = mkdtempSync(join(tmpdir(), 'mcujs-js-memory-'));
  try {
    const source = declaration === undefined ? root : tmp;
    if (declaration !== undefined) {
      mkdirSync(join(tmp, 'board', board), { recursive: true });
      writeFileSync(join(tmp, 'board', board, 'board_config.cmake'), declaration);
    }
    const script = join(tmp, 'test.cmake');
    writeFileSync(script, `set(MCUJS_ROOT "${source}")\nset(MCUJS_BOARD "${board}")\ninclude("${root}/platform/esp32/js-memory.cmake")\n` +
      'message(STATUS "RESULT=${MCUJS_JS_HEAP_KIB},${MCUJS_JS_HEAP_EXTERNAL},${MCUJS_JERRY_EXTERNAL_CONTEXT}")\n');
    return spawnSync('cmake', ['-P', script], { encoding: 'utf8' });
  } finally { rmSync(tmp, { recursive: true, force: true }); }
}

for (const board of ['seeed_reterminal_sticky', 'waveshare_esp32s3_epaper_1.54_v2', 'seeed_xiao_esp32s3']) {
  test(`${board} declares a 256KiB external JS heap`, () => {
    const result = configure(board);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /RESULT=256,1,ON/);
  });
}

test('internal heaps select JerryScript static allocation', () => {
  const result = configure('test', 'set(MCUJS_JS_HEAP_KIB 64)\nset(MCUJS_JS_HEAP_REGION internal)\n');
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /RESULT=64,0,OFF/);
});

test('invalid or missing memory policies fail configuration', () => {
  for (const [size, region] of [['0', 'external'], ['-1', 'external'], ['1.5', 'external'], ['513', 'external'], ['256', 'automatic'], ['', 'internal']]) {
    const result = configure('test', `set(MCUJS_JS_HEAP_KIB "${size}")\nset(MCUJS_JS_HEAP_REGION "${region}")\n`);
    assert.notEqual(result.status, 0, `accepted ${size}/${region}`);
    assert.match(result.stderr, /JS heap/);
  }
});

test('the 16-bit-pointer heap boundary remains explicit', () => {
  const result = configure('test', 'set(MCUJS_JS_HEAP_KIB 512)\nset(MCUJS_JS_HEAP_REGION external)\n');
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /RESULT=512,1,ON/);
});
