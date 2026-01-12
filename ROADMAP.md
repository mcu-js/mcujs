# mcujs Roadmap

## Critical: Runaway Script Protection

**Problem:** An infinite loop in `index.js` blocks the entire system, making the filesystem inaccessible. Users cannot fix their code without reflashing the device.

### Solution: Dual-Core Execution (Recommended)

The RP2040 and RP2350 both have two ARM cores. We can isolate JavaScript execution from the USB stack:

| Core | Responsibilities |
|------|------------------|
| Core 0 | USB stack (TinyUSB), filesystem (FAT12), REPL input |
| Core 1 | JavaScript engine (JerryScript), timer callbacks |

**Benefits:**
- Filesystem remains accessible even if JS hangs
- User can delete/edit `index.js` via USB mass storage
- REPL can send abort signal to Core 1
- No device reflash required to recover

**Implementation:**
- Use `pico_multicore` library
- Core 0 runs `main_loop()` with `tud_task()` and `repl_task()`
- Core 1 runs JS engine in separate loop
- Inter-core FIFO for communication (abort signals, REPL eval requests)
- Shared memory for console output buffer

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
- [ ] Stack traces with line numbers
- [ ] Source map support for debugging
- [ ] Better error messages for common mistakes
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
- [ ] `DMA` - Direct memory access for high-speed transfers
- [ ] `RTC` - Real-time clock (external module support)
- [ ] `WS2812` - NeoPixel LED support (via PIO)

### Board Support
- [ ] Waveshare RP2040/RP2350 boards
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

### Networking (Future Hardware)
- [ ] Pico W support (WiFi)
- [ ] HTTP client
- [ ] WebSocket client
- [ ] MQTT client

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
