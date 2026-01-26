# mcujs Roadmap

## Critical: Runaway Script Protection

**Problem:** An infinite loop in `index.js` blocks the entire system, making the filesystem inaccessible. Users cannot fix their code without reflashing the device.

### Solution: Boot-Safe Recovery (Recommended)

We already have a safe boot path: hold the button during boot to skip auto-loading `index.js`. That keeps USB mass storage accessible so users can delete or fix broken scripts without reflashing.

**Benefits:**
- Simple recovery path with no multicore complexity
- Filesystem remains accessible for repair
- No device reflash required to recover

**Implementation:**
- Document the button-hold boot behavior in user-facing docs
- Surface a clear boot message when safe mode is active

### Solution: Watchdog Timer (Fallback)

Enable hardware watchdog that must be fed from the main loop:

```c
watchdog_enable(8000, true);  // 8 second timeout

// In main_loop()
while (1) {
    watchdog_update();  // Feed the watchdog
    tud_task();
    // ...
}
```

**Benefits:**
- Device auto-resets if JS blocks for too long
- Combined with BOOTSEL safe mode, provides recovery path

**Drawbacks:**
- Legitimate long-running JS code could trigger reset
- Need to expose `watchdog.feed()` to JS for long operations

---

## Short-Term (Next Up)

### USB Mass Storage
- [x] Test Windows/Mac compatibility

### Filesystem from JavaScript
- [x] Implement `fs.readFileSync()` to read files from JavaScript
- [x] Implement `fs.writeFileSync()` to write files from JavaScript  
- [x] Implement `fs.readdirSync()` to list directory contents from JavaScript

### REPL Commands
- [x] Implement `.rm FILE` to delete files
- [x] Implement `.uf2` / `.uf2!` to enter UF2 mode
- [x] Implement `.usbreset` to reset USB connection

### Testing
- [x] Bun end-to-end harness (build/flash/REPL/filesystem)

---

## Near-Term Improvements

### REPL Enhancements
- [x] Tab completion for global objects and properties
- [x] Include build ID in `.info` output
- [x] Multi-line paste support

### Error Handling
- [x] Stack traces with line numbers
- [x] "Did you mean?" suggestions for typos (variables, modules)
- [ ] Source map support for debugging
- [ ] Catch and report uncaught promise rejections

### Filesystem
- [x] Subdirectory support
- [x] Long filename support (LFN) via FatFs
- [ ] File timestamps
- [ ] Larger file support (currently 32KB limit for JS files)

### Performance
- [ ] JerryScript snapshot support (pre-compile JS to bytecode)
- [ ] Lazy module loading
- [ ] Memory pool tuning for RP2040 (264KB) vs RP2350 (520KB)

### Build System
- [x] Shrink Docker image (was ~3GB, now ~1.3GB)
  - Switched from Ubuntu to Alpine Linux base
  - Clean apk caches in same layer as install

---

## Future Features

### Additional Hardware APIs
- [x] `ADC` - Analog-to-digital converter
- [ ] `ADC` configurable Vref / calibration
- [ ] `UART` - Serial communication
- [ ] `PIO` - Programmable I/O (RP2040/RP2350 unique feature)
- [x] `SPI.writeBufferDMA()` - DMA transfers for high-speed SPI (display updates)
- [ ] `DMA` - General-purpose direct memory access
- [ ] `RTC` - Real-time clock (external module support)
- [ ] `WS2812` - NeoPixel LED support (via PIO)
- [x] `graphics` module - RGB565 framebuffer with fill, rect, pixel, line, circle primitives

### Board Support
- [x] Waveshare RP2040-Zero (NeoPixel on GPIO 16)
- [x] Waveshare RP2040 Touch LCD 1.28" (GC9A01A display, CST816S touch, QMI8658 IMU)
- [x] Raspberry Pi Pico 2 W (CYW43 WiFi/BLE chip, LED on CYW43)
- [x] Waveshare RP2350-LCD-1.47-A (ST7789V3 display 320x172, 16MB flash, NeoPixel)
- [ ] Raspberry Pi Pico W (CYW43 WiFi/BLE chip)
- [ ] Additional Waveshare RP2350 boards
- [ ] Adafruit Feather RP2040/RP2350
- [ ] Pimoroni boards
- [ ] Generic RP2040/RP2350 board config

### Module System
- [x] `require()` for CommonJS modules
- [x] Built-in module path resolution (`./`, `../`, `/lib/`)
- [x] JSON import support

### ES Modules (Low Priority)
- [ ] ES module `import`/`export` (partial, no async)
  - Would require significant parser changes
  - CommonJS covers most embedded use cases

### Host Tooling (`host/` directory)
- [ ] `mcujs-cli` - Command-line tool for deployment
  - `mcujs deploy index.js` - Copy file to connected device
  - `mcujs repl` - Connect to serial REPL
  - `mcujs monitor` - Watch console output
  - `mcujs reset` - Reset device
- [ ] VS Code extension
  - Syntax highlighting for mcujs APIs
  - Deploy on save
  - Integrated serial monitor

### Debugging
- [ ] `debugger` statement support
- [ ] Breakpoints via REPL (`.break file.js:10`)
- [ ] Variable inspection
- [ ] Step execution

### HID (Human Interface Device)

USB HID support for keyboard and mouse emulation. The device appears as a standard keyboard/mouse to the host computer, enabling automation, macro keyboards, custom input devices, and accessibility tools.

**Implementation Notes:**
- Uses TinyUSB HID class (already bundled with Pico SDK)
- Composite device: CDC (REPL) + MSC (filesystem) + HID (keyboard/mouse)
- Standard HID report descriptors for maximum compatibility
- No drivers required on host (works with Windows, Mac, Linux)

#### Keyboard API

```javascript
const Keyboard = require('keyboard');

// Type a string (handles shift automatically)
Keyboard.print('Hello, World!');

// Press and release a single key
Keyboard.press('a');
Keyboard.release('a');

// Shortcut: press and release
Keyboard.tap('a');

// Modifier keys
Keyboard.press('ctrl');
Keyboard.tap('c');          // Ctrl+C
Keyboard.release('ctrl');

// Convenience method for key combinations
Keyboard.combo('ctrl', 'alt', 'delete');
Keyboard.combo('cmd', 'shift', '4');    // Mac screenshot

// Special keys
Keyboard.tap('enter');
Keyboard.tap('tab');
Keyboard.tap('escape');
Keyboard.tap('backspace');
Keyboard.tap('delete');
Keyboard.tap('up');         // Arrow keys
Keyboard.tap('down');
Keyboard.tap('left');
Keyboard.tap('right');
Keyboard.tap('home');
Keyboard.tap('end');
Keyboard.tap('pageup');
Keyboard.tap('pagedown');
Keyboard.tap('f1');         // Function keys F1-F12
Keyboard.tap('capslock');
Keyboard.tap('printscreen');

// Media keys (if supported by HID descriptor)
Keyboard.tap('mute');
Keyboard.tap('volumeup');
Keyboard.tap('volumedown');
Keyboard.tap('playpause');
Keyboard.tap('nexttrack');
Keyboard.tap('prevtrack');

// Release all keys (safety)
Keyboard.releaseAll();

// Check if a key is currently pressed
Keyboard.isPressed('shift');  // Returns boolean
```

#### Mouse API

```javascript
const Mouse = require('mouse');

// Move mouse relative to current position
Mouse.move(10, -5);         // x, y delta (pixels)

// Click buttons
Mouse.click('left');        // 'left', 'right', 'middle'
Mouse.click('right');

// Press and release separately (for drag operations)
Mouse.press('left');
Mouse.move(100, 0);         // Drag 100px right
Mouse.release('left');

// Double-click
Mouse.doubleClick('left');

// Scroll wheel
Mouse.scroll(3);            // Scroll up 3 units
Mouse.scroll(-3);           // Scroll down 3 units
Mouse.scrollHorizontal(2);  // Horizontal scroll (if supported)

// Release all buttons (safety)
Mouse.releaseAll();

// Check button state
Mouse.isPressed('left');    // Returns boolean
```

#### Combined Example: Macro Keyboard

```javascript
const Keyboard = require('keyboard');
const gpio = require('gpio');

// Button on GPIO 15
gpio.init(15, gpio.INPUT_PULLUP);

// Poll for button press
setInterval(() => {
    if (gpio.get(15) === 0) {
        // Button pressed - type a code snippet
        Keyboard.print('console.log("Hello!");');
        Keyboard.tap('enter');
        
        // Debounce
        while (gpio.get(15) === 0) {}
    }
}, 10);
```

#### Combined Example: Mouse Jiggler

```javascript
const Mouse = require('mouse');

// Move mouse slightly every 30 seconds to prevent screen lock
setInterval(() => {
    Mouse.move(1, 0);
    Mouse.move(-1, 0);
}, 30000);
```

#### Implementation Tasks

- [x] Add HID device class to TinyUSB descriptor (composite CDC+MSC+HID)
- [x] Implement keyboard HID report generation
- [x] Implement mouse HID report generation  
- [x] Create `keyboard` native module
- [x] Create `mouse` native module
- [x] Handle USB enumeration with HID
- [ ] Test on Windows, Mac, Linux hosts
- [x] Document keyboard layout considerations (US QWERTY default)

### Networking (Pico W / Pico 2 W)
- [x] Pico 2 W board support (LED via CYW43)
- [ ] Pico W board support (RP2040 + CYW43)
- [ ] WiFi
  - [ ] `wifi.connect(ssid, password)` - Connect to access point
  - [ ] `wifi.disconnect()` - Disconnect from network
  - [ ] `wifi.status()` - Connection status and IP info
  - [ ] `wifi.scan()` - Scan for available networks
- [ ] HTTP client
  - [ ] `http.get(url, callback)` - Simple GET request
  - [ ] `http.request(options, callback)` - Full HTTP client
- [ ] WebSocket client
  - [ ] `WebSocket` class for real-time communication
- [ ] MQTT client
  - [ ] `mqtt.connect(broker)` - Connect to MQTT broker
  - [ ] `mqtt.subscribe(topic, callback)` - Subscribe to topics
  - [ ] `mqtt.publish(topic, message)` - Publish messages
- [ ] TCP/UDP sockets (lower-level networking)
- [ ] Bluetooth (CYW43439 supports BLE)
  - [ ] BLE peripheral mode
  - [ ] BLE central mode (scanning/connecting)

---

## Known Limitations

### Memory Constraints
- RP2040: 264KB SRAM total, ~100KB available for JS heap
- RP2350: 520KB SRAM total, ~200KB available for JS heap
- Large arrays/strings can exhaust memory quickly

### JavaScript Limitations
- No `async`/`await` (JerryScript limitation)
- No `Proxy` or `Reflect`
- Limited `RegExp` support
- No `BigInt`
- 32-bit integers only (no 64-bit)

### Filesystem Limitations
- ~1MB usable on Pico, ~3.5MB on Pico 2

### USB Limitations
- Cannot use CDC and MSC simultaneously with some hosts
- Windows may cache filesystem aggressively
- Safe eject recommended before reset

---

## Dependencies

Current versions and upgrade status:

| Dependency | Current | Latest | Status |
|------------|---------|--------|--------|
| JerryScript | 3.0.0 | 3.0.0 | Up to date |
| Pico SDK | 2.2.0 | 2.2.0 | Up to date |
| TinyUSB | (bundled with Pico SDK) | - | Via Pico SDK |
| FatFs | 0.16 | 0.16 | Up to date |

### Recently Updated

- [x] FatFs 0.15 → 0.16
  - Fixed `f_readdir` infinite loop bug
  - Fixed dot names with terminating separator issues
  - `f_getcwd` and `..` now work on exFAT volumes
- [x] Pico SDK 2.0.0 → 2.2.0
  - New board support
  - Encrypted binary support  
  - Bug fixes for watchdog, DMA, flash
  - GCC 15 support
- [x] FatFs: Replaced custom FAT12 implementation with FatFs R0.15
  - Long filename (LFN) support
  - UTF-8 encoding
  - Better Windows/Mac compatibility
