const assert = require("node:assert/strict");
const { test } = require("node:test");
const { spawnSync } = require("node:child_process");
const { mkdtempSync, mkdirSync, copyFileSync, writeFileSync, readFileSync, readdirSync, rmSync, symlinkSync, existsSync } = require("node:fs");
const { join, resolve } = require("node:path");
const { tmpdir } = require("node:os");

const root = resolve(__dirname, "..");
const imageId = `sha256:${"a".repeat(64)}`;
const rpBoards = spawnSync("bash", ["scripts/boards.sh"], { cwd: root, encoding: "utf8" }).stdout.trim().split("\n");

// These are recording stubs, not firmware or Docker runtime evidence. Every
// invocation is intercepted; unexpected Docker verbs fail instead of falling back.
function fixture(run) {
  const dir = mkdtempSync(join(tmpdir(), "mcujs-docker-test-"));
  const source = join(dir, "source with spaces");
  const bin = join(dir, "bin");
  const log = join(dir, "calls.jsonl");
  mkdirSync(bin);
  for (const path of ["build.sh", "version.txt", "scripts/lib/boards.sh", "platform/esp32/docker-build.sh"]) {
    const dest = join(source, path);
    mkdirSync(resolve(dest, ".."), { recursive: true });
    copyFileSync(join(root, path), dest);
  }
  writeFileSync(join(bin, "git"), "#!/bin/sh\nprintf '1234567\\n'\n", { mode: 0o755 });
  writeFileSync(join(bin, "docker"), `#!${process.execPath}
const fs = require('node:fs');
const path = require('node:path');
const args = process.argv.slice(2);
fs.appendFileSync(process.env.TEST_LOG, JSON.stringify(args) + '\\n');
if (args[0] === 'image' && args[1] === 'inspect') {
  if (process.env.TEST_MISSING) process.exit(1);
  console.log(process.env.TEST_BAD_ID || '${imageId}');
} else if (args[0] === 'build') {
  process.exit(Number(process.env.TEST_BUILD_EXIT || 0));
} else if (args[0] === 'run') {
  const mount = args.find(a => a.endsWith(':/output'));
  if (!mount) throw new Error('Missing output mount');
  const output = mount.slice(0, -':/output'.length);
  const source = args.find(a => a.endsWith(':/source:ro')).slice(0, -':/source:ro'.length);
  const version = fs.readFileSync(path.join(source, 'version.txt'), 'utf8').trim();
  const esp = args.includes('/source/platform/esp32/docker-entrypoint.sh');
  const board = args[args.length - 2];
  const boards = board === 'all' ? JSON.parse(process.env.TEST_RP_BOARDS) : [board];
  const names = esp ? ['mcujs-esp32s3.bin', 'mcujs-esp32s3.elf', 'mcujs-esp32s3.map',
    'mcujs-' + version + '-seeed_xiao_esp32s3.uf2', 'mcujs-' + version + '-seeed_xiao_esp32s3.capabilities.json'] :
    boards.flatMap(b => ['bin', 'uf2', 'elf', 'elf.map'].map(ext => 'mcujs-' + version + '-' + b + '.' + ext));
  if (!process.env.TEST_NO_OUTPUT) {
    for (const name of names) {
      fs.writeFileSync(path.join(output, name), version + '+1234567\\nRECORDING STUB ONLY\\n');
      if (process.env.TEST_RUN_EXIT) break;
    }
  }
  process.exit(Number(process.env.TEST_RUN_EXIT || 0));
} else { throw new Error('Unexpected Docker call: ' + JSON.stringify(args)); }
`, { mode: 0o755 });
  const env = { ...process.env, PATH: `${bin}:${process.env.PATH}`, TEST_LOG: log, TEST_RP_BOARDS: JSON.stringify(rpBoards) };
  for (const key of Object.keys(env)) {
    if (key.startsWith("MCUJS_")) delete env[key];
  }
  function invoke(args, extra = {}, direct = false) {
    writeFileSync(log, "");
    const result = spawnSync("bash", [join(source, direct ? "platform/esp32/docker-build.sh" : "build.sh"), ...args], {
      cwd: dir, env: { ...env, ...extra }, encoding: "utf8",
    });
    const calls = readFileSync(log, "utf8").trim().split("\n").filter(Boolean).map(JSON.parse);
    return { ...result, calls, text: result.stdout + result.stderr };
  }
  try { run({ source, dir, invoke }); } finally { rmSync(dir, { recursive: true, force: true }); }
}

function option(args, name) { return args[args.indexOf(name) + 1]; }
for (const board of ["pico", "seeed_xiao_esp32s3"]) {
  const esp = board === "seeed_xiao_esp32s3";
  const imageEnv = esp ? "MCUJS_ESP32_DOCKER_IMAGE" : "MCUJS_RP_DOCKER_IMAGE";
  const outputPath = esp ? "platform/esp32/build-docker" : "build";
  test(`${board}: dispatch is immutable, networkless, source RO, caller UID/GID`, () => fixture(({ source, invoke }) => {
    const result = invoke([board, "--clean"], { [imageEnv]: "local/custom:reviewed", MCUJS_DOCKER_NETWORK: "host" });
    assert.equal(result.status, 0, result.text);
    assert.deepEqual(result.calls.map(a => a[0]), ["image", "run"]);
    assert.equal(result.calls[0].at(-1), "local/custom:reviewed");
    const args = result.calls[1];
    assert.equal(option(args, "--pull"), "never");
    assert.equal(option(args, "--network"), "none");
    assert.equal(args.filter(a => a === "--network").length, 1);
    assert.equal(option(args, "--cap-drop"), "ALL");
    assert.equal(option(args, "--security-opt"), "no-new-privileges");
    assert.equal(option(args, "-u"), `${process.getuid()}:${process.getgid()}`);
    assert.ok(args.includes(`${source}:/source:ro`));
    assert.ok(args.includes(`${join(source, outputPath)}:/output`));
    assert.equal(args.filter(a => a === "-v").length, 2);
    assert.ok(args.includes("HOME=/tmp/mcujs-home"));
    assert.ok(args.includes("MCUJS_BUILD_GIT_SHA=1234567"));
    assert.equal(option(args, "--entrypoint"), "/bin/bash");
    assert.ok(args.includes(imageId));
    assert.ok(!args.includes("local/custom:reviewed"));
    if (esp) {
      assert.equal(option(args, "--platform"), "linux/amd64");
      assert.deepEqual(args.slice(args.indexOf(imageId) + 1), ["/source/platform/esp32/docker-entrypoint.sh", "build"]);
    } else {
      assert.deepEqual(args.slice(args.indexOf(imageId) + 1), ["/source/docker-entrypoint.sh", "pico", "Release"]);
    }
  }));
  test(`${board}: missing/malformed local image fails without acquisition or output deletion`, () => fixture(({ source, invoke }) => {
    const output = join(source, outputPath);
    mkdirSync(output, { recursive: true });
    writeFileSync(join(output, "previous.uf2"), "preserve");
    for (const extra of [{ TEST_MISSING: "1" }, { TEST_BAD_ID: "not-an-image-id" }]) {
      const result = invoke([board], extra);
      assert.notEqual(result.status, 0);
      assert.deepEqual(result.calls.map(a => a[0]), ["image"]);
      assert.match(result.text, extra.TEST_MISSING ? /--prepare-image/ : /immutable image ID/);
      assert.equal(readFileSync(join(output, "previous.uf2"), "utf8"), "preserve");
    }
  }));
  test(`${board}: explicit preparation is separate, configurable and failure-preserving`, () => fixture(({ source, invoke }) => {
    for (const flag of ["--prepare-image", "--rebuild-image"]) {
      const result = invoke([board, flag, "--docker-network", "bridge"], { [imageEnv]: "local/custom:prepare" });
      assert.equal(result.status, 0, result.text);
      assert.deepEqual(result.calls.map(a => a[0]), ["build"]);
      const args = result.calls[0];
      assert.equal(option(args, "-t"), "local/custom:prepare");
      assert.equal(option(args, "--network"), "bridge");
      assert.equal(args.at(-1), source);
      if (esp) {
        assert.equal(option(args, "-f"), join(source, "platform/esp32/Dockerfile"));
        assert.equal(option(args, "--platform"), "linux/amd64");
      }
      assert.ok(!existsSync(join(source, outputPath)));
    }
    const failed = invoke([board, "--prepare-image"], { TEST_BUILD_EXIT: "37" });
    assert.equal(failed.status, 37);
    assert.deepEqual(failed.calls.map(a => a[0]), ["build"]);
  }));
  test(`${board}: failed/empty build clears selected stale and partial outputs`, () => fixture(({ source, invoke }) => {
    const output = join(source, outputPath);
    mkdirSync(output, { recursive: true });
    const old = `mcujs-0.0.0-${board}.uf2`;
    writeFileSync(join(output, old), "stale");
    writeFileSync(join(output, "unrelated.txt"), "preserve");
    const result = invoke([board], { TEST_RUN_EXIT: "42" });
    assert.equal(result.status, 42, result.text);
    assert.deepEqual(readdirSync(output), ["unrelated.txt"]);
    const empty = invoke([board], { TEST_NO_OUTPUT: "1" });
    assert.notEqual(empty.status, 0);
    assert.match(empty.text, /did not produce/);
    assert.deepEqual(readdirSync(output), ["unrelated.txt"]);
  }));
  test(`${board}: rejects network override and output symlinks before running`, () => fixture(({ source, dir, invoke }) => {
    const network = invoke([board, "--docker-network", "host"]);
    assert.notEqual(network.status, 0);
    assert.match(network.text, /only to --prepare-image/);
    assert.deepEqual(network.calls, []);
    const output = join(source, outputPath);
    mkdirSync(resolve(output, ".."), { recursive: true });
    symlinkSync(dir, output);
    const linked = invoke([board]);
    assert.notEqual(linked.status, 0);
    assert.deepEqual(linked.calls.map(a => a[0]), ["image"]);
    assert.match(linked.text, /symlink/i);
  }));
}

test("all still dispatches the existing RP registry; RP debug survives", () => fixture(({ invoke }) => {
  assert.ok(rpBoards.includes("pico2") && !rpBoards.includes("seeed_xiao_esp32s3"));
  for (const args of [[], ["all", "--clean"], ["pico2", "--debug"]]) {
    const result = invoke(args);
    assert.equal(result.status, 0, result.text);
    assert.deepEqual(result.calls.map(a => a[0]), ["image", "run"]);
    assert.deepEqual(result.calls[1].slice(-2), args[0] === "pico2" ? ["pico2", "Debug"] : ["all", "Release"]);
  }
}));

test("invalid/unsupported flags fail before Docker", () => fixture(({ invoke }) => {
  for (const args of [["not-a-board"], ["seeed_xiao_esp32s3", "--debug"], ["seeed_xiao_esp32s3", "--no-docker"], ["pico", "--no-docker", "--prepare-image"], ["pico", "--docker-network"]]) {
    const result = invoke(args);
    assert.notEqual(result.status, 0, result.text);
    assert.deepEqual(result.calls, []);
  }
}));

test("direct ESP32 entrypoint also never acquires implicitly", () => fixture(({ invoke }) => {
  const missing = invoke([], { TEST_MISSING: "1" }, true);
  assert.notEqual(missing.status, 0);
  assert.deepEqual(missing.calls.map(a => a[0]), ["image"]);
  const build = invoke([], { MCUJS_DOCKER_NETWORK: "host" }, true);
  assert.equal(build.status, 0, build.text);
  assert.deepEqual(build.calls.map(a => a[0]), ["image", "run"]);
  assert.equal(option(build.calls[1], "--network"), "none");
  const prep = invoke(["--prepare-image"], {}, true);
  assert.equal(prep.status, 0, prep.text);
  assert.deepEqual(prep.calls.map(a => a[0]), ["build"]);
}));
