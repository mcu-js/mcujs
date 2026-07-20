const { readdirSync, readFileSync } = require("node:fs");
const { join, relative } = require("node:path");
const test = require("node:test");
const assert = require("node:assert/strict");
const { boardDescriptors } = require("../runtime/board-registry.js");

const root = join(__dirname, "..");
const examplesRoot = join(root, "examples");
const rp2040PiZeroRoadmap = join(
  root,
  "board/waveshare_rp2040_pizero/waveshare_rp2040_pizero_phases.md",
);

function javascriptFiles(directory) {
  return readdirSync(directory, { withFileTypes: true })
    .flatMap((entry) => {
      const path = join(directory, entry.name);
      if (entry.isDirectory()) return javascriptFiles(path);
      return entry.isFile() && entry.name.endsWith(".js") ? [path] : [];
    })
    .sort();
}

function lineFor(source, index) {
  return source.slice(0, index).split("\n").length;
}

function collectMatches(path, source, expression, message) {
  const matches = [];
  for (const match of source.matchAll(expression)) {
    matches.push(
      `${relative(root, path)}:${lineFor(source, match.index)}: ${message}: ${match[0]}`,
    );
  }
  return matches;
}

test("examples use the strict boolean GPIO contract", () => {
  const failures = [];
  for (const path of javascriptFiles(examplesRoot)) {
    const source = readFileSync(path, "utf8");
    failures.push(
      ...collectMatches(
        path,
        source,
        /GPIO\.set\s*\([^,\n]+,\s*[-+]?(?:\d+(?:\.\d+)?|\.\d+)\s*\)/g,
        "GPIO.set numeric literal must be boolean",
      ),
      ...collectMatches(
        path,
        source,
        /GPIO\.set\s*\([^,\n]+,\s*[^\n?]+\?\s*[01]\s*:\s*[01]\s*\)/g,
        "GPIO.set numeric ternary must produce boolean",
      ),
      ...collectMatches(
        path,
        source,
        /GPIO\.get\s*\([^\n)]*\)\s*(?:===?|!==?)\s*[01]\b/g,
        "GPIO.get returns boolean and must not be compared with a number",
      ),
      ...collectMatches(
        path,
        source,
        /\b[01]\s*(?:===?|!==?)\s*GPIO\.get\s*\([^\n)]*\)/g,
        "GPIO.get returns boolean and must not be compared with a number",
      ),
    );
  }

  assert.deepEqual(failures, [], failures.join("\n"));
});

test("RP2040-PiZero guidance feature-detects its absent onboard LED", () => {
  const source = readFileSync(rp2040PiZeroRoadmap, "utf8");
  const guidance = source.split("\n").find((line) => line.includes("No onboard LED"));

  assert.ok(guidance, "roadmap must document the missing onboard LED");
  assert.match(guidance, /`board\.led` is absent/i);
  assert.match(guidance, /feature-detect[^\n]*`board\.devices\.led`/i);
  assert.match(guidance, /`board\.capabilities\.gpio`[^\n]*`board\.exposedPins`/i);
  assert.match(guidance, /never expect a graceful no-op/i);
  assert.doesNotMatch(guidance, /use `board\.led\(\)` returns gracefully/i);
});

const displayBoards = Object.freeze({
  "waveshare-lcd-1.28": {
    boardId: "waveshare_rp2040_touch_lcd_1.28",
    peripheralClockHz: 125000000,
  },
  "waveshare-lcd-1.47": {
    boardId: "waveshare_rp2350_lcd_1.47_a",
    peripheralClockHz: 150000000,
  },
  "waveshare-lcd-1.69": {
    boardId: "waveshare_rp2350_touch_lcd_1.69",
    peripheralClockHz: 150000000,
  },
});

function actualRpSpiRate(peripheralClockHz, requestedHz) {
  let best = 0;
  for (let prescale = 2; prescale <= 254; prescale += 2) {
    for (let postdiv = 1; postdiv <= 256; postdiv += 1) {
      const rate = Math.floor(peripheralClockHz / (prescale * postdiv));
      if (rate <= requestedHz && rate > best) best = rate;
    }
  }
  return best;
}

function actualRpI2cRate(peripheralClockHz, requestedHz) {
  const period = Math.floor((peripheralClockHz + Math.floor(requestedHz / 2)) / requestedHz);
  return Math.floor(peripheralClockHz / period);
}

function numericProperty(objectSource, name) {
  const match = objectSource.match(new RegExp(`\\b${name}\\s*:\\s*(\\d+)\\b`));
  return match ? Number(match[1]) : undefined;
}

test("Waveshare display examples use declared routes and exact RP bus rates", () => {
  const failures = [];
  for (const [directory, config] of Object.entries(displayBoards)) {
    const descriptor = boardDescriptors[config.boardId];
    const routes = descriptor.capabilities.spi?.routes ?? [];
    const directoryPath = join(examplesRoot, directory);
    for (const path of javascriptFiles(directoryPath)) {
      const source = readFileSync(path, "utf8");
      for (const match of source.matchAll(/\{[^{}]*\bspiBus\s*:\s*\d+[^{}]*\}/gs)) {
        const route = {
          bus: numericProperty(match[0], "spiBus"),
          sck: numericProperty(match[0], "sck"),
          mosi: numericProperty(match[0], "mosi"),
          miso: numericProperty(match[0], "miso"),
        };
        if (Object.values(route).some((value) => value === undefined)) continue;
        const declared = routes.some((candidate) =>
          candidate.bus === route.bus && candidate.sck === route.sck &&
          candidate.mosi === route.mosi && candidate.miso === route.miso);
        if (!declared) {
          failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: SPI route is not declared: ${JSON.stringify(route)}`);
        }
      }

      const ratePatterns = [
        /(?:^|[^A-Za-z0-9_$])SPI\.init\s*\([^\n)]*,\s*(\d+)\s*\)/g,
      ];
      if (source.includes("SPI.init")) {
        ratePatterns.push(/\bbaudrate\s*=\s*options\.baudrate\s*\|\|\s*(\d+)\b/g);
      }
      for (const expression of ratePatterns) {
        for (const match of source.matchAll(expression)) {
          const requested = Number(match[1]);
          const actual = actualRpSpiRate(config.peripheralClockHz, requested);
          if (actual !== requested) {
            failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: SPI rate ${requested} is not exactly representable; RP SDK selects ${actual}`);
          }
        }
      }

      const i2cRoutes = descriptor.capabilities.i2c?.routes ?? [];
      for (const match of source.matchAll(/I2C\.init\s*\(\s*([^,\n]+),\s*([^,\n]+),\s*([^,\n]+),\s*(\d+)\s*\)/g)) {
        const requested = Number(match[4]);
        const actual = actualRpI2cRate(config.peripheralClockHz, requested);
        if (actual !== requested) {
          failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: I2C rate ${requested} is not exactly representable; RP SDK selects ${actual}`);
        }
      }
      if (source.includes("I2C.init")) {
        for (const match of source.matchAll(/\bbaudrate\s*=\s*options\.baudrate\s*\|\|\s*(\d+)\b/g)) {
          const requested = Number(match[1]);
          const actual = actualRpI2cRate(config.peripheralClockHz, requested);
          if (actual !== requested) {
            failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: I2C default rate ${requested} is not exactly representable; RP SDK selects ${actual}`);
          }
        }
      }
      for (const match of source.matchAll(/\{[^{}]*\bi2cBus\s*:\s*\d+[^{}]*\}/gs)) {
        const route = {
          bus: numericProperty(match[0], "i2cBus"),
          sda: numericProperty(match[0], "sda"),
          scl: numericProperty(match[0], "scl"),
        };
        if (Object.values(route).some((value) => value === undefined)) continue;
        const declared = i2cRoutes.some((candidate) =>
          candidate.bus === route.bus && candidate.sda === route.sda && candidate.scl === route.scl);
        if (!declared) {
          failures.push(`${relative(root, path)}:${lineFor(source, match.index)}: I2C route is not declared: ${JSON.stringify(route)}`);
        }
      }
    }
  }

  assert.deepEqual(failures, [], failures.join("\n"));
});
