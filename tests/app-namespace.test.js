'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const read = p => fs.readFileSync(path.join(root, p), 'utf8');
test('both boot paths select only /app/index.js while retaining safe-mode gates', () => {
  assert.match(read('src/boot.h'), /#define BOOT_INDEX_FILE "\/app\/index\.js"/);
  const rp = read('platform/rp2/boot.c');
  assert.ok(rp.indexOf('if (boot_check_safe_mode())') < rp.indexOf('if (!boot_file_exists())'));
  const esp = read('platform/esp32/main/boot.c');
  assert.match(esp, /#define MCUJS_BOOT_INDEX "\/app\/index\.js"/);
  assert.match(esp, /if \(s_safe_mode\)/);
  assert.doesNotMatch(rp + esp, /fs_exists\("\/index\.js"\)/);
});
test('configured filesystem capabilities advertise the internal application root', () => {
  const registry = require('../runtime/board-registry.js');
  const boards = Object.values(registry.boardDescriptors);
  assert.ok(boards.length > 0);
  for (const board of boards) assert.equal(board.capabilities.fs.appRoot, '/app');
});
test('REPL default listing uses the application volume', () => {
  assert.match(read('src/repl.c'), /fs_list_dir\(FS_APP_ROOT, repl_ls_callback, NULL\)/);
});
