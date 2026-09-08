# Changelog

All notable changes to mcujs will be documented in this file.

## [Unreleased — 0.2.0]

Development work toward the portable API release. **Not a published release or
qualified release candidate.** `version.txt` still reads `0.1.0`; check the
installed firmware build ID and `board.apiVersion` before using these APIs.

### Added

- Seeed Studio XIAO ESP32-S3 support alongside the existing RP2040/RP2350
  targets: persistent script storage, boot-script recovery, native USB CDC/MSC,
  the shared interactive REPL, and board-qualified UF2 application updates.
- Canonical `require('board')`, immutable `board.apiVersion`, semantic pins,
  onboard-device inventory, and per-module capability discovery.
  `require('mcujs:module').has(name)` enables feature detection without board-name
  branches. Generated capability manifests describe each firmware image.
- Read-only `board.buttonPressed()` on Pico (BOOTSEL) and XIAO ESP32-S3 (BOOT),
  plus a portable, bounded button-to-LED example and a first-light lesson.
  BOOT/BOOTSEL remain recovery controls; they are not general-purpose outputs.
- Registry-backed `.capabilities` / `.capabilities NAME` commands and
  board-specific `.help`, including button help only on supported boards.
- Registry-backed graphics/screen discovery on RP firmware and DVI discovery
  on the DVI-enabled PiZero image. Method/limit descriptors and physical panel
  inventory are independent of image decoding; XIAO omits these display APIs.
  This exposes the existing display stack, not a newly normalized portable API.
- Portable contract/schema validation, native backend conformance tests,
  board-specific REPL tests, and electrical acceptance protocols.

### Changed / breaking

- Firmware selects an explicit feature map. Unsupported modules, optional
  methods, and onboard shortcuts are absent rather than stubs or no-ops.
  An external peripheral driver does not imply an onboard device exists.
- GPIO requires initialization and strict booleans for writes. Peripheral
  takeover invalidates stale GPIO state; explicitly reinitialize GPIO after
  the peripheral releases the pin.
- PWM duty uses only a finite `0..1` ratio. Out-of-range values throw instead of
  clamping; in-range duty/frequency requests that cannot be represented exactly
  throw `ERR_NOT_SUPPORTED`. Resource conflicts and exhaustion are explicit.
- I2C/SPI options use advertised routes and transfer limits. Bytes, lengths,
  addresses, modes, and frequencies are validated before native conversion;
  oversized transfers are not truncated. SPI uses fixed 8-bit words and
  MSB-first bit order, with supported modes and transfer limits reported per
  backend.
- ADC uses discovered pins/channels and support-gated voltage/temperature
  methods. Voltage is in volts and temperature in degrees Celsius. RP-only
  `adc.TEMP` / `adc.VSYS` aliases are not portable channel discovery.
- External NeoPixel options, RGB bytes, pixel counts, ordering, pin ownership,
  and operational errors follow a shared strict contract. Onboard multi-pixel
  shortcuts reject oversized input rather than silently discarding it.
- Programming errors remain `TypeError` / `RangeError`; migrated peripheral
  operations expose stable error codes for contention, unsupported native
  configurations, resource exhaustion, missing I2C devices, and I/O failures.
- Filesystem access observes host/device ownership: eject host-mounted storage
  before runtime access rather than allowing concurrent writers.

### Compatibility and build tooling

- Existing globals and documented positional I2C/SPI forms remain compatibility
  aliases through 0.x where supported; new code should use canonical lower-case
  modules and feature detection. RP SPI DMA and boot recovery extensions remain
  explicitly capability-gated, not universal portable APIs.
- USB HID and image-decoder metadata report current-image capabilities without
  promising portable graphics/display APIs or HID on ESP32-S3.
- Unified Docker build entrypoints support pinned offline builder images,
  source build identity, board manifests, and validated XIAO partition/UF2
  packaging. Factory recovery, application updates, and project storage remain
  separate operations.
- Pushes and PRs targeting `development` run source checks and docs builds.
  Development does not upload/deploy Pages or automatically publish releases.

### Verification and remaining limitations

- The same button example ran on Pico and XIAO firmware `0.1.0+5fc1294`:
  press/release events were recorded, the demos completed, final LED readings
  were off, and temporary scripts were removed. This is not an all-peripheral
  or all-board qualification.
- Native REPL checks cover all ten shipping board configurations, including
  presence/absence of button help. The newer help change has not yet been
  flashed to those two boards.
- A read-only discovery pass on those installed images matched all 14 Pico
  modules, all 11 XIAO modules, and nine capability descriptors on each board
  against their build's registry. Module loading, `has()`, help's module list,
  board metadata, and the checked optional board methods agreed. Storage
  remained device-ready and empty; no peripheral outputs or project files
  were written during this pass.
- Full exact-candidate builds, per-board hardware discovery/recovery/storage
  qualification, and electrical peripheral measurements remain release gates.
  Host/native tests and CI do not substitute for those measurements.
- Development commit `4a2bb00` built successfully for all ten shipping targets
  in pinned, network-isolated containers. Every UF2 application payload matched
  its binary, and all binaries/ELFs contained the expected build ID. A local
  packaging test verified ten firmware files, ten capability manifests, their
  checksums, and archive contents. These artifacts were neither flashed nor
  published; final 0.2.0 candidate qualification remains open.
- The PWM fade example exists, but XIAO's onboard LED pin (GPIO21) is not in its
  current PWM capability; do not assume a no-wiring fade works on both boards.
- Portable graphics/display normalization and resource-handle API redesign are
  outside the 0.2.0 scope.

See the [0.1 → 0.2 migration guide](docs/docs/migration/0.2.md) for code changes
and the [release checklist](docs/docs/development/mcujs-0.2-portable-api.md#020-release-checklist)
for current evidence and open gates. `main`, tags, and publishing require
separate release approval.

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

#### Raspberry Pi Docker direct-input pins

The root `Dockerfile` now fixes the direct source inputs selected on 2026-09-05.
The intended versions remain Alpine 3.19, Pico SDK 2.2.0, picotool 2.2.0,
JerryScript 3.0.0, FatFs R0.16, and picojpeg 1.1. No APK package names or version
constraints changed. PicoDVI did not previously select a version; its current
upstream commit is now explicit. These selections do not reproduce an unknown
historical builder image: the old Alpine tag, Git tags, and unpinned branches
could have resolved differently when that image was built.

| Input | Selected identity | Authoritative HTTPS acquisition source |
|-------|-------------------|----------------------------------------|
| Alpine 3.19 | `sha256:6baf43584bcb78f2e5847d1de515f23499913ac9f12bdf834811a3145eb11ca1` | [Docker Official Image registry index](https://registry-1.docker.io/v2/library/alpine/manifests/3.19) |
| Pico SDK 2.2.0 | `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779` | [raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk/tree/a1438dff1d38bd9c65dbd693f0e5db4b9ae91779) |
| picotool 2.2.0 | `a7eb3988f0645239185fadb4e25d8279478c2dbb` | [raspberrypi/picotool](https://github.com/raspberrypi/picotool/tree/a7eb3988f0645239185fadb4e25d8279478c2dbb) |
| JerryScript 3.0.0 | `50200152feb724a74a5f64e44d7885151537cfad` | [pando-project/jerryscript](https://github.com/pando-project/jerryscript/tree/50200152feb724a74a5f64e44d7885151537cfad) |
| FatFs R0.16 | SHA-256 `99f7dc1f7e095356e4a9e3dbe29959090d8b948afe2bbc5441e52fdf4b85449e` | [Elm Chan's ff16.zip](https://elm-chan.org/fsw/ff/arc/ff16.zip) |
| picojpeg 1.1 | `8ab33a909b4115ace4a952b7ffcb64c350f9298d` | [richgel999/picojpeg](https://github.com/richgel999/picojpeg/tree/8ab33a909b4115ace4a952b7ffcb64c350f9298d) |
| PicoDVI (previously unversioned) | `dccd738bfa9af75badcb32acde3e41bd6a3fa30a` | [Wren6991/PicoDVI](https://github.com/Wren6991/PicoDVI/tree/dccd738bfa9af75badcb32acde3e41bd6a3fa30a) |

The Alpine digest is the multi-platform OCI index, not an amd64-only manifest.
It was calculated from the registry response bytes and matched its
`Docker-Content-Digest` header; no image layers were pulled. Each Git selection
was resolved through the upstream GitHub commits API and matched `git ls-remote`
over HTTPS (the named release tags above, or `master` for picojpeg and PicoDVI).
The picojpeg source still identifies itself as v1.1. FatFs was downloaded from
the author's HTTPS site, checked as a ZIP, and identified as R0.16 in `source/ff.h`.
Its SHA-256 was calculated locally from those bytes, not taken from a separately
signed upstream checksum. Future downloads must pass that checksum before unzip.
This upstream identity/checksum evidence is historical; the software-only lexical
checker corrections did not repeat network acquisition or upstream verification.

Git fetches request full commits, detach the checkout, and verify `HEAD`.
Pico SDK submodules remain at the gitlinks recorded by the selected SDK; the
build does not use `submodule update --remote`. Existing `/opt` paths and license
notices are preserved. picojpeg now uses a complete upstream checkout rather
than two files downloaded from `master`.

Run `node scripts/check-rp-docker-inputs.js --self-test` before rebuilding the
Raspberry Pi builder. The dependency-free check validates this Dockerfile's
closed instruction/command set, checks shell syntax with `sh -n`, and rejects
deliberately weakened copies, including a new RUN after a comment ending in a
backslash. Full-line comments are removed before folding instruction continuations.
Leading UTF-8 BOMs are rejected before directive checks or comment removal so
BOM-prefixed syntax and escape directives cannot be hidden. Before discarding a
comment, the check rejects syntax/escape/check directives in the same
leading-whitespace-trimmed representation. This conservatively excludes directive
forms, including indented escape directives that change continuation semantics;
it does not imply that every rejected form selects an external frontend.
Self-tests cover space/tab/mixed indentation and case/spacing/CRLF variants.
It accepts an optional Dockerfile path for review.
Hashes remain in Dockerfile; changing acquisition commands requires updating
and reviewing the focused check. This is not a general Dockerfile linter.

This is direct-input pinning, not complete transitive package reproducibility.
`apk add` still resolves packages from Alpine repositories; package availability,
toolchain versions, transitive build downloads, and resulting image/firmware
bytes are not locked or proven reproducible here. No host packages or SDKs were
installed. Image construction, all supported firmware targets, and host-platform
compatibility still require the separate post-review build checks. The ESP32
Docker build is unchanged.
