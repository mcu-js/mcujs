#!/usr/bin/env node

// A closed check for this small Dockerfile, not a general Dockerfile parser.
// Keep hashes in Dockerfile; changes to acquisition commands need check review too.
const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const { resolve } = require("node:path");
const { spawnSync } = require("node:child_process");

const repositories = [
  ["PICO_SDK", "PICO_SDK_PATH", "/opt/pico-sdk", "raspberrypi/pico-sdk"],
  ["PICOTOOL", "PICOTOOL_SOURCE_PATH", "/opt/picotool-src", "raspberrypi/picotool"],
  ["JERRYSCRIPT", "JERRYSCRIPT_PATH", "/opt/jerryscript", "pando-project/jerryscript"],
  ["PICOJPEG", "PICOJPEG_PATH", "/opt/picojpeg", "richgel999/picojpeg"],
  ["PICODVI", "PICODVI_PATH", "/opt/picodvi", "Wren6991/PicoDVI"],
];
const variable = (name) => "${" + name + "}";

function check(source) {
  // Only the existing shell-form RUN, literal ENV, and simple final instructions
  // are supported. Reject new acquisition paths rather than guessing their safety.
  // BuildKit strips a leading BOM before detecting directives; reject it before
  // directive checks or comment removal can hide a change in parser semantics.
  assert.ok(!source.startsWith("\uFEFF"), "Leading UTF-8 BOM is not supported");
  // Docker removes full-line comments first; a comment's backslash never
  // continues onto the next instruction or hides it from the closed check.
  const uncommented = source.split(/\r?\n/)
    .filter((line) => {
      const comment = line.trimStart();
      if (!comment.startsWith("#")) return true;
      // Inspect the same representation we discard, including indentation.
      // This conservative policy is not a claim that every form is active.
      assert.doesNotMatch(comment, /^#\s*(syntax|escape|check)\s*=/i,
        "Parser directives need explicit review");
      return false;
    }).join("\n");
  const lines = uncommented.replace(/\\\n[ \t]*/g, "").split("\n")
    .map((line) => line.trim()).filter(Boolean);
  assert.match(lines.shift() || "", /^FROM alpine:3\.19@sha256:[0-9a-f]{64}$/,
    "Alpine 3.19 must have a full multi-platform index digest");
  const env = new Map();
  const runs = [];
  const final = [];
  for (const line of lines) {
    if (line.startsWith("ENV ")) {
      const match = /^ENV ([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)$/.exec(line);
      assert.ok(match, "ENV must use one literal value");
      assert.ok(!env.has(match[1]), `Duplicate ENV ${match[1]}`);
      env.set(match[1], match[2]);
    } else if (line.startsWith("RUN ")) {
      const command = line.slice(4);
      const syntax = spawnSync("sh", ["-n"], { input: command, encoding: "utf8" });
      assert.equal(syntax.status, 0, syntax.error?.message || syntax.stderr);
      runs.push(command);
    } else {
      final.push(line);
    }
  }
  function expectEnv(name, expected) {
    const value = env.get(name) || "";
    if (expected instanceof RegExp) assert.match(value, expected, `Invalid ${name}`);
    else assert.equal(value, expected, `Unexpected ${name}`);
    env.delete(name);
  }
  function expectRun(commands) {
    const expected = commands.join(" && ");
    const index = runs.indexOf(expected);
    assert.notEqual(index, -1, `Missing or changed acquisition/build command: ${commands[0]}`);
    runs.splice(index, 1);
  }
  expectRun(["apk add --no-cache build-base cmake git python3 bash newlib-arm-none-eabi " +
    "gcc-arm-none-eabi g++-arm-none-eabi linux-headers bsd-compat-headers"]);
  for (const [name, pathName, path, repo] of repositories) {
    expectEnv(pathName, path);
    expectEnv(`${name}_COMMIT`, /^[0-9a-f]{40}$/);
    const directory = variable(pathName);
    const commit = variable(`${name}_COMMIT`);
    const commands = [
      `git init ${directory}`, `cd ${directory}`,
      `git remote add origin https://github.com/${repo}.git`,
      `git fetch --depth 1 origin ${commit}`,
      "git checkout --detach FETCH_HEAD",
      `test "$(git rev-parse HEAD)" = "${commit}"`,
    ];
    if (name === "PICO_SDK") {
      commands.push("git submodule update --init --recursive --depth 1");
    }
    if (name === "PICOTOOL") {
      commands.push(`cmake -S ${directory} -B ${directory}/build ` +
        "-DPICO_SDK_PATH=${PICO_SDK_PATH} -DPICOTOOL_NO_LIBUSB=1 " +
        "-DPICOTOOL_FLAT_INSTALL=1 -DCMAKE_INSTALL_PREFIX=/opt/picotool",
      `cmake --build ${directory}/build --target install --parallel`,
      `rm -rf ${directory}`);
    }
    expectRun(commands);
  }
  expectEnv("PICOTOOL_VERSION", "2.2.0");
  expectEnv("picotool_DIR", "/opt/picotool/picotool");
  expectEnv("FATFS_PATH", "/opt/fatfs");
  expectEnv("FATFS_SHA256", /^[0-9a-f]{64}$/);
  expectRun([
    "mkdir -p ${FATFS_PATH}",
    "wget -q https://elm-chan.org/fsw/ff/arc/ff16.zip -O /tmp/ff16.zip",
    "printf '%s  %s\\n' \"${FATFS_SHA256}\" /tmp/ff16.zip | sha256sum -c -",
    "unzip -q /tmp/ff16.zip -d ${FATFS_PATH}",
    "rm /tmp/ff16.zip", "rm ${FATFS_PATH}/source/ffconf.h",
  ]);
  expectEnv("CTX_PATH", "/opt/ctx");
  expectEnv("CTX_SHA256", /^[0-9a-f]{64}$/);
  expectRun([
    "wget -q https://ctx.graphics/ctx-0.1.18.tar.bz2 -O /tmp/ctx.tar.bz2",
    "printf '%s  %s\\n' \"${CTX_SHA256}\" /tmp/ctx.tar.bz2 | sha256sum -c -",
    "mkdir -p ${CTX_PATH}",
    "tar -xjf /tmp/ctx.tar.bz2 --strip-components=1 -C ${CTX_PATH}",
    "rm /tmp/ctx.tar.bz2",
  ]);
  expectRun(["chmod +x /usr/local/bin/docker-entrypoint.sh"]);
  assert.equal(env.size, 0, "Unexpected ENV may change acquisition behavior");
  assert.deepEqual(runs, [], "Unexpected RUN; review any new direct inputs");
  assert.deepEqual(final, ["WORKDIR /workspace", "COPY docker-entrypoint.sh /usr/local/bin/",
    'ENTRYPOINT ["docker-entrypoint.sh"]', 'CMD ["all"]'],
  "Unexpected Dockerfile instruction; remote ADD/COPY and new stages need review");
}

try {
  const args = process.argv.slice(2);
  const selfTest = args[0] === "--self-test";
  if (selfTest) args.shift();
  assert.ok(args.length <= 1, "Usage: node scripts/check-rp-docker-inputs.js [--self-test] [Dockerfile]");
  const source = readFileSync(args[0] || resolve(__dirname, "../Dockerfile"), "utf8");
  check(source);
  console.log("RP direct-input policy and shell syntax checks passed.");
  if (selfTest) {
    const mutations = [
      ["mutable base", /@sha256:[0-9a-f]{64}/, ""],
      ["moving frontend", /^/, "# syntax=docker/dockerfile:latest\n"],
      ["BOM-prefixed syntax directive", /^/, "\uFEFF# syntax=docker/dockerfile:latest\n"],
      ["BOM-prefixed escape directive", /^/, "\uFEFF# escape=`\n"],
      ["HTTP ctx archive", "https://ctx.graphics", "http://ctx.graphics"],
      ["floating ctx archive", "ctx-0.1.18.tar.bz2", "ctx-latest.tar.bz2"],
      ["HTTP archive", "https://elm-chan.org", "http://elm-chan.org"],
      ["missing checksum", /    && printf[^\n]+\n/, ""],
      ["ignored checksum failure", "sha256sum -c -", "sha256sum -c - || true"],
      ["floating submodules", "submodule update --init", "submodule update --remote --init"],
      ["unchecked extra download", /$/, "\nRUN wget https://example.invalid/latest.zip\n"],
      ["unchecked RUN after comment backslash", /$/,
        "\n# review fixture comment \\\nRUN wget https://example.invalid/unpinned -O /opt/unverified\n"],
      ["remote ADD", /$/, "\nADD https://example.invalid/latest.zip /opt/\n"],
      ["ENV override", /$/, "\nENV PICO_SDK_COMMIT=master\n"],
    ];
    for (const [label, indent] of [["space", " "], ["tab", "\t"], ["mixed", " \t "]]) {
      for (const [directive, value] of [["syntax", "docker/dockerfile:latest"],
        ["escape", "`"], ["check", "skip=all"]]) {
        mutations.push([`${label}-prefixed ${directive} directive`, /^/,
          `${indent}# ${directive}=${value}\n`]);
        mutations.push([`${label}-prefixed ${directive} case/spacing/CRLF`, /^/,
          `${indent}#\t${directive.toUpperCase()} \t= ${value}\r\n`]);
      }
    }
    for (const [name] of repositories) {
      mutations.push([`${name} moving ref`, new RegExp(`ENV ${name}_COMMIT=[0-9a-f]{40}`),
        `ENV ${name}_COMMIT=master`]);
      mutations.push([`${name} missing identity check`,
        `    && test "$(git rev-parse HEAD)" = "${variable(`${name}_COMMIT`)}"`, "    && true"]);
    }
    for (const [label, pattern, replacement] of mutations) {
      const weakened = source.replace(pattern, replacement);
      assert.notEqual(weakened, source, `Mutation did not apply: ${label}`);
      assert.throws(() => check(weakened), undefined, `Accepted weakened copy: ${label}`);
      console.log(`Rejected weakened copy: ${label}`);
    }
    console.log(`All ${mutations.length} negative checks passed.`);
  }
} catch (error) {
  console.error(`RP Docker input check failed: ${error.message}`);
  process.exitCode = 1;
}
