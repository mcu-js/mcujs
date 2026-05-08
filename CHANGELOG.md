# Changelog

All notable changes to mcujs will be documented in this file.

## [0.1.0] - 2026-05-08

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
- `screen` - Unified display API for DVI, LCD, OLED displays
  - `screen.init(driver)` - Initialize with display driver
  - `screen.fill(color)` - Fill screen with color
  - `screen.fillRect(x, y, w, h, color)` - Draw filled rectangle
  - `screen.fillCircle(cx, cy, r, color)` - Draw filled circle
  - `screen.drawLine(x0, y0, x1, y1, color)` - Draw line
  - `screen.drawText(x, y, text, color, size)` - Draw text (5x7 font)
  - `screen.show()` - Flush framebuffer to display
  - Preset colors: `BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `CYAN`, `MAGENTA`, `YELLOW`, `ORANGE`, `GRAY`
- `DVI` - Native DVI/HDMI output for RP2040-PiZero (640x480@60Hz via 160x120 framebuffer)
- `graphics` - RGB565 framebuffer for displays (fill, rect, pixel, line, circle, text)
- `image` - JPEG and BMP image decoding to graphics buffers
  - `image.drawJPEG(handle, path, {x, y})` - Load and decode JPEG from filesystem
  - `image.drawBMP(handle, path, {x, y})` - Load and decode BMP from filesystem
  - Supports: Baseline JPEG, 16/24/32-bit uncompressed BMP
  - Max file size: 16KB (RP2040), 192KB (RP2350)
- `keyboard` - USB HID keyboard emulation
  - `keyboard.print(text)` - Type a string (handles shift for uppercase/symbols)
  - `keyboard.tap(key)` - Press and release a key
  - `keyboard.press(key)` / `keyboard.release(key)` - Hold/release for combos
  - `keyboard.releaseAll()` - Release all keys (safety)
  - `keyboard.isPressed(key)` - Check if a key is pressed
  - Supported keys: letters, numbers, F1-F12, modifiers (ctrl, shift, alt, super/gui/cmd/win), arrows, enter, tab, space, backspace, delete, escape, home, end, pageup, pagedown, printscreen, and more
  - Media keys: `mute`, `volumeup`, `volumedown`, `playpause`, `nexttrack`, `prevtrack`, `stop`, `brightnessup`, `brightnessdown`
- `mouse` - USB HID mouse emulation
  - `mouse.move(x, y)` - Move cursor relative to current position
  - `mouse.click(button)` - Click a button (default: left)
  - `mouse.doubleClick(button)` - Double-click
  - `mouse.press(button)` / `mouse.release(button)` - Hold/release for drag
  - `mouse.scroll(amount)` - Vertical scroll
  - `mouse.scrollH(amount)` - Horizontal scroll
  - `mouse.releaseAll()` - Release all buttons
  - Buttons: `left`, `right`, `middle`
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
- **Stack traces**: Errors show function names, source file, and line/column numbers
- **"Did you mean?" suggestions**: Typos show helpful suggestions
  - `consoel` → "Did you mean 'console'?"
  - `console.logg()` → "Did you mean 'console.log'?"
  - `Math.flor()` → "Did you mean 'Math.floor'?"
  - `require('gipo')` → "Did you mean 'gpio'?"
- **Pretty-printed output**: Objects and arrays display as formatted JSON instead of `[object Object]`
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
- Raspberry Pi Pico 2 W (RP2350, 4MB flash, CYW43 LED support)
- Waveshare RP2040-Zero (RP2040, 2MB flash, NeoPixel on GPIO 16)
- Waveshare RP2040-PiZero (RP2040, 16MB flash, DVI/HDMI output, MicroSD slot)
- Waveshare RP2040 Touch LCD 1.28" (RP2040, 4MB flash, round display, touch, IMU)
- Waveshare RP2350-LCD-1.47-A (RP2350, 16MB flash, ST7789V3 320x172 display, NeoPixel)
- Waveshare RP2350-Touch-LCD-1.69 (RP2350, 16MB flash, ST7789V2 240x280 display, touch, IMU, buzzer)
- Adafruit Feather RP2040 (RP2040, 8MB flash, NeoPixel, STEMMA QT)

### Build System
- Docker-based build for reproducibility
- Alpine Linux image (~1.3GB)
- Central board registry shared by build, docs, verification, and release packaging
- Deterministic release scripts for all supported board UF2s, manifests, checksums, and tarballs
- Prebuilt picotool v2.2.0 in the Docker builder to avoid network fetches during firmware builds
- Support for RP2040 and RP2350 release boards

### Third-Party Libraries
- **Pico SDK** v2.2.0 - Hardware abstraction and TinyUSB integration
- **JerryScript** v3.0.0 - JavaScript engine
- **FatFs** R0.16 - FAT filesystem (Elm Chan)
- **picojpeg** v1.1 - JPEG decoder (Rich Geldreich, Public Domain)
- **PicoDVI** - DVI/HDMI output library (Luke Wren, BSD-3-Clause)
- **TinyUSB** - USB stack (via Pico SDK)
- BMP decoder - Custom implementation (16/24/32-bit uncompressed)
