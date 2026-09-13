const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const { mkdtempSync, writeFileSync, readFileSync, rmSync } = require('node:fs');
const { tmpdir } = require('node:os');
const { join } = require('node:path');
const test = require('node:test');

const canonical = 'https://mcujs.org/';
const apex = 'https://mcujs.com/';
const www = 'https://www.mcujs.com/';
const redirect = '<!doctype html><html><head><meta http-equiv="refresh" content="0; url=https://mcujs.org/"></head><body>mcujs has moved.</body></html>';

function finalize(overrides = {}) {
  const dir = mkdtempSync(join(tmpdir(), 'mcujs-pages-'));
  try {
    const responses = {
      [canonical]: { code: 200, effective: canonical, body: '<html><title>MCU.js</title></html>' },
      'https://www.mcujs.org/': { code: 200, effective: canonical },
      [apex]: { code: 200, effective: apex, body: redirect },
      [www]: { code: 200, effective: apex, body: redirect },
      ...overrides,
    };
    writeFileSync(join(dir, 'responses.json'), JSON.stringify(responses));
    writeFileSync(join(dir, 'curl'), `#!${process.execPath}
const fs = require('node:fs');
const args = process.argv.slice(2);
const url = args.at(-1);
fs.appendFileSync(process.env.CALLS, JSON.stringify({command: 'curl', args}) + '\\n');
const responses = JSON.parse(fs.readFileSync(process.env.RESPONSES, 'utf8'));
let response = responses[url];
if (Array.isArray(response)) {
  response = response.shift();
  fs.writeFileSync(process.env.RESPONSES, JSON.stringify(responses));
}
if (!response) throw new Error('Unexpected request: ' + url);
if (response.exit) process.exit(response.exit);
const output = args.indexOf('-o');
if (output !== -1) fs.writeFileSync(args[output + 1], response.body || '');
process.stdout.write(args[args.indexOf('-w') + 1]
  .replace('%{http_code}', response.code)
  .replace('%{url_effective}', response.effective)
  .replace('%{content_type}', response.contentType ?? 'text/html'));
`, { mode: 0o755 });
    writeFileSync(join(dir, 'gh'), `#!${process.execPath}
const fs = require('node:fs');
const args = process.argv.slice(2);
fs.appendFileSync(process.env.CALLS, JSON.stringify({command: 'gh', args}) + '\\n');
if (args[0] !== 'api' || !/^repos\\/mcu-js\\/mcujs(?:\\.com)?\\/pages$/.test(args[1]) || args.length !== 4 || args[2] !== '--jq') process.exit(90);
console.log(args[3] === '.https_enforced' ? 'true' : '{}');
`, { mode: 0o755 });
    const result = spawnSync('bash', ['scripts/finalize-pages-cutover.sh'], {
      cwd: join(__dirname, '..'),
      encoding: 'utf8',
      env: { ...process.env, PATH: `${dir}:${process.env.PATH}`, CALLS: join(dir, 'calls'), RESPONSES: join(dir, 'responses.json') },
    });
    const calls = readFileSync(join(dir, 'calls'), 'utf8').trim().split('\n').map(JSON.parse);
    return { ...result, calls };
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

test('dry run accepts the HTTPS .com meta-refresh and verifies its canonical destination', () => {
  const result = finalize();
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /Dry run complete/);
  assert.equal(result.calls.filter(c => c.command === 'curl' && c.args.at(-1) === canonical).length, 3);
  assert.ok(result.calls.some(c => c.command === 'curl' && c.args.at(-1) === www));
  assert.equal(result.calls.filter(c => c.command === 'gh').length, 4);
});

test('existing HTTPS HTTP-redirect results still pass without requiring HTML', () => {
  const result = finalize({
    [apex]: { code: 200, effective: canonical },
    [www]: { code: 200, effective: canonical },
  });
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.equal(result.calls.filter(c => c.command === 'curl').length, 4);
});

test('HTML attribute order, quoting and case do not change a canonical refresh', () => {
  const result = finalize({ [apex]: { code: 200, effective: apex,
    body: "<HTML><HEAD><META content='0;url=https://mcujs.org/' HTTP-EQUIV='Refresh'></HEAD></HTML>" } });
  assert.equal(result.status, 0, result.stdout + result.stderr);
});

for (const target of [
  'http://mcujs.org/', '//mcujs.org/', 'https://evil.example/',
  'https://mcujs.org.evil.example/', 'https://mcujs.org@evil.example/',
  'https://mcujs.org/wrong', 'https://www.mcujs.org/',
  'https://mcujs.org/?next=wrong', 'https://mcujs.org/#wrong',
]) {
  test(`rejects noncanonical meta-refresh target ${target}`, () => {
    const result = finalize({ [apex]: { code: 200, effective: apex, body: redirect.replace(canonical, target) } });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /exact canonical meta-refresh/);
    assert.equal(result.calls.filter(c => c.command === 'curl').at(-1).args.at(-1), apex);
  });
}

const meta = '<meta http-equiv="refresh" content="0; url=https://mcujs.org/">';
for (const [name, body] of Object.entries({
  'arbitrary HTTP 200 error page': '<html><head><title>Site not found</title></head><body>Unavailable</body></html>',
  'canonical link only': `<html><head><link rel="canonical" href="${canonical}"></head></html>`,
  'commented-out refresh': `<html><head><!-- ${meta} --></head></html>`,
  'refresh text in script': `<html><head><script>const example = '${meta}';</script></head></html>`,
  'refresh text in title': `<html><head><title>${meta}</title></head></html>`,
  'inert template refresh': `<html><head><template>${meta}</template></head></html>`,
  'self-closing template does not activate its contents': `<html><head><template/>${meta}</template></head></html>`,
  'self-closing raw-text element does not activate its contents': `<html><head><title/>${meta}</title></head></html>`,
  'noscript refresh': `<html><head><noscript>${meta}</noscript></head></html>`,
  'duplicate content attributes': `<html><head><meta http-equiv="refresh" content="0; url=https://evil.example/" content="0; url=${canonical}"></head></html>`,
  'conflicting refreshes': `<html><head>${meta}<meta http-equiv="refresh" content="0; url=https://evil.example/"></head></html>`,
  'delayed refresh': redirect.replace('0; url=', '30; url='),
})) {
  test(`rejects ${name}`, () => {
    const result = finalize({ [apex]: { code: 200, effective: apex, body } });
    assert.notEqual(result.status, 0, result.stdout);
    assert.match(result.stderr, /exact canonical meta-refresh/);
  });
}

for (const effective of ['http://mcujs.com/', 'https://evil.example/', 'https://mcujs.com.evil.example/', 'https://mcujs.com/error']) {
  test(`rejects a canonical refresh served from ${effective}`, () => {
    const result = finalize({ [apex]: { code: 200, effective, body: redirect } });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /untrusted redirect page/);
  });
}

for (const url of [apex, www]) {
  test(`rejects HTTP error responses with otherwise valid HTML at ${url}`, () => {
    const result = finalize({ [url]: { code: 404, effective: url, body: redirect } });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /returned HTTP 404/);
  });
  test(`rejects TLS verification failure at ${url}`, () => {
    const result = finalize({ [url]: { exit: 60 } });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /HTTPS request failed/);
  });
}

for (const [name, destination] of Object.entries({
  'HTTP error': { code: 503, effective: canonical, body: 'Service Unavailable' },
  'wrong destination': { code: 200, effective: 'https://evil.example/' },
  'noncanonical path': { code: 200, effective: 'https://mcujs.org/error' },
  'HTTPS downgrade': { code: 200, effective: 'http://mcujs.org/' },
  'TLS failure': { exit: 60 },
})) {
  test(`rechecks the meta-refresh destination and rejects ${name}`, () => {
    const result = finalize({ [canonical]: [ { code: 200, effective: canonical }, destination ] });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /mcujs.com destination/);
    assert.equal(result.calls.filter(c => c.command === 'curl' && c.args.at(-1) === canonical).length, 2);
    assert.ok(!result.calls.some(c => c.command === 'curl' && c.args.at(-1) === www));
  });
}

for (const contentType of ['text/plain', 'application/json', '']) {
  test(`rejects refresh markup served as ${contentType || 'unknown content type'}`, () => {
    const result = finalize({ [apex]: { code: 200, effective: apex, body: redirect, contentType } });
    assert.notEqual(result.status, 0, result.stdout);
    assert.match(result.stderr, /not an HTML redirect page/);
  });
}

test('all requests retain TLS verification and prohibit non-HTTPS redirects', () => {
  const result = finalize();
  assert.equal(result.status, 0, result.stdout + result.stderr);
  for (const { args } of result.calls.filter(c => c.command === 'curl')) {
    assert.ok(args.includes('-L'));
    assert.equal(args[args.indexOf('--proto') + 1], '=https');
    assert.equal(args[args.indexOf('--proto-redir') + 1], '=https');
    assert.ok(!args.includes('-k') && !args.includes('--insecure'));
  }
});
