import { readFileSync, readdirSync, writeFileSync, unlinkSync, renameSync, mkdirSync } from "fs";
import { join } from "path";

const TEXT_DECODER = new TextDecoder();

function runCommand(command, options = {}) {
  const result = Bun.spawnSync(["bash", "-lc", command], {
    stdout: "pipe",
    stderr: "pipe",
    ...options,
  });
  if (result.exitCode !== 0) {
    const stderr = TEXT_DECODER.decode(result.stderr || new Uint8Array());
    throw new Error(`Command failed: ${command}\n${stderr}`);
  }
  return TEXT_DECODER.decode(result.stdout || new Uint8Array()).trim();
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function readVersion() {
  return readFileSync(new URL("../../version.txt", import.meta.url), "utf8").trim();
}

function readGitSha() {
  const result = Bun.spawnSync(["git", "rev-parse", "--short", "HEAD"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (result.exitCode !== 0) {
    return "unknown";
  }
  return TEXT_DECODER.decode(result.stdout || new Uint8Array()).trim();
}

export function expectedBuildId() {
  return `${readVersion()}+${readGitSha()}`;
}

export function buildFirmware() {
  runCommand("./build.sh pico");
}

export function uf2Path() {
  return new URL("../../build/mcujs-0.1.0-pico.uf2", import.meta.url).pathname;
}

export function findSerialPort() {
  const devices = readdirSync("/dev");
  const matches = devices.filter((name) => name.startsWith("ttyACM"));
  if (matches.length === 0) {
    return null;
  }
  return `/dev/${matches[0]}`;
}

export function listBlockDevices() {
  const output = runCommand("lsblk -J -o NAME,LABEL,MOUNTPOINT");
  return JSON.parse(output);
}

export function findMount(label) {
  const data = listBlockDevices();
  for (const device of data.blockdevices || []) {
    if (device.label === label) {
      return device.mountpoint || null;
    }
    for (const child of device.children || []) {
      if (child.label === label) {
        return child.mountpoint || null;
      }
    }
  }
  return null;
}

export function hasLabel(label) {
  const data = listBlockDevices();
  for (const device of data.blockdevices || []) {
    if (device.label === label) {
      return true;
    }
    for (const child of device.children || []) {
      if (child.label === label) {
        return true;
      }
    }
  }
  return false;
}

export function ensureMount(label) {
  const mountpoint = findMount(label);
  if (mountpoint) {
    return mountpoint;
  }
  const data = listBlockDevices();
  for (const device of data.blockdevices || []) {
    for (const child of device.children || []) {
      if (child.label === label) {
        runCommand(`udisksctl mount -b /dev/${child.name}`);
        return findMount(label);
      }
    }
  }
  throw new Error(`Could not mount volume ${label}`);
}

export function copyUf2(mountpoint) {
  const target = join(mountpoint, "mcujs.uf2");
  runCommand(`cp "${uf2Path()}" "${target}"`);
}

export async function waitForLabel(label, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const mountpoint = findMount(label);
    if (mountpoint) {
      return mountpoint;
    }
    if (hasLabel(label)) {
      return null;
    }
    await sleep(250);
  }
  throw new Error(`Timed out waiting for ${label} volume`);
}

export async function waitForLabelAvailable(label, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (hasLabel(label)) {
      return;
    }
    await sleep(250);
  }
  throw new Error(`Timed out waiting for ${label} to appear`);
}

export async function waitForLabelDisconnect(label, timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (!hasLabel(label)) {
      return;
    }
    await sleep(250);
  }
  throw new Error(`Timed out waiting for ${label} disconnect`);
}

export async function waitForSerial(timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const port = findSerialPort();
    if (port) {
      return port;
    }
    await sleep(250);
  }
  throw new Error("Timed out waiting for serial port");
}

export async function waitForSerialDisconnect(timeoutMs = 15000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const port = findSerialPort();
    if (!port) {
      return;
    }
    await sleep(250);
  }
  throw new Error("Timed out waiting for serial disconnect");
}

export async function setupSerial(port, retries = 5, delayMs = 300) {
  for (let attempt = 0; attempt < retries; attempt += 1) {
    try {
      runCommand(`stty -F "${port}" 115200 raw -echo -icanon -icrnl -ixon -ixoff`);
      return;
    } catch (error) {
      if (attempt === retries - 1) {
        throw error;
      }
      await sleep(delayMs);
    }
  }
}

export function writeToPort(port, data) {
  return Bun.write(port, data);
}

export async function sendRaw(port, data) {
  await writeToPort(port, data);
}

export function openReader(port) {
  const file = Bun.file(port);
  return file.stream().getReader();
}

export async function readUntil(reader, matcher, timeoutMs = 5000) {
  const deadline = Date.now() + timeoutMs;
  let buffer = "";
  let readPromise = reader.read();

  while (Date.now() < deadline) {
    const result = await Promise.race([
      readPromise,
      sleep(100).then(() => ({ timeout: true })),
    ]);

    if (result && result.timeout) {
      continue;
    }

    if (!result || result.done) {
      break;
    }

    buffer += TEXT_DECODER.decode(result.value || new Uint8Array());
    if (matcher(buffer)) {
      return buffer;
    }

    readPromise = reader.read();
  }

  throw new Error(`Timed out waiting for serial output. Last buffer: ${buffer}`);
}

export async function sendCommand(port, reader, command, prompt = "> ") {
  await writeToPort(port, `${command}\r`);
  return readUntil(reader, (buffer) => buffer.includes(prompt));
}

export function ensureDir(path) {
  mkdirSync(path, { recursive: true });
}

export function writeFile(path, contents) {
  writeFileSync(path, contents);
}

export function removeFile(path) {
  try {
    unlinkSync(path);
  } catch (error) {
    if (error?.code !== "ENOENT") {
      throw error;
    }
  }
}

export function renameFile(from, to) {
  renameSync(from, to);
}

export function syncFilesystem() {
  runCommand("sync");
}
