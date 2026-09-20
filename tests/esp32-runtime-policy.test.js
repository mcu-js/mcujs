const assert = require('node:assert/strict');
const { readFileSync, mkdtempSync, writeFileSync, rmSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join } = require('node:path');
const { spawnSync } = require('node:child_process');
const test = require('node:test');
const root = join(__dirname, '..');
const defaults = readFileSync(join(root, 'platform/esp32/sdkconfig.defaults'), 'utf8');
const build = readFileSync(join(root, 'platform/esp32/build.sh'), 'utf8');
// Exercise the actual build gate without a toolchain, not a copy of its rules.
const validator = build.slice(build.indexOf('validate_runtime_config() {'),
  build.indexOf('validate_app_flash_metadata() {'));

test('ESP default scheduler provides a 1ms USB service quantum', () => {
  assert.match(defaults, /^CONFIG_FREERTOS_HZ=1000$/m);
});
for (const [hz, accepted] of [[1000, true], [100, false], [undefined, false]]) {
  test(`ESP build gate ${accepted ? 'accepts' : 'rejects'} scheduler Hz=${hz}`, () => {
    const dir = mkdtempSync(join(tmpdir(), 'mcujs-esp-policy-'));
    try {
      const config = defaults.split('\n').filter(line => !line.startsWith('CONFIG_FREERTOS_HZ='));
      // Values resolved by pinned IDF from the defaults above.
      config.push('# CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG is not set',
        'CONFIG_ESP_CONSOLE_SECONDARY_NONE=y', 'CONFIG_ESP_TASK_WDT_EN=y',
        'CONFIG_ESP_TASK_WDT_INIT=y', 'CONFIG_COMPILER_HIDE_PATHS_MACROS=y');
      if (hz !== undefined) config.push(`CONFIG_FREERTOS_HZ=${hz}`);
      writeFileSync(join(dir, 'sdkconfig'), config.join('\n') + '\n');
      const p = spawnSync('bash', ['-euc', validator + '\nvalidate_runtime_config'], {
        encoding: 'utf8', env: { ...process.env, BUILD_DIR: dir, MCUJS_BOARD: 'waveshare_esp32s3_epaper_1.54_v2' },
      });
      if (accepted) assert.equal(p.status, 0, p.stderr);
      else {
        assert.notEqual(p.status, 0, 'Slow/missing scheduler setting must not ship');
        assert.match(p.stderr, /CONFIG_FREERTOS_HZ=1000/);
      }
    } finally { rmSync(dir, { recursive: true, force: true }); }
  });
}
