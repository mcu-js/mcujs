const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { createRequire } = require('node:module');
const { join } = require('node:path');
const { runInNewContext } = require('node:vm');
const test = require('node:test');
const docsRequire = createRequire(join(__dirname, '../docs/package.json'));
const yaml = docsRequire('js-yaml');
const workflow = name => yaml.load(readFileSync(join(__dirname, '../.github/workflows', name), 'utf8'));

test('development pushes and pull requests run source checks and docs builds', () => {
  for (const name of ['ci.yml', 'docs.yml']) {
    const w = workflow(name);
    for (const event of ['push', 'pull_request']) {
      assert.ok(w.on[event].branches.includes('development'), `${name}: ${event}`);
      assert.equal(w.on[event].paths, undefined, `${name}: checks should not depend on changed paths`);
    }
    assert.deepEqual(w.permissions, { contents: 'read' });
  }
});

test('Pages artifacts and deployment are limited to release-branch pushes', () => {
  const w = workflow('docs.yml');
  const upload = w.jobs.build.steps.find(s => s.uses?.startsWith('actions/upload-pages-artifact@'));
  assert.ok(upload);
  for (const gate of [upload.if, w.jobs.deploy.if]) {
    assert.equal(typeof gate, 'string');
    for (const [event_name, ref, allowed] of [
      ['push', 'refs/heads/development', false],
      ['pull_request', 'refs/heads/development', false],
      ['push', 'refs/heads/main', true],
      ['push', 'refs/heads/master', true],
      ['pull_request', 'refs/heads/main', false],
      ['push', 'refs/tags/v0.2.0', false],
    ]) {
      assert.equal(runInNewContext(gate, { github: { event_name, ref } }, { timeout: 100 }), allowed, `${event_name} ${ref}`);
    }
  }
  assert.deepEqual(w.jobs.deploy.permissions, { pages: 'write', 'id-token': 'write' });
  assert.match(w.concurrency.group, /github\.ref/);
});

test('development pushes cannot trigger the release workflow', () => {
  const w = workflow('release.yml');
  assert.equal(w.on.push.branches, undefined);
  assert.equal(w.on.push['branches-ignore'], undefined);
  assert.deepEqual(w.on.push.tags, ['v*']);
});
