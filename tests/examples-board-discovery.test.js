const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const { join } = require("node:path");
const test = require("node:test");

const root = join(__dirname, "..");
const examplePath = join(root, "examples/board-discovery/index.js");

test("board discovery example feature-detects without board-name sniffing", () => {
  const source = readFileSync(examplePath, "utf8");
  assert.match(source, /require\(['"]board['"]\)/);
  assert.match(source, /require\(['"]mcujs:module['"]\)/);
  assert.match(source, /boardApi\.pins/);
  assert.match(source, /boardApi\.devices/);
  assert.match(source, /boardApi\.capabilities\(\)/);
  assert.doesNotMatch(source, /boardApi\.name\s*={2,3}/);
  assert.doesNotMatch(source, /switch\s*\(\s*boardApi\.name/);
});
