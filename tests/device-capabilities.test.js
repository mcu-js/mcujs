'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { mkdtempSync, writeFileSync, readFileSync, rmSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join, resolve } = require('node:path');
const { execFileSync } = require('node:child_process');
const { boardDescriptors, manifestFor, configuredDeviceCapabilities } = require('../runtime/board-registry');
const root = resolve(__dirname, '..');

test('only compiled managed button profiles advertise read-only events', () => {
  for (const [id, descriptor] of Object.entries(boardDescriptors)) {
    assert.equal(descriptor.modules.includes('mcujs:button'), Boolean(descriptor.capabilities.devices?.button), id);
  }
  for (const board of ['pico', 'seeed_xiao_esp32s3']) {
    const value = manifestFor(board).capabilities.devices.button;
    assert.deepEqual(value, { interface: 'button-events', readOnly: true, maxOpenHandles: 1, pollIntervalMs: 10, debounceMs: 30 });
    assert.deepEqual(manifestFor(board, { configuredDisplay: true }).capabilities.devices.button, value);
  }
  for (const board of ['pico2', 'seeed_reterminal_sticky', 'waveshare_rp2350_lcd_1.47_a'])
    assert.equal(manifestFor(board).capabilities.devices?.button, undefined);
});

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
    writeFileSync(source, '#include "runtime_registry.h"\n#include <stdio.h>\nint main(void){const mcujs_runtime_registry_t *r=mcujs_runtime_registry();puts(r->manifest_json); printf("%d %d\\n",mcujs_runtime_has_module("devices"),mcujs_runtime_find_capability("devices")!=NULL);for(size_t i=0;i<r->builtin_module_count;i++)puts(r->builtin_modules[i]);return 0;}\n');
    const displayProfiles = {
      seeed_reterminal_sticky: 'MCUJS_CANVAS_STICKY',
      'waveshare_esp32s3_epaper_1.54_v2': 'MCUJS_CANVAS_EPAPER154',
      waveshare_rp2040_pizero: 'MCUJS_CANVAS_DVI',
      'waveshare_rp2350_lcd_1.47_a': 'MCUJS_CANVAS_DEFAULT_ST7789',
      'waveshare_rp2350_touch_lcd_1.69': 'MCUJS_CANVAS_DEFAULT_ST7789',
      'waveshare_rp2350_touch_lcd_2.8': 'MCUJS_CANVAS_DEFAULT_ST7789',
    };
    for (const board of Object.keys(boardDescriptors)) {
      const macro = 'MCUJS_BOARD_' + board.toUpperCase().replaceAll('.', '_');
      const profile = displayProfiles[board];
      for (const enabled of [false, true]) {
        const args = ['-std=c11', '-I' + join(root, 'host'), '-D' + macro, source, join(root, 'host/runtime_registry.c'), '-o', join(dir, 'dump')];
        if (enabled) { args.push('-DMCUJS_EXPERIMENTAL_CANVAS=1'); if (profile) args.push('-D' + profile + '=1'); }
        execFileSync(process.env.CC || 'cc', args);
        const [json, flags, ...modules] = execFileSync(join(dir, 'dump'), { encoding: 'utf8' }).trim().split('\n');
        const configured = enabled && !!profile;
        const manifest = manifestFor(board, { configuredDisplay: configured });
        assert.deepEqual(JSON.parse(json), manifest, board);
        assert.equal(flags, Object.hasOwn(manifest.capabilities, 'devices') ? '1 1' : '1 0', board);
        const expectedModules = [...boardDescriptors[board].modules];
        if (enabled) {
          expectedModules.push('mcujs:canvas-native', 'canvas');
          if (!['MCUJS_CANVAS_EPAPER154', 'MCUJS_CANVAS_STICKY'].includes(profile))
            expectedModules.push('displays/st7789');
        }
        assert.deepEqual(modules.sort(), expectedModules.sort(), `${board}: actual compiled module list`);
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
