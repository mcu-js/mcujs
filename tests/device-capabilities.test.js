'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { mkdtempSync, writeFileSync, readFileSync, rmSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join, resolve } = require('node:path');
const { execFileSync } = require('node:child_process');
const { manifestFor, configuredDeviceCapabilities } = require('../runtime/board-registry');
const root = resolve(__dirname, '..');

test('device metadata reports explicit build support, not physical display inventory', () => {
  assert.ok(manifestFor('seeed_reterminal_sticky').board.devices.display);
  assert.equal(manifestFor('seeed_reterminal_sticky').capabilities.devices, undefined);
  const enabled = manifestFor('seeed_reterminal_sticky', { configuredDisplay: true });
  assert.deepEqual(enabled.capabilities.devices, { display: { interface: 'canvas-2d-subset', maxOpenHandles: 1 } });
  assert.deepEqual(enabled.capabilities.devices, configuredDeviceCapabilities);
  assert.equal(manifestFor('seeed_reterminal_sticky').capabilities.devices, undefined);
});

test('compiled registry, manifest and device discovery agree for enabled and disabled profiles', () => {
  const dir = mkdtempSync(join(tmpdir(), 'mcujs-device-registry-'));
  try {
    const source = join(dir, 'dump.c');
    writeFileSync(source, '#include "runtime_registry.h"\n#include <stdio.h>\nint main(void){puts(mcujs_runtime_registry()->manifest_json); printf("%d %d\\n",mcujs_runtime_has_module("devices"),mcujs_runtime_find_capability("devices")!=NULL);return 0;}\n');
    for (const [board, macro, profile] of [
      ['seeed_reterminal_sticky', 'MCUJS_BOARD_SEEED_RETERMINAL_STICKY', 'MCUJS_CANVAS_STICKY'],
      ['waveshare_rp2350_lcd_1.47_a', 'MCUJS_BOARD_WAVESHARE_RP2350_LCD_1_47_A', 'MCUJS_CANVAS_DEFAULT_ST7789'],
      ['pico', 'MCUJS_BOARD_PICO', null],
    ]) {
      for (const enabled of [false, true]) {
        const args = ['-std=c11', '-I' + join(root, 'host'), '-D' + macro, source, join(root, 'host/runtime_registry.c'), '-o', join(dir, 'dump')];
        if (enabled) { args.push('-DMCUJS_EXPERIMENTAL_CANVAS=1'); if (profile) args.push('-D' + profile + '=1'); }
        execFileSync(process.env.CC || 'cc', args);
        const [json, flags] = execFileSync(join(dir, 'dump'), { encoding: 'utf8' }).trim().split('\n');
        const configured = enabled && !!profile;
        assert.deepEqual(JSON.parse(json), manifestFor(board, { configuredDisplay: configured }));
        assert.equal(flags, configured ? '1 1' : '1 0');
        const output = join(dir, 'build.capabilities.json');
        execFileSync('python3', [join(root, 'scripts/build-capability-manifest.py'), '--board', board, '--output', output, ...(configured ? ['--configured-display'] : [])]);
        assert.deepEqual(JSON.parse(readFileSync(output, 'utf8')), JSON.parse(json));
      }
    }
  } finally { rmSync(dir, { recursive: true, force: true }); }
});


test('ESP requirement discovery does not emit a target capability manifest', () => {
  const dir = mkdtempSync(join(tmpdir(), 'mcujs-esp-requirements-'));
  try {
    const fakeJerry = join(dir, 'jerry');
    require('node:fs').mkdirSync(join(fakeJerry, 'tools'), { recursive: true });
    writeFileSync(join(fakeJerry, 'tools/build.py'), '# existence fixture only\n');
    const script = join(dir, 'requirements.cmake');
    writeFileSync(script, `set(CMAKE_CROSSCOMPILING FALSE)
macro(idf_component_register)
  return()
endmacro()
include("${join(root, 'platform/esp32/main/CMakeLists.txt')}")
`);
    execFileSync('cmake', ['-P', script], { cwd: dir, env: { ...process.env, JERRYSCRIPT_PATH: fakeJerry } });
  } finally { rmSync(dir, { recursive: true, force: true }); }
});
