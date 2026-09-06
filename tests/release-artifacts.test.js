const assert = require("node:assert/strict");
const { execFileSync, spawnSync } = require("node:child_process");
const {
  chmodSync,
  copyFileSync,
  mkdirSync,
  mkdtempSync,
  readdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} = require("node:fs");
const { tmpdir } = require("node:os");
const { join } = require("node:path");
const test = require("node:test");

const root = join(__dirname, "..");
const releaseScript = join(root, "scripts", "release.sh");
const packageScript = join(root, "scripts", "package-release.sh");
const verifierPath = join(root, "scripts", "verify-esp32-release-artifacts.js");
const version = readFileSync(join(root, "version.txt"), "utf8").trim();
const gitSha = execFileSync("git", ["-C", root, "rev-parse", "--short", "HEAD"], {
  encoding: "utf8",
}).trim();
const board = "seeed_xiao_esp32s3";
const uf2Name = `mcujs-${version}-${board}.uf2`;
const capabilityName = `mcujs-${version}-${board}.capabilities.json`;

function makeUf2(binary, { flags = 0x00002000 } = {}) {
  const blockCount = Math.ceil(binary.length / 256);
  const uf2 = Buffer.alloc(blockCount * 512);
  for (let index = 0; index < blockCount; index += 1) {
    const block = uf2.subarray(index * 512, (index + 1) * 512);
    block.writeUInt32LE(0x0a324655, 0);
    block.writeUInt32LE(0x9e5d5157, 4);
    block.writeUInt32LE(flags, 8);
    block.writeUInt32LE(index * 256, 12);
    block.writeUInt32LE(256, 16);
    block.writeUInt32LE(index, 20);
    block.writeUInt32LE(blockCount, 24);
    block.writeUInt32LE(0xc47e5767, 28);
    binary.copy(block, 32, index * 256, (index + 1) * 256);
    block.writeUInt32LE(0x0ab16f30, 508);
  }
  return uf2;
}

function writeFixture(buildDir, buildId) {
  const binary = Buffer.alloc(256);
  binary.write(buildId, 32, "ascii");
  writeFileSync(join(buildDir, "mcujs-esp32s3.bin"), binary);
  writeFileSync(join(buildDir, uf2Name), makeUf2(binary));
  writeFileSync(
    join(buildDir, capabilityName),
    readFileSync(join(root, "runtime", "manifests", `${board}.json`)),
  );
  return binary;
}

function runVerifier(buildDir) {
  return spawnSync(process.execPath, [verifierPath, "--build-dir", buildDir], {
    cwd: root,
    encoding: "utf8",
  });
}

function writeExecutable(path, body) {
  writeFileSync(path, `#!/usr/bin/env bash\nset -euo pipefail\n${body}\n`);
  chmodSync(path, 0o755);
}

function writeSignalInjectingMv(harnessRoot) {
  const binDir = join(harnessRoot, "test-bin");
  const realMv = execFileSync("sh", ["-c", "command -v mv"], { encoding: "utf8" }).trim();
  mkdirSync(binDir, { recursive: true });
  writeExecutable(
    join(binDir, "mv"),
    [
      'args=("$@")',
      'source_path="${args[${#args[@]}-2]}"',
      'target_path="${args[${#args[@]}-1]}"',
      `"${realMv}" "$@"`,
      'if [[ ! -e "${MCUJS_PACKAGE_TEST_SIGNAL_MARKER}" ]]; then',
      '  should_signal=0',
      '  case "${MCUJS_PACKAGE_TEST_SIGNAL_PHASE}" in',
      '    backup) [[ "${target_path}" == */.mcujs-package-backup.*/* ]] && should_signal=1 ;;',
      '    publish) [[ "${source_path}" == */.mcujs-package-stage.*/* ]] && should_signal=1 ;;',
      '  esac',
      '  if [[ "${should_signal}" -eq 1 ]]; then',
      '    : > "${MCUJS_PACKAGE_TEST_SIGNAL_MARKER}"',
      '    kill -TERM "${PPID}"',
      '  fi',
      'fi',
    ].join("\n"),
  );
  return binDir;
}

function assertNoPackageTemps(distDir) {
  assert.deepEqual(
    readdirSync(distDir).filter((name) => name.startsWith(".mcujs-package-")),
    [],
  );
}

function writePackageHarness(harnessRoot) {
  const harnessVersion = "9.9.9";
  const releaseName = `mcujs-${harnessVersion}-unknown`;
  const harnessUf2 = `mcujs-${harnessVersion}-${board}.uf2`;
  const harnessCapability = `mcujs-${harnessVersion}-${board}.capabilities.json`;
  mkdirSync(join(harnessRoot, "scripts", "lib"), { recursive: true });
  mkdirSync(join(harnessRoot, "platform", "esp32", "build-docker"), { recursive: true });
  mkdirSync(join(harnessRoot, "runtime", "manifests"), { recursive: true });
  copyFileSync(packageScript, join(harnessRoot, "scripts", "package-release.sh"));
  writeFileSync(join(harnessRoot, "version.txt"), `${harnessVersion}\n`);
  writeFileSync(
    join(harnessRoot, "scripts", "lib", "boards.sh"),
    [
      `MCUJS_RELEASE_BOARDS=(${board})`,
      'mcujs_board_chip() { printf "ESP32-S3"; }',
      'mcujs_board_flash() { printf "8MB"; }',
      "",
    ].join("\n"),
  );
  writeFileSync(join(harnessRoot, "scripts", "generate-runtime-registry.js"), "");
  writeFileSync(join(harnessRoot, "scripts", "verify-esp32-release-artifacts.js"), "");
  writeFileSync(join(harnessRoot, "runtime", "manifests", `${board}.json`), "{\"current\":true}\n");
  writeFileSync(join(harnessRoot, "platform", "esp32", "build-docker", harnessUf2), "new uf2\n");
  writeFileSync(
    join(harnessRoot, "platform", "esp32", "build-docker", harnessCapability),
    "{\"current\":true}\n",
  );
  return { releaseName, harnessUf2, harnessCapability };
}

function runReleaseHarness(args) {
  const harnessRoot = mkdtempSync(join(tmpdir(), "mcujs-release-driver-test-"));
  const logPath = join(harnessRoot, "calls.log");
  try {
    mkdirSync(join(harnessRoot, "scripts"), { recursive: true });
    mkdirSync(join(harnessRoot, "platform", "esp32"), { recursive: true });
    copyFileSync(releaseScript, join(harnessRoot, "scripts", "release.sh"));
    writeExecutable(
      join(harnessRoot, "scripts", "verify-release.sh"),
      'printf "verify-release\\n" >> "${MCUJS_RELEASE_TEST_LOG}"',
    );
    writeExecutable(
      join(harnessRoot, "build.sh"),
      'printf "build.sh %s\\n" "$*" >> "${MCUJS_RELEASE_TEST_LOG}"',
    );
    writeExecutable(
      join(harnessRoot, "platform", "esp32", "docker-build.sh"),
      'printf "docker-build\\n" >> "${MCUJS_RELEASE_TEST_LOG}"',
    );
    writeExecutable(
      join(harnessRoot, "scripts", "package-release.sh"),
      'printf "package-release %s\\n" "$*" >> "${MCUJS_RELEASE_TEST_LOG}"',
    );

    const result = spawnSync("bash", [join(harnessRoot, "scripts", "release.sh"), ...args], {
      encoding: "utf8",
      env: { ...process.env, MCUJS_RELEASE_TEST_LOG: logPath },
    });
    assert.equal(result.status, 0, result.stderr);
    return readFileSync(logPath, "utf8").trim().split("\n");
  } finally {
    rmSync(harnessRoot, { recursive: true, force: true });
  }
}

test("release driver freshly builds XIAO before packaging and scopes its flags", () => {
  assert.deepEqual(runReleaseHarness([]), [
    "verify-release",
    "build.sh all --clean",
    "docker-build",
    "package-release --force",
  ]);
  assert.deepEqual(runReleaseHarness(["--no-docker"]), [
    "verify-release",
    "build.sh all --clean --no-docker",
    "docker-build",
    "package-release --force",
  ]);
  assert.deepEqual(runReleaseHarness(["--skip-build"]), [
    "verify-release",
    "package-release --force",
  ]);
  assert.deepEqual(runReleaseHarness(["--rebuild-image"]), [
    "verify-release",
    "build.sh all --prepare-image",
    "build.sh all --clean",
    "docker-build",
    "package-release --force",
  ]);

  const help = execFileSync("bash", [releaseScript, "--help"], { encoding: "utf8" });
  assert.match(help, /--no-docker\s+Build RP boards with the local Pico toolchain/);
  assert.match(help, /XIAO ESP32-S3.*pinned Docker lane/s);
  assert.match(help, /--skip-build\s+Package existing RP and XIAO build artifacts/);

  const packageSource = readFileSync(packageScript, "utf8");
  assert.match(
    packageSource,
    /node "\$\{ROOT_DIR\}\/scripts\/verify-esp32-release-artifacts\.js"/,
  );
});

test("XIAO release verification rejects stale or relabelled firmware", () => {
  const buildDir = mkdtempSync(join(tmpdir(), "mcujs-xiao-release-test-"));
  try {
    const binary = writeFixture(buildDir, `${version}+${gitSha}`);
    const current = runVerifier(buildDir);
    assert.equal(current.status, 0, current.stderr);
    assert.match(current.stdout, /Verified XIAO ESP32-S3 release artifacts/);

    writeFixture(buildDir, `${version}+0000000`);
    const stale = runVerifier(buildDir);
    assert.notEqual(stale.status, 0);
    assert.match(stale.stderr, /does not contain current source build ID/);

    writeFileSync(join(buildDir, "mcujs-esp32s3.bin"), binary);
    const relabelledUf2 = makeUf2(binary);
    relabelledUf2[64] ^= 0xff;
    writeFileSync(join(buildDir, uf2Name), relabelledUf2);
    const relabelled = runVerifier(buildDir);
    assert.notEqual(relabelled.status, 0);
    assert.match(relabelled.stderr, /UF2 payload does not match mcujs-esp32s3.bin/);

    writeFixture(buildDir, `${version}+${gitSha}`);
    writeFileSync(join(buildDir, capabilityName), "{}\n");
    const mismatchedManifest = runVerifier(buildDir);
    assert.notEqual(mismatchedManifest.status, 0);
    assert.match(mismatchedManifest.stderr, /manifest does not match the generated registry/);
  } finally {
    rmSync(buildDir, { recursive: true, force: true });
  }
});

test("XIAO release verification rejects unsupported UF2 flags and ota_0 overrun", () => {
  const buildDir = mkdtempSync(join(tmpdir(), "mcujs-xiao-uf2-bounds-test-"));
  try {
    const buildId = `${version}+${gitSha}`;
    const binary = writeFixture(buildDir, buildId);

    writeFileSync(join(buildDir, uf2Name), makeUf2(binary, { flags: 0x00002001 }));
    const noFlash = runVerifier(buildDir);
    assert.notEqual(noFlash.status, 0);
    assert.match(noFlash.stderr, /unsupported XIAO UF2 flags/);

    writeFileSync(join(buildDir, uf2Name), makeUf2(binary, { flags: 0x00002002 }));
    const unknownFlag = runVerifier(buildDir);
    assert.notEqual(unknownFlag.status, 0);
    assert.match(unknownFlag.stderr, /unsupported XIAO UF2 flags/);

    const oversizedBinary = Buffer.alloc(0x400100);
    oversizedBinary.write(buildId, 32, "ascii");
    writeFileSync(join(buildDir, "mcujs-esp32s3.bin"), oversizedBinary);
    writeFileSync(join(buildDir, uf2Name), makeUf2(oversizedBinary));
    const overrun = runVerifier(buildDir);
    assert.notEqual(overrun.status, 0);
    assert.match(overrun.stderr, /exceeds ota_0 payload limit of 4194304 bytes/);
  } finally {
    rmSync(buildDir, { recursive: true, force: true });
  }
});

test("package release preserves the authoritative release on injected late failure", () => {
  const harnessRoot = mkdtempSync(join(tmpdir(), "mcujs-package-transaction-test-"));
  try {
    const { releaseName, harnessUf2, harnessCapability } = writePackageHarness(harnessRoot);
    const distDir = join(harnessRoot, "dist");
    const packageDir = join(distDir, releaseName);
    mkdirSync(packageDir, { recursive: true });
    const preserved = new Map([
      [join(packageDir, "preserved.txt"), "old package\n"],
      [join(distDir, harnessUf2), "old uf2\n"],
      [join(distDir, harnessCapability), "old capabilities\n"],
      [join(distDir, `${releaseName}-manifest.txt`), "old manifest\n"],
      [join(distDir, `${releaseName}-SHA256SUMS.txt`), "old sums\n"],
      [join(distDir, `${releaseName}.tar.gz`), "old tarball\n"],
    ]);
    for (const [path, contents] of preserved) writeFileSync(path, contents);

    const fakeTar = join(harnessRoot, "fake-tar");
    writeExecutable(
      fakeTar,
      'if [[ "${1:-}" == "--version" ]]; then printf "tar (GNU tar) 1.0\\n"; exit 0; fi\nprintf "injected late tar failure\\n" >&2\nexit 73',
    );
    const result = spawnSync("bash", [join(harnessRoot, "scripts", "package-release.sh"), "--force"], {
      cwd: harnessRoot,
      encoding: "utf8",
      env: { ...process.env, TAR: fakeTar },
    });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /injected late tar failure/);
    for (const [path, contents] of preserved) {
      assert.equal(readFileSync(path, "utf8"), contents, `preserved ${path}`);
    }
    assertNoPackageTemps(distDir);
  } finally {
    rmSync(harnessRoot, { recursive: true, force: true });
  }
});

test("package release rolls back TERM immediately after backup and publication renames", () => {
  for (const phase of ["backup", "publish"]) {
    const harnessRoot = mkdtempSync(join(tmpdir(), `mcujs-package-${phase}-signal-test-`));
    try {
      const { releaseName, harnessUf2, harnessCapability } = writePackageHarness(harnessRoot);
      const distDir = join(harnessRoot, "dist");
      const packageDir = join(distDir, releaseName);
      mkdirSync(packageDir, { recursive: true });
      const preserved = new Map([
        [join(packageDir, "preserved.txt"), `old package ${phase}\n`],
        [join(distDir, harnessUf2), `old uf2 ${phase}\n`],
        [join(distDir, harnessCapability), `old capabilities ${phase}\n`],
        [join(distDir, `${releaseName}-manifest.txt`), `old manifest ${phase}\n`],
        [join(distDir, `${releaseName}-SHA256SUMS.txt`), `old sums ${phase}\n`],
        [join(distDir, `${releaseName}.tar.gz`), `old tarball ${phase}\n`],
      ]);
      for (const [path, contents] of preserved) writeFileSync(path, contents);

      const binDir = writeSignalInjectingMv(harnessRoot);
      const marker = join(harnessRoot, `${phase}.signal-fired`);
      const result = spawnSync(
        "bash",
        [join(harnessRoot, "scripts", "package-release.sh"), "--force"],
        {
          cwd: harnessRoot,
          encoding: "utf8",
          env: {
            ...process.env,
            MCUJS_PACKAGE_TEST_SIGNAL_MARKER: marker,
            MCUJS_PACKAGE_TEST_SIGNAL_PHASE: phase,
            PATH: `${binDir}:${process.env.PATH}`,
          },
        },
      );
      assert.notEqual(result.status, 0, `${phase} injection unexpectedly succeeded`);
      assert.equal(readFileSync(marker, "utf8"), "");
      for (const [path, contents] of preserved) {
        assert.equal(readFileSync(path, "utf8"), contents, `${phase} preserved ${path}`);
      }
      assertNoPackageTemps(distDir);
    } finally {
      rmSync(harnessRoot, { recursive: true, force: true });
    }
  }
});

test("package release without --force rejects any existing top-level target", () => {
  const harnessRoot = mkdtempSync(join(tmpdir(), "mcujs-package-existing-target-test-"));
  try {
    const { releaseName, harnessUf2 } = writePackageHarness(harnessRoot);
    const distDir = join(harnessRoot, "dist");
    mkdirSync(distDir, { recursive: true });
    const partialTarget = join(distDir, harnessUf2);
    writeFileSync(partialTarget, "authoritative partial output\n");

    const result = spawnSync("bash", [join(harnessRoot, "scripts", "package-release.sh")], {
      cwd: harnessRoot,
      encoding: "utf8",
    });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /already exists.*--force/);
    assert.equal(readFileSync(partialTarget, "utf8"), "authoritative partial output\n");
    assert.equal(readdirSync(distDir).includes(releaseName), false);
    assertNoPackageTemps(distDir);
  } finally {
    rmSync(harnessRoot, { recursive: true, force: true });
  }
});
