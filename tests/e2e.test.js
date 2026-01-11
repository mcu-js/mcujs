import { describe, test, beforeAll, afterAll, expect, setDefaultTimeout } from "bun:test";
import {
  expectedBuildId,
  buildFirmware,
  findSerialPort,
  waitForSerial,
  waitForSerialDisconnect,
  setupSerial,
  openReader,
  sendCommand,
  sendRaw,
  readUntil,
  waitForLabel,
  waitForLabelDisconnect,
  waitForLabelAvailable,
  ensureMount,
  findMount,
  hasLabel,
  copyUf2,
  writeFile,
  renameFile,
  syncFilesystem,
  removeFile,
  ensureDir,
} from "./helpers/device.js";
import { join } from "path";

setDefaultTimeout(30000);

const prompt = "> ";
const files = {
  hello: "helloWorld.js",
  helloRenamed: "helloWorldRenamed.js",
  lib: "libTest.js",
  libRenamed: "libTestRenamed.js",
  paste: "paste.js",
};

let port;
let reader;

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const waitForPrompt = () => readUntil(reader, (buffer) => buffer.endsWith(prompt));

async function openSerial() {
  for (let attempt = 0; attempt < 5; attempt += 1) {
    port = findSerialPort() ?? (await waitForSerial());
    try {
      await setupSerial(port);
      reader = openReader(port);
      await sendCommand(port, reader, "\x03", prompt);
      await sendCommand(port, reader, ".help", prompt);
      return;
    } catch (error) {
      if (attempt === 4) {
        throw error;
      }
      await delay(300);
    }
  }
}

async function resetUsb() {
  await sendRaw(port, ".usbreset\r");
  reader.releaseLock();
  port = null;
  reader = null;
  await waitForSerialDisconnect();
  await waitForLabelDisconnect("MCUJS");
  await waitForLabelAvailable("MCUJS");
  await waitForSerial();
  await openSerial();
}

async function ensureRuntimeFlashed() {
  buildFirmware();

  if (hasLabel("RPI-RP2")) {
    const uf2Mount = ensureMount("RPI-RP2");
    copyUf2(uf2Mount);
    await waitForLabel("MCUJS");
    ensureMount("MCUJS");
  }

  await openSerial();

  const info = await sendCommand(port, reader, ".info", prompt);
  if (!info.includes(expectedBuildId())) {
    await sendRaw(port, ".uf2!\r");
    reader.releaseLock();
    port = null;
    reader = null;
    await waitForLabel("RPI-RP2");
    const uf2Mount = ensureMount("RPI-RP2");
    copyUf2(uf2Mount);
    await waitForLabel("MCUJS");
    ensureMount("MCUJS");
    await openSerial();
    const updatedInfo = await sendCommand(port, reader, ".info", prompt);
    if (!updatedInfo.includes(expectedBuildId())) {
      throw new Error("Flashed firmware build ID mismatch");
    }
  }
}

beforeAll(async () => {
  await ensureRuntimeFlashed();
});

afterAll(() => {
  if (reader) {
    reader.releaseLock();
  }
  const mcujsMount = findMount("MCUJS");
  if (!mcujsMount) {
    return;
  }
  removeFile(join(mcujsMount, files.hello));
  removeFile(join(mcujsMount, files.helloRenamed));
  removeFile(join(mcujsMount, files.paste));
  removeFile(join(mcujsMount, "lib", files.lib));
  removeFile(join(mcujsMount, "lib", files.libRenamed));
});

describe("repl info", () => {
  let helpOutput = "";
  let infoOutput = "";

  beforeAll(async () => {
    helpOutput = await sendCommand(port, reader, ".help", prompt);
    infoOutput = await sendCommand(port, reader, ".info", prompt);
  });

  test("help includes usbreset", () => {
    expect(helpOutput).toContain(".usbreset");
  });

  test("help includes uf2", () => {
    expect(helpOutput).toContain(".uf2");
  });

  test("info includes build", () => {
    expect(infoOutput).toContain(`Build: ${expectedBuildId()}`);
  });

  test("info includes board", () => {
    expect(infoOutput).toContain("Board:");
  });

  test("info includes chip", () => {
    expect(infoOutput).toContain("Chip:");
  });

  test("info includes flash", () => {
    expect(infoOutput).toContain("Flash:");
  });

  test("info includes ram", () => {
    expect(infoOutput).toContain("RAM:");
  });
});

describe("repl values", () => {
  let numbersOutput = "";
  let booleansOutput = "";
  let stringsOutput = "";
  let arraysOutput = "";
  let objectsOutput = "";
  let jsonParseOutput = "";

  beforeAll(async () => {
    numbersOutput = await sendCommand(
      port,
      reader,
      "console.log(1);\rconsole.log(-2);\rconsole.log(1.5);",
      prompt,
    );
    booleansOutput = await sendCommand(
      port,
      reader,
      "console.log(true);\rconsole.log(false);\rconsole.log(undefined);",
      prompt,
    );
    stringsOutput = await sendCommand(
      port,
      reader,
      "console.log(\"hello world\");\rconsole.log(\"spaced value\");",
      prompt,
    );
    arraysOutput = await sendCommand(
      port,
      reader,
      "console.log(JSON.stringify([1,2,\"a\"]));",
      prompt,
    );
    objectsOutput = await sendCommand(
      port,
      reader,
      "console.log(JSON.stringify({a:1,b:\"x\"}));",
      prompt,
    );
    jsonParseOutput = await sendCommand(
      port,
      reader,
      "console.log(JSON.stringify(JSON.parse(\"{\\\"a\\\":1}\")));",
      prompt,
    );
  });

  test("prints integer", () => {
    expect(numbersOutput).toMatch(/\b1\b/);
  });

  test("prints negative integer", () => {
    expect(numbersOutput).toMatch(/-2/);
  });

  test("prints float", () => {
    expect(numbersOutput).toMatch(/1\.5/);
  });

  test("prints true", () => {
    expect(booleansOutput).toContain("true");
  });

  test("prints false", () => {
    expect(booleansOutput).toContain("false");
  });

  test("prints undefined", () => {
    expect(booleansOutput).toContain("undefined");
  });

  test("prints string", () => {
    expect(stringsOutput).toContain("hello world");
  });

  test("prints spaced string", () => {
    expect(stringsOutput).toContain("spaced value");
  });

  test("prints array stringify", () => {
    expect(arraysOutput).toContain("[1,2,\"a\"]");
  });

  test("prints object stringify", () => {
    expect(objectsOutput).toContain("{\"a\":1,\"b\":\"x\"}");
  });

  test("prints JSON.parse output", () => {
    expect(jsonParseOutput).toContain("{\"a\":1}");
  });
});

describe("repl evaluation", () => {
  let expressionsOutput = "";
  let bareOutput = "";
  let undefinedOutput = "";

  beforeAll(async () => {
    expressionsOutput = await sendCommand(
      port,
      reader,
      "console.log(1 + 2);\rconsole.log(Math.max(3, 7));\rconsole.log(\"a\" + \"b\");\rconsole.log(3 > 2);",
      prompt,
    );
    bareOutput = await sendCommand(
      port,
      reader,
      "1 + 2\rMath.max(5, 9)\r\"hi\" + \"there\"\r3 > 2\r",
      prompt,
    );
    undefinedOutput = await sendCommand(port, reader, "var x = 1", prompt);
  });

  test("console evaluates 1 + 2", () => {
    expect(expressionsOutput).toContain("3");
  });

  test("console evaluates Math.max", () => {
    expect(expressionsOutput).toContain("7");
  });

  test("console evaluates string concat", () => {
    expect(expressionsOutput).toContain("ab");
  });

  test("console evaluates boolean", () => {
    expect(expressionsOutput).toContain("true");
  });

  test("bare evaluates 1 + 2", () => {
    expect(bareOutput).toContain("3");
  });

  test("bare evaluates Math.max", () => {
    expect(bareOutput).toContain("9");
  });

  test("bare evaluates string concat", () => {
    expect(bareOutput).toContain("hithere");
  });

  test("bare evaluates boolean", () => {
    expect(bareOutput).toContain("true");
  });

  test("bare var assignment does not print undefined", () => {
    expect(undefinedOutput).not.toContain("undefined");
  });
});

describe("paste execution", () => {
  let pasteOutput = "";

  beforeAll(async () => {
    await sendRaw(port, ".multiline\rconsole.log(\"paste-exec\");\r.end\r");
    pasteOutput = await waitForPrompt();
  });

  test("paste runs inline code", () => {
    expect(pasteOutput).toContain("paste-exec");
  });
});

describe("console output", () => {
  let output = "";
  let multiOutput = "";

  beforeAll(async () => {
    output = await sendCommand(
      port,
      reader,
      "console.log(\"log\");\rconsole.warn(\"warn\");\rconsole.error(\"error\");",
      prompt,
    );
    multiOutput = await sendCommand(
      port,
      reader,
      "console.log(\"a\", 1, true);",
      prompt,
    );
  });

  test("console.log output", () => {
    expect(output).toContain("log");
  });

  test("console.warn output", () => {
    expect(output).toContain("warn");
  });

  test("console.error output", () => {
    expect(output).toContain("error");
  });

  test("multi arg includes string", () => {
    expect(multiOutput).toContain("a");
  });

  test("multi arg includes number", () => {
    expect(multiOutput).toContain("1");
  });

  test("multi arg includes boolean", () => {
    expect(multiOutput).toContain("true");
  });
});

describe("modules", () => {
  let gpioOutput = "";
  let pwmOutput = "";
  let i2cOutput = "";
  let spiOutput = "";
  let builtinOutput = "";
  let aliasOutput = "";

  beforeAll(async () => {
    gpioOutput = await sendCommand(
      port,
      reader,
      "var GPIO = require(\"gpio\");\rconsole.log(typeof GPIO.init);",
      prompt,
    );
    pwmOutput = await sendCommand(
      port,
      reader,
      "var PWM = require(\"pwm\");\rconsole.log(typeof PWM.init);",
      prompt,
    );
    i2cOutput = await sendCommand(
      port,
      reader,
      "var I2C = require(\"i2c\");\rconsole.log(typeof I2C.init);",
      prompt,
    );
    spiOutput = await sendCommand(
      port,
      reader,
      "var SPI = require(\"spi\");\rconsole.log(typeof SPI.init);",
      prompt,
    );
    builtinOutput = await sendCommand(
      port,
      reader,
      "var mod = require(\"mcujs:module\");\rconsole.log(JSON.stringify(mod.builtinModules));",
      prompt,
    );
    aliasOutput = await sendCommand(
      port,
      reader,
      "console.log(require(\"mcujs:module\") === require(\"node:module\"));",
      prompt,
    );
  });

  test("gpio module exports functions", () => {
    expect(gpioOutput).toContain("function");
  });

  test("pwm module exports functions", () => {
    expect(pwmOutput).toContain("function");
  });

  test("i2c module exports functions", () => {
    expect(i2cOutput).toContain("function");
  });

  test("spi module exports functions", () => {
    expect(spiOutput).toContain("function");
  });

  test("builtin modules list includes fs", () => {
    expect(builtinOutput).toContain("fs");
  });

  test("builtin modules list includes gpio", () => {
    expect(builtinOutput).toContain("gpio");
  });

  test("builtin modules list includes adc", () => {
    expect(builtinOutput).toContain("adc");
  });

  test("node module alias matches", () => {
    expect(aliasOutput).toContain("true");
  });
});

describe("adc", () => {
  let pinOutput = "";
  let channelOutput = "";
  let voltageOutput = "";
  let tempOutput = "";
  let constantsOutput = "";

  beforeAll(async () => {
    pinOutput = await sendCommand(
      port,
      reader,
      "var adc = require(\"adc\");\rconsole.log(adc.readPin(26));",
      prompt,
    );
    channelOutput = await sendCommand(
      port,
      reader,
      "var adc = require(\"adc\");\rconsole.log(adc.readChannel(0));",
      prompt,
    );
    voltageOutput = await sendCommand(
      port,
      reader,
      "var adc = require(\"adc\");\rconsole.log(adc.readVoltagePin(26));",
      prompt,
    );
    tempOutput = await sendCommand(
      port,
      reader,
      "var adc = require(\"adc\");\rconsole.log(adc.readTempC());",
      prompt,
    );
    constantsOutput = await sendCommand(
      port,
      reader,
      "var adc = require(\"adc\");\rconsole.log(adc.TEMP);\rconsole.log(adc.VSYS);",
      prompt,
    );
  });

  test("readPin returns number", () => {
    expect(pinOutput).toMatch(/\d+/);
  });

  test("readChannel returns number", () => {
    expect(channelOutput).toMatch(/\d+/);
  });

  test("readVoltagePin returns number", () => {
    expect(voltageOutput).toMatch(/\d+(\.\d+)?/);
  });

  test("readTempC returns number", () => {
    expect(tempOutput).toMatch(/-?\d+(\.\d+)?/);
  });

  test("TEMP constant is 4", () => {
    expect(constantsOutput).toContain("4");
  });

  test("VSYS constant is 3", () => {
    expect(constantsOutput).toContain("3");
  });
});

describe("filesystem", () => {
  let runHelloOutput = "";
  let listOutput = "";
  let listRenamedOutput = "";
  let runRenamedOutput = "";
  let pasteRunOutput = "";
  let requireOutput = "";
  let requireRenamedOutput = "";
  let missingCatOutput = "";
  let missingRunOutput = "";
  let removeHelloOutput = "";
  let removeLibOutput = "";
  let listAfterRmOutput = "";

  beforeAll(async () => {
    const mcujsMount = ensureMount("MCUJS");
    const libDir = join(mcujsMount, "lib");
    ensureDir(libDir);

    writeFile(join(mcujsMount, files.hello), "console.log(\"hello\")\n");
    syncFilesystem();

    runHelloOutput = await sendCommand(port, reader, `.run /${files.hello}`, prompt);
    listOutput = await sendCommand(port, reader, ".ls", prompt);

    renameFile(join(mcujsMount, files.hello), join(mcujsMount, files.helloRenamed));
    syncFilesystem();

    listRenamedOutput = await sendCommand(port, reader, ".ls", prompt);
    runRenamedOutput = await sendCommand(port, reader, `.run /${files.helloRenamed}`, prompt);

    await sendRaw(port, `.multiline /${files.paste}\rconsole.log(\"paste\");\r.end\r`);
    await waitForPrompt();
    pasteRunOutput = await sendCommand(port, reader, `.run /${files.paste}`, prompt);

    writeFile(join(libDir, files.lib), "module.exports = { msg: \"hello-from-lib\" };\n");
    syncFilesystem();

    requireOutput = await sendCommand(
      port,
      reader,
      `var libMod = require(\"${files.lib.replace(".js", "")}\");\rconsole.log(libMod.msg);`,
      prompt,
    );

    renameFile(join(libDir, files.lib), join(libDir, files.libRenamed));
    syncFilesystem();

    requireRenamedOutput = await sendCommand(
      port,
      reader,
      `var libMod2 = require(\"${files.libRenamed.replace(".js", "")}\");\rconsole.log(libMod2.msg);`,
      prompt,
    );

    missingCatOutput = await sendCommand(port, reader, ".cat /missing.js", prompt);
    missingRunOutput = await sendCommand(port, reader, ".run /missing.js", prompt);

    removeHelloOutput = await sendCommand(port, reader, `.rm /${files.helloRenamed}`, prompt);
    removeLibOutput = await sendCommand(port, reader, `.rm /lib/${files.libRenamed}`, prompt);
    listAfterRmOutput = await sendCommand(port, reader, ".ls", prompt);
  });

  test(".run prints output", () => {
    expect(runHelloOutput).toContain("hello");
  });

  test(".ls lists file", () => {
    expect(listOutput).toContain(files.hello);
  });

  test(".ls lists renamed file", () => {
    expect(listRenamedOutput).toContain(files.helloRenamed);
  });

  test(".run after rename", () => {
    expect(runRenamedOutput).toContain("hello");
  });

  test(".multiline writes file", () => {
    expect(pasteRunOutput).toContain("paste");
  });

  test("require from lib works", () => {
    expect(requireOutput).toContain("hello-from-lib");
  });

  test("require from renamed lib works", () => {
    expect(requireRenamedOutput).toContain("hello-from-lib");
  });

  test("missing .cat returns error", () => {
    expect(missingCatOutput).toContain("Error");
  });

  test("missing .run returns error", () => {
    expect(missingRunOutput).toContain("Error");
  });

  test(".rm removes root file", () => {
    expect(removeHelloOutput).toContain("File removed");
  });

  test(".rm removes lib file", () => {
    expect(removeLibOutput).toContain("File removed");
  });

  test(".ls hides removed file", () => {
    expect(listAfterRmOutput).not.toContain(files.helloRenamed);
  });
});

describe("javascript APIs", () => {
  let boardInfoOutput = "";
  let ledPinOutput = "";
  let ledToggleOutput = "";
  let millisOutput = "";
  let processOutput = "";
  let timerOutput = "";

  beforeAll(async () => {
    boardInfoOutput = await sendCommand(port, reader, "console.log(board.name);\rconsole.log(board.chip);", prompt);
    ledPinOutput = await sendCommand(port, reader, "console.log(board.ledPin);", prompt);
    ledToggleOutput = await sendCommand(port, reader, "board.led(true);\rboard.led(false);", prompt);
    millisOutput = await sendCommand(port, reader, "console.log(board.millis());\rconsole.log(board.millis());", prompt);
    processOutput = await sendCommand(port, reader, "console.log(process.version);\rconsole.log(JSON.stringify(process.versions));", prompt);

    await sendRaw(port, "setTimeout(() => console.log(\"timer-done\"), 150);\r");
    timerOutput = await sendCommand(port, reader, "", "timer-done");
  });

  test("board name", () => {
    expect(boardInfoOutput).toContain("pico");
  });

  test("board chip", () => {
    expect(boardInfoOutput).toContain("RP");
  });

  test("board led pin", () => {
    expect(ledPinOutput).toMatch(/\d+/);
  });

  test("board led toggle", () => {
    expect(ledToggleOutput).not.toContain("Error:");
  });

  test("board millis returns number", () => {
    expect(millisOutput).toMatch(/\d+/);
  });

  test("process version includes mcujs", () => {
    expect(processOutput).toContain("mcujs");
  });

  test("process versions includes jerryscript", () => {
    expect(processOutput).toContain("jerryscript");
  });

  test("setTimeout fires", () => {
    expect(timerOutput).toContain("timer-done");
  });
});

describe("usbreset", () => {
  let helpOutput = "";

  beforeAll(async () => {
    await resetUsb();
    ensureMount("MCUJS");
    helpOutput = await sendCommand(port, reader, ".help", prompt);
  });

  test("usbreset reconnects", () => {
    expect(helpOutput).toContain(".usbreset");
  });
});
