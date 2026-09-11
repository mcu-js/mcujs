# Changelog

All notable changes to mcujs will be documented in this file.

## [Unreleased — 0.2.0]

- Added a bounded baseline JPEG reader and portable Canvas example: a native
  snapshot up to 16 KiB, dimensions up to 320x320, and one RGB block per read.
  The existing picojpeg decoder is reused without display ownership, raw
  graphics handles or a second full-frame pixel buffer. Legacy methods remain
  until their remaining callers migrate. New firmware is required; see
  [limits and qualification](docs/docs/development/binary-file-assets.md#baseline-jpeg-example).

- Extended the row-wise Canvas BMP example to explicit 16-bit RGB565 bitfields
  and 32-bit BI_RGB (unused fourth byte ignored). Input stays bounded; mask,
  truncation and cleanup tests accompany real format fixtures. Corrected the
  previously mislabeled 16-bit asset. Native decoders and JPEG remain unchanged.
  See [supported formats](docs/docs/development/binary-file-assets.md#image-example).

- Migrated the bouncing-balls demo to configured `devices` + Canvas circles:
  five colored balls, dimension-aware positions, bounded edge collisions and
  frame text. A separate 30-second timer releases the display instead of leaving
  a persistent `Done!` image. No runtime or firmware change. See the
  [demo contract](docs/docs/development/display-canvas.md#bounded-bouncing-balls-animation).

- Added bounded Canvas `arc()` for filled and outlined circles and partial arcs:
  radius 0–128, at most 32 straight segments per turn, retained paths and the
  existing native renderer. No new native interface or legacy API removal.
  See [limits and migration decisions](docs/docs/development/display-canvas.md#bounded-circles-and-arcs).

- Migrated the existing PiZero-folder rainbow demo to configured `devices` +
  Canvas: scrolling bars and frame text, no DVI globals or RGB565 plumbing.
  It closes after 30 seconds instead of leaving a persistent `Done!` screen.
  No firmware or legacy binding change. See [scope and hardware limits](docs/docs/development/display-canvas.md#bounded-rainbow-animation).

- Added bounded Canvas bitmap text: `fillText`, width-only `measureText`, and
  1x–4x `monospace` fonts with saved state, clipping and printable ASCII limits.
  No new native interface or legacy binding removal. Newlines become spaces;
  multiline labels use separate calls. See the [text contract](docs/docs/development/display-canvas.md#bounded-bitmap-text).

- Made `devices` + `display.canvas` the documented default for drawing. The
  existing `display-canvas-demo.js` is now directly runnable (not an exported
  helper), closes after 20 seconds, and passed repeat/close checks with visible
  geometry on the attached 1.47-A and 1.69 LCDs without firmware changes.
  Deprecated `screen`, `graphics` and public `image` for new code; removal is
  before v1.0, gated on useful behavior replacements or explicit retirement.
  No bindings or decoders are removed yet. See the
  [migration audit and physical limits](docs/docs/development/display-canvas.md#legacy-behavior-audit-and-removal-gate).

- Added `devices.microphone` capture on ePaper 1.54 V2: explicit bounded PCM
  recording, one owner, AbortSignal/stop/close and native audio-rail shutdown
  independent of JavaScript. The onboard mic captured a known three-tone chime;
  physical cancellation/reopen and stalled-JS capture checks passed. No background
  capture or upload. See [microphone contract and evidence](docs/docs/microphone.md).

- Added configured `devices.speaker` on RP2350 Touch LCD 2.8: bounded native
  PCM WAV (16-bit mono, 16000 Hz) streaming, linear volume, Promise completion,
  cancellation, stop/close, and fail-silent PIO/native underrun handling.
  No mixing, microphone or SD qualification. The operator-heard chime,
  recorded cancellation/replay, large-file streaming and reset checks passed;
  see [speaker contract and evidence](docs/docs/speaker.md).

- Added `devices.buzzer` on RP2350 Touch LCD 1.69: one exclusive tone handle,
  bounded exact-representable frequencies and durations, Promise completion,
  AbortSignal cancellation, stop/close and native-alarm shutdown independent of
  JavaScript servicing. Other boards omit this capability; no speaker/mic API.
  GPIO2 is now reserved from generic GPIO access. `events.AbortSignal.subscribe`
  provides bounded cancellation callbacks independent of event propagation.
  See [buzzer contract](docs/docs/buzzer.md) for limits and hardware qualification.


- Bounded binary file handles on `/app` and configured `/sd`: Uint8Array
  `openSync`/`readSync`/`writeSync`/`closeSync`, explicit ownership/error handling,
  four-handle and 4096-byte-transfer limits, and cleanup on VM teardown.
  Added chunked copy and row-wise 24-bit BMP examples. The initial SD write
  error did not recur in a bounded create/rewrite probe and remains unexplained.
  See [bounded binary assets](docs/docs/development/binary-file-assets.md).

- Experimental optional `/sd` on the RP2350 LCD 1.47 A: separate lazy FAT mount,
  bounded SPI block I/O, no SD auto-format or USB export, unchanged `/app`
  startup, mount-boundary checks and explicit media errors. A bounded JSON pixel
  asset demo loaded two distinct pictures from the card: exact hashes after
  software resets and physical A/B rendering were verified by webcam. The first
  write returned an I/O error; retry succeeded, but its cause remains unresolved.
  SD bus/pins stay reserved outside app peripheral routes. Other SD slots,
  exFAT, SDSC, binary/streaming APIs and hot-swap remain unsupported/unqualified.
  See [removable SD assets](docs/docs/development/sd-assets.md).

- **Breaking:** internal application volumes use logical `/app` on RP2 and ESP32.
  Boot runs `/app/index.js`; relative filesystem paths start at `/app`, and file
  entry points now run as CommonJS modules with module-relative imports. No
  legacy absolute-root aliases, automatic SD startup, or automatic file migration.
- Fixed RP2350 BOOTSEL sampling to use its SDK-defined QSPI input bit rather
  than the RP2040 input bit, which falsely selected safe mode during autorun QA.
  Real held-button recovery remains required for hardware acceptance.
- Added a bounded saved-settings drawing startup example and namespace/backend
  regression tests. On the 1.69, candidate `c746dd7` passed software-reset autorun
  and settings persistence; the operator confirmed charger-powered cold-start
  and visible aqua drawing. Earlier USB-connected touch-read errors remain
  unresolved, and physical held-BOOTSEL recovery is not yet verified.
  See [application namespace and upgrade procedure](docs/docs/development/app-namespace.md).

Development work toward the portable API release. **Not a published release or
qualified release candidate.** `version.txt` still reads `0.1.0`; check the
installed firmware build ID and `board.apiVersion` before using these APIs.

Bounded hardware acceptance: Sticky PSRAM/display/restart and vanilla Pico
non-PSRAM scheduling/events/button checks passed on candidate `d7cfa41`.
The integration includes the required eight-byte PSRAM arena alignment fix.
See [hardware acceptance](docs/docs/development/runtime-foundation-acceptance.md)
for exact evidence and exclusions. Other boards' builds are not physical
qualification; PiZero flash-readback investigation is tracked separately in #18.
RP2 upgrades can relocate the filesystem when firmware size changes: verified
backups and an approved restoration plan are required before such updates.

### Added

- Experimental Canvas pointer input for the configured onboard RP2350 Touch LCD
  1.69: one-contact down/move/up/cancel events, explicit start/stop, Canvas-local
  coordinates, exclusive I2C ownership and lifecycle cancellation. Other display
  profiles report zero touch points. Includes a shared drawing consumer and an
  explicit browser mouse adapter. Native/host/browser checks pass; physical
  portrait drawing and basic finger alignment were confirmed on `f349bb4`.
  Includes STOP-separated CST816 reads fixing report-read failures. See
  `docs/docs/development/canvas-pointer.md` for subset and ownership limits.

- Sticky AI/Power-button restart: hold for three seconds after releasing the
  button following boot. Short presses cancel; a held-through-boot button
  cannot loop restarts. Main-loop-driven, restart-only; safe boot is unchanged.

- Configured, read-only button handles for Pico BOOTSEL and XIAO BOOT through
  `devices.button`: debounced press/release events, readable state, exclusive
  polling ownership, cancellation cleanup, error recovery and VM-safe timers.
  Unsupported/reset/power controls remain absent; touch is not included.
- `EventTarget.clear(target)` lifecycle extension removes current registrations
  and signal links; it does not close devices or abort signals.

- First configured-display device slice: lazy `require('devices')` discovery
  backed by compiled capabilities, absent unsupported displays, stable frozen
  descriptors, exclusive native ownership, live state and repeatable cleanup.
  Owned Canvas handles gain explicit `present()` (show before close), native
  failure state, and stable EBUSY/EIO/ENXIO errors. Profile-specific build
  manifests report the same support as runtime discovery. No new peripherals,
  device queue, generic pin/SD arbiter or compatibility aliases. Physical
  acceptance is limited to the checkpoint above; see
  `docs/docs/development/device-display-handles.md`.

- Lazy `require('events')` with bounded, synchronous `Event`/`EventTarget` and
  `AbortController`/`AbortSignal` subsets. Signal cancellation removes attached
  listeners before abort notification; dispatch is safe under mutation and
  reentrancy. Native 64KiB Jerry tests check limits, repeated-workload heap use,
  Promise rejection, file-cache clearing and fresh-VM reuse. Engine teardown
  now releases built-in and file-module cache handles. This is not Node's
  EventEmitter, a DOM implementation, a device event queue or hardware I/O
  cancellation. See `docs/docs/development/bounded-events.md`; bounded Pico
  hardware acceptance passed as recorded above.

- Cooperative Promise-job processing on RP and ESP: up to 16 jobs before and
  after each timer pass, preserving queued work while yielding between batches.
  Adds a pinned, build-local JerryScript bounded-job extension without changing
  upstream `jerry_run_jobs()` or the SDK cache. Native tests cover completion,
  rejection handlers, async/await, ordering, sustained chains and lifecycle.
  Fixes RP callback timer-slot reuse and releases timer refs before engine
  teardown on both backends. This is not browser/Node microtask ordering,
  callback preemption, queue backpressure or unhandled-rejection reporting.
  See `docs/docs/development/promise-jobs.md`; bounded Pico hardware acceptance
  passed as recorded above.

- Experimental Seeed reTerminal Sticky target: 800×480 Canvas in 8MB PSRAM,
  SSD1677 full monochrome refresh and native UART console through the onboard
  USB bridge. No TinyUSB/MSC on microphone pins; no touch/audio/sensor APIs.
  Board-gated renderer limits leave smaller-board memory profiles unchanged.
  The 768,000-byte RGB565 framebuffer requires PSRAM, with no internal-SRAM
  fallback; full updates use bounded BUSY waits and power down the panel.
- *The Night Ferry*, an original procedural Canvas ink scene with an optional
  one-shot startup script. Two settled hardware restarts verified one successful
  presentation each, with stable idle operation; an operator-supplied video
  subsequently confirmed the physical artwork's orientation and legibility.
  Sticky remains outside release packaging; battery life, cold battery startup
  and long-term ghosting are not qualified. See `platform/esp32/STICKY.md` for
  recovery, build and qualification boundaries.

- Opt-in, experimental `display.canvas` API with a shared Canvas 2D subset
  across RP2040/RP2350 and ESP32-S3. Applications use `getContext('2d')`;
  display adapters own the wiring, transport and presentation. The same
  ISC-licensed ctx 0.1.18 rasterizer serves HDMI, RGB565 LCD and monochrome
  e-paper backends. Not enabled in standard firmware or a complete Canvas/DOM
  implementation. See `docs/docs/development/display-canvas.md` and
  `docs/docs/development/canvas-first-slice.md` for the supported subset.
- ST7789-family Canvas profiles for Waveshare RP2350-LCD-1.47-A,
  RP2350-Touch-LCD-2.8 and RP2350-Touch-LCD-1.69, alongside PiZero HDMI.
  The 2.8 board gains an initial runtime/USB/filesystem port; its touch, audio,
  SD and sensors remain unsupported. The 1.69 Canvas slice likewise does not
  add touch, audio, SD or IMU support.
- Initial Waveshare ESP32-S3-ePaper-1.54 **V2** port: native USB CDC/MSC,
  persistent script storage and a 200×200 Canvas surface. Its private backend
  converts RGB565 drawing to monochrome, performs a full update with bounded
  BUSY waits, then sleeps and powers down the panel. V1 is not interchangeable.
  Public GPIO/peripheral APIs, audio, RTC, sensors and SD are not qualified or
  exposed by this conservative image. The board remains outside shipping
  release packaging and uses ROM/esptool recovery, not a XIAO TinyUF2 image.
- Original procedural Canvas examples with source, software previews and run
  instructions: *Star Fisher*, *Somebody's Still Awake*, *The Moon Mender* and
  *The Wandering Library*. The newer compositions use uniform scaling;
  the original e-paper example draws in one task without animation or autorun.
- Opt-in V2 e-paper partial-refresh waveform using the vendor LUT, with a
  monochrome previous-frame cache restored after panel power-down. The first
  update is full; four partial updates are followed by a full cleaning refresh.
  Canvas drawing calls are unchanged. Transfers still cover the full image;
  this is not dirty-rectangle SPI or a production-qualified refresh policy.
- Optional *Wandering Library* startup script with irregular snail blinks,
  occasional longer pauses and cleaning after every two blinks. Copying its
  `index.js` explicitly enables autorun; other demos are unchanged. See
  `examples/waveshare-epaper-1.54-v2/AUTORUN.md`.
- Private V2 battery-power latch initialization and long-press PWR shutdown,
  guarded against treating the startup press as a shutdown request. The unused
  audio rail stays disabled; no new public GPIO or power API is introduced.
  Battery-only startup/blinking was operator-confirmed. A short partial-refresh
  trial reported no visible ghosting; battery life, long-term panel endurance
  and the least-frequent suitable cleaning interval remain unmeasured.

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

- ESP32-S3 JavaScript heaps are declared in each target's
  `board/<board-id>/board_config.cmake`: 256KiB in PSRAM for XIAO ESP32-S3,
  Waveshare ePaper 1.54 V2 and reTerminal Sticky. JerryScript's external context
  keeps the managed heap out of internal SRAM without enabling 32-bit compressed
  pointers. Missing PSRAM fails explicitly; there is no internal fallback.
  Host tests exercise the real engine/ESP port with a simulated PSRAM allocator,
  including a live object graph that exhausts the previous 128KiB heap, GC,
  cleanup/reinitialization and allocation failure. This configuration still
  needs on-device PSRAM, storage and responsiveness qualification on each board.
  RP2040/RP2350 heap settings and all CPU budgets are unchanged.
- Sticky retains its explicit 240MHz CPU setting. Its earlier on-device artwork
  acceptance used a 128KiB internal JS heap, not the new PSRAM-backed heap.
- Canvas presentation is batched after a JavaScript drawing task, rather than
  after individual drawing calls. Applications need no hardware-specific
  `show()` or `present()` call. Each display owns its surface and drawing state;
  closing a display invalidates its drawing context.
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

### Fixed

- Request explicit 8-byte alignment for the ESP32-S3 PSRAM JavaScript arena.
  Ordinary ESP-IDF allocation may be only 4-byte aligned, corrupting Jerry's
  compressed pointers during startup. Native allocator tests now exercise
  that weaker alignment guarantee, named parsing and exception handling.
- Isolated PiZero Canvas stack storage from Core 1 and TMDS memory, kept the
  Core 1 stack independently sized, and distinguished requested USB resets
  from Canvas watchdog recovery.
- Omitted disabled ADC and image-decoder implementations from the initial
  LCD 2.8 image instead of compiling unsupported facilities.
- Included ESP-IDF's PSRAM component for e-paper and added a compile-time guard
  against missing octal PSRAM configuration. Unknown Kconfig settings must not
  silently leave a bootable image without its required display memory.
- Kept Canvas presentation/failure counters available when a JerryScript build
  lacks optional heap telemetry; unavailable heap measurements are omitted.

### Verification and remaining limitations

- Physical Canvas output has been checked on PiZero HDMI, LCD 1.47, Touch LCD
  2.8, Touch LCD 1.69 (portrait), and ESP32-S3 e-paper 1.54 V2. The unchanged
  *Somebody's Still Awake* JavaScript ran on both the 2.8 LCD and PiZero HDMI.
- E-paper firmware `0.1.0+3a53622` ran *The Wandering Library* with one completed
  full presentation and zero reported presentation failures; a physical photo
  confirmed orientation and the complete composition. The factory flash was
  backed up twice with matching hashes, and application updates preserved the
  bootloader, partition table, NVS and calibration area. USB reset can leave
  this board in ROM download mode: release BOOT and perform a normal PWR cycle.
- Focused regression tests and native checks cover Canvas behavior, registry
  honesty, SPI transfers, monochrome bit order, BUSY timeout, failure cleanup,
  sleep and release. These are not full-board electrical qualification or
  evidence of long-term e-paper endurance. The 1.69 landscape profile remains
  physically unqualified; RGB565/monochrome output is not browser-identical.

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
- Full Canvas/DOM conformance, unimplemented drawing features and a broader
  resource-handle redesign remain outside this experimental display slice.

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
