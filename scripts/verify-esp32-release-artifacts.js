#!/usr/bin/env node
"use strict";

const { execFileSync } = require("node:child_process");
const { lstatSync, readFileSync } = require("node:fs");
const { join, resolve } = require("node:path");

const UF2_MAGIC_START0 = 0x0a324655;
const UF2_MAGIC_START1 = 0x9e5d5157;
const UF2_MAGIC_END = 0x0ab16f30;
const UF2_FAMILY_FLAG = 0x00002000;
const UF2_SUPPORTED_FLAGS = UF2_FAMILY_FLAG;
const ESP32S3_FAMILY_ID = 0xc47e5767;
const UF2_BLOCK_SIZE = 512;
const UF2_PAYLOAD_SIZE = 256;

function fail(message) {
  throw new Error(message);
}

function readRequiredFile(path, label) {
  let stat;
  try {
    stat = lstatSync(path);
  } catch {
    fail(`Missing ${label}: ${path}`);
  }
  if (stat.isSymbolicLink() || !stat.isFile()) {
    fail(`${label} is not a regular file: ${path}`);
  }
  return readFileSync(path);
}

function readOta0PayloadLimit(root) {
  const partitionPath = join(root, "platform", "esp32", "partitions.csv");
  const contents = readRequiredFile(partitionPath, "ESP32 partition table").toString("utf8");
  // This release lane supports the pinned TinyUF2 no-OTA layout only. A
  // same-named row alone cannot establish the runtime destination or capacity.
  const expected = [
    ["nvs", "data", "nvs", 0x9000, 0x5000],
    ["otadata", "data", "ota", 0xe000, 0x2000],
    ["ota_0", "app", "ota_0", 0x10000, 0x400000],
    ["uf2", "app", "factory", 0x410000, 0x40000],
    ["ffat", "data", "fat", 0x450000, 0x3b0000],
  ];
  const rows = contents.split(/\r?\n/)
    .map((line) => line.replace(/#.*/, "").trim())
    .filter(Boolean)
    .map((line) => line.split(",").map((field) => field.trim()));
  const number = (value) => /^(?:0x[0-9a-f]+|[0-9]+)$/i.test(value) ? Number(value) : NaN;
  if (rows.length !== expected.length) fail("Unsupported ESP32 partition contract: entry count");
  for (const [name, type, subtype, offset, size] of expected) {
    const matches = rows.filter((row) => row[0] === name);
    const row = matches[0];
    if (matches.length !== 1 || row.length < 5 || row.length > 6 ||
        row[1] !== type || row[2] !== subtype || number(row[3]) !== offset ||
        number(row[4]) !== size || (row[5] || "") !== "") {
      fail(`Unsupported ESP32 partition contract for ${name}`);
    }
  }
  return 0x400000;
}

function reconstructUf2(uf2) {
  if (uf2.length === 0 || uf2.length % UF2_BLOCK_SIZE !== 0) {
    fail("XIAO UF2 is empty or not 512-byte block aligned");
  }

  const blockCount = uf2.length / UF2_BLOCK_SIZE;
  const payloads = [];
  for (let index = 0; index < blockCount; index += 1) {
    const offset = index * UF2_BLOCK_SIZE;
    const block = uf2.subarray(offset, offset + UF2_BLOCK_SIZE);
    const magic0 = block.readUInt32LE(0);
    const magic1 = block.readUInt32LE(4);
    const flags = block.readUInt32LE(8);
    const address = block.readUInt32LE(12);
    const payloadSize = block.readUInt32LE(16);
    const blockNumber = block.readUInt32LE(20);
    const declaredCount = block.readUInt32LE(24);
    const familyId = block.readUInt32LE(28);
    const endMagic = block.readUInt32LE(508);

    if (magic0 !== UF2_MAGIC_START0 || magic1 !== UF2_MAGIC_START1 || endMagic !== UF2_MAGIC_END) {
      fail(`Invalid XIAO UF2 magic in block ${index}`);
    }
    if (flags !== UF2_SUPPORTED_FLAGS) {
      fail(`Invalid unsupported XIAO UF2 flags 0x${flags.toString(16)} in block ${index}`);
    }
    if (familyId !== ESP32S3_FAMILY_ID) {
      fail(`Invalid XIAO ESP32-S3 UF2 family metadata in block ${index}`);
    }
    if (payloadSize !== UF2_PAYLOAD_SIZE || blockNumber !== index || declaredCount !== blockCount) {
      fail(`Invalid XIAO UF2 block metadata in block ${index}`);
    }
    if (address !== index * UF2_PAYLOAD_SIZE) {
      fail(`XIAO UF2 payload addresses are not contiguous from zero at block ${index}`);
    }
    payloads.push(block.subarray(32, 32 + payloadSize));
  }
  return Buffer.concat(payloads);
}

function verifyPayload(binary, uf2, ota0PayloadLimit) {
  if (!Number.isSafeInteger(ota0PayloadLimit) || ota0PayloadLimit <= 0) {
    fail(`Invalid ota_0 payload limit: ${ota0PayloadLimit}`);
  }
  const reconstructed = reconstructUf2(uf2);
  if (reconstructed.length > ota0PayloadLimit) {
    fail(`XIAO UF2 payload exceeds ota_0 payload limit of ${ota0PayloadLimit} bytes`);
  }
  if (reconstructed.length < binary.length ||
      !reconstructed.subarray(0, binary.length).equals(binary)) {
    fail("XIAO UF2 payload does not match mcujs-esp32s3.bin");
  }
  for (const byte of reconstructed.subarray(binary.length)) {
    if (byte !== 0) fail("XIAO UF2 payload has non-zero data beyond mcujs-esp32s3.bin");
  }
}

function verifyEsp32ReleaseArtifacts({ root, buildDir, version, gitSha }) {
  const board = "seeed_xiao_esp32s3";
  const binaryPath = join(buildDir, "mcujs-esp32s3.bin");
  const uf2Path = join(buildDir, `mcujs-${version}-${board}.uf2`);
  const capabilityPath = join(buildDir, `mcujs-${version}-${board}.capabilities.json`);
  const generatedCapabilityPath = join(root, "runtime", "manifests", `${board}.json`);
  const binary = readRequiredFile(binaryPath, "XIAO firmware binary");
  const uf2 = readRequiredFile(uf2Path, "XIAO UF2");
  const capability = readRequiredFile(capabilityPath, "XIAO capability manifest");
  const generatedCapability = readRequiredFile(generatedCapabilityPath, "generated XIAO capability manifest");
  const ota0PayloadLimit = readOta0PayloadLimit(root);
  const expectedBuildId = `${version}+${gitSha}`;

  if (!/^[0-9a-f]{7,40}$/.test(gitSha)) {
    fail(`Invalid source Git SHA for XIAO release verification: ${gitSha}`);
  }
  if (!binary.includes(Buffer.from(expectedBuildId, "ascii"))) {
    fail(`XIAO firmware does not contain current source build ID ${expectedBuildId}`);
  }
  verifyPayload(binary, uf2, ota0PayloadLimit);
  if (!capability.equals(generatedCapability)) {
    fail("XIAO capability manifest does not match the generated registry");
  }

  return { binaryPath, uf2Path, capabilityPath, expectedBuildId };
}

function main() {
  const root = resolve(__dirname, "..");
  let buildDir = resolve(join(root, "platform", "esp32", "build-docker"));
  if (process.argv.length > 2) {
    if (process.argv.length !== 4 || process.argv[2] !== "--build-dir") {
      fail("Usage: scripts/verify-esp32-release-artifacts.js [--build-dir PATH]");
    }
    buildDir = resolve(process.argv[3]);
  }
  const version = readFileSync(join(root, "version.txt"), "utf8").trim();
  const gitSha = execFileSync("git", ["-C", root, "rev-parse", "--short", "HEAD"], {
    encoding: "utf8",
  }).trim();
  const result = verifyEsp32ReleaseArtifacts({ root, buildDir, version, gitSha });
  console.log(`Verified XIAO ESP32-S3 release artifacts for ${result.expectedBuildId}`);
}

if (require.main === module) {
  try {
    main();
  } catch (error) {
    console.error(`[FAIL] ${error.message}`);
    process.exitCode = 1;
  }
}

module.exports = {
  readOta0PayloadLimit,
  reconstructUf2,
  verifyEsp32ReleaseArtifacts,
  verifyPayload,
};
