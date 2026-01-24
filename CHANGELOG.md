# Changelog

All notable changes to mcujs will be documented in this file.

## [0.1.0] - Unreleased

### Core Features
- JavaScript runtime using JerryScript v3.0.0
- USB CDC serial REPL with multi-line input support
- USB MSC mass storage for drag-and-drop file transfer
- FAT12 filesystem on flash using remaining space after firmware
- **Auto-format**: Filesystem automatically formats on first boot or if corruption is detected
- Auto-run `/index.js` on boot
- **BOOTSEL Safe Mode**: Hold BOOTSEL button during power-on to skip `index.js` auto-run (allows recovery from infinite loops)
- **Boot Status Indicator**: LED or NeoPixel blinks on startup to show boot progress (3 blinks = success, rapid blinks = error)

### Hardware APIs
- `GPIO` - Digital input/output with pull-up/pull-down support
- `PWM` - Pulse width modulation
- `I2C` - I2C bus communication
- `SPI` - SPI bus communication with DMA support (`SPI.writeBufferDMA()`)
- `ADC` - Analog-to-digital converter
- `NeoPixel` - WS2812B addressable LED support with configurable wire order (RGB/GRB)
- `graphics` - RGB565 framebuffer for displays (fill, rect, pixel, line, circle, text)
- `board` object - LED control, NeoPixel helper, system info, unique ID, memory stats
- `mcujs:module` - Built-in module registry

### JavaScript APIs
- `console.log()`, `console.warn()`, `console.error()` - Serial output
- `setTimeout()` / `setInterval()` - Timers with `clearTimeout()` / `clearInterval()`
- `process` object - Node.js-compatible process information:
  - `process.version` - mcujs version (e.g., "v0.1.0")
  - `process.arch` - CPU architecture (e.g., "RP2040", "RP2350")
  - `process.platform` - Always "mcujs"
  - `process.versions` - Object with dependency versions

### Filesystem (`fs` module)
Node.js-compatible synchronous filesystem API with subdirectory support:
- `fs.readFileSync(path)` - Read file contents as string
- `fs.writeFileSync(path, data)` - Write string to file
- `fs.appendFileSync(path, data)` - Append string to file
- `fs.existsSync(path)` - Check if file/directory exists
- `fs.unlinkSync(path)` - Delete a file
- `fs.readdirSync(path)` - List directory contents (any path, not just root)
- `fs.statSync(path)` - Get file info (size, isFile, isDirectory)
- `fs.renameSync(oldPath, newPath)` - Rename/move a file
- `fs.mkdirSync(path)` - Create a directory

### CommonJS Module System
Full `require()` implementation for modular code:
- Relative imports: `require('./utils')`, `require('../lib/helper')`
- Absolute imports: `require('/config')`
- Bare module imports: `require('math')` searches `/lib/math.js`
- JSON imports: `require('./config.json')` or `require('config')` (tries .js then .json)
- `exports.foo = ...` and `module.exports = ...` patterns
- `__filename` and `__dirname` available in modules
- `require.cache` for inspecting loaded modules
- Module caching (modules only loaded once)

### REPL Features
- Command history with Up/Down arrow keys (8 entries stored)
- Line editing with Left/Right arrows, Home/End, Backspace anywhere
- Tab completion for global objects and properties (includes REPL-defined globals)
- Built-in commands:
  - `.help` - Show available commands
  - `.info` - Show board info (chip, memory, filesystem, build)
  - `.ls` - List files on the device with sizes
  - `.cat FILE` - Display file contents
  - `.rm FILE` - Delete a file
  - `.run FILE` - Execute a JavaScript file
  - `.multiline [FILE]` - Multi-line input (end with `.end`)
  - `.format` - Format filesystem (prompted, 3s countdown)
  - `.format!` - Format filesystem immediately
  - `.uf2` - Reboot into UF2 mode (prompted)
  - `.uf2!` - Reboot into UF2 mode immediately
  - `.usbreset` - Reset USB connection (reboot)


### Board Support
- Raspberry Pi Pico (RP2040, 2MB flash)
- Raspberry Pi Pico 2 (RP2350, 4MB flash)
- Waveshare RP2040-Zero (RP2040, 2MB flash, NeoPixel on GPIO 16)
- Waveshare RP2040 Touch LCD 1.28" (RP2040, 4MB flash, round display, touch, IMU)

### Build System
- Docker-based build for reproducibility
- Alpine Linux image (~1.3GB)
- Support for Raspberry Pi Pico (RP2040) and Pico 2 (RP2350)
