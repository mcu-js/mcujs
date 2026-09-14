const test = require('node:test');
const assert = require('node:assert/strict');
const { cpSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { tmpdir } = require('node:os');
const { spawnSync } = require('node:child_process');
const root = resolve(__dirname, '..');

test('hardware helper selects the artifact for the current source version', async () => {
  const { uf2Path } = await import('./helpers/device.js');
  const version = readFileSync(join(root, 'version.txt'), 'utf8').trim();
  assert.equal(uf2Path(), join(root, 'build', `mcujs-${version}-pico.uf2`));
});

test('RP project configuration accepts RC and stable versions without losing firmware identity', () => {
  const source = readFileSync(join(root, 'CMakeLists.txt'), 'utf8');
  const setup = source.split('# Board selection')[0];
  const project = source.match(/^project\(mcujs VERSION .*$/m)[0]
    .replace('LANGUAGES C CXX ASM', 'LANGUAGES NONE');
  for (const version of ['0.2.0-rc.1', '0.2.0']) {
    const dir = mkdtempSync(join(tmpdir(), 'mcujs-cmake-version-'));
    try {
      writeFileSync(join(dir, 'version.txt'), version + '\n');
      writeFileSync(join(dir, 'CMakeLists.txt'), setup + project + '\n' +
        'file(WRITE "${CMAKE_BINARY_DIR}/identity.txt" "${PROJECT_VERSION};${MCUJS_VERSION}")\n');
      const result = spawnSync('cmake', ['-S', dir, '-B', join(dir, 'build')], { encoding: 'utf8' });
      assert.equal(result.status, 0, result.stdout + result.stderr);
      assert.equal(readFileSync(join(dir, 'build/identity.txt'), 'utf8'), `0.2.0;${version}`);
    } finally { rmSync(dir, { recursive: true, force: true }); }
  }
});

// Run the production board gate with its normal setup in a disposable fixture.
// Later release stages (native compilers/docs) are tested by verify-release itself.
function check(change) {
  const dir = mkdtempSync(join(tmpdir(), 'mcujs-board-gate-'));
  try {
    for (const name of ['board', 'runtime', 'scripts/lib', 'version.txt']) {
      cpSync(join(root, name), join(dir, name), { recursive: true });
    }
    const source = readFileSync(join(root, 'scripts/verify-release.sh'), 'utf8');
    assert.equal(source.split('\ncheck_clean_tree\n').length, 2);
    writeFileSync(join(dir, 'scripts/verify-release.sh'), source.split('\ncheck_clean_tree\n')[0] + '\ncheck_board_registry\n');
    if (change) change(dir);
    return spawnSync('bash', [join(dir, 'scripts/verify-release.sh')], { encoding: 'utf8' });
  } finally { rmSync(dir, { recursive: true, force: true }); }
}

test('release board gate accepts declared RP and ESP profiles, including experimental boards', () => {
  const result = check();
  assert.equal(result.status, 0, result.stdout + result.stderr);
});
test('release board gate still rejects missing RP, missing ESP and unknown profiles', () => {
  for (const name of ['pico', 'seeed_xiao_esp32s3', 'seeed_reterminal_sticky']) {
    const result = check(dir => rmSync(join(dir, 'board', name, 'board_config.cmake')));
    assert.notEqual(result.status, 0, `Accepted missing ${name}`);
  }
  const result = check(dir => {
    mkdirSync(join(dir, 'board/unknown'));
    writeFileSync(join(dir, 'board/unknown/board_config.cmake'), '');
  });
  assert.notEqual(result.status, 0);
  assert.match(result.stdout + result.stderr, /Board registry drift/);
});
