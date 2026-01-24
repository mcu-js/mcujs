# Waveshare RP2040 Touch LCD 1.28" Screen Development Plan

## Overview

This document tracks the implementation of a native graphics/screen buffer system for the Waveshare RP2040 Touch LCD 1.28" display in mcujs.

## Display Hardware Specs

| Spec | Value |
|------|-------|
| Display | GC9A01A driver, 1.28" round IPS LCD |
| Resolution | 240 x 240 pixels |
| Color Depth | 16-bit RGB565 (65K colors) |
| Interface | SPI1 |
| Full Framebuffer | 240 x 240 x 2 = 115,200 bytes (~112.5 KB) |

### Pin Configuration (from Waveshare official DEV_Config.h)

| Function | GPIO |
|----------|------|
| LCD SPI1 SCK | 10 |
| LCD SPI1 MOSI | 11 |
| LCD SPI1 MISO | 12 |
| LCD CS | 9 |
| LCD DC | 8 |
| LCD RST | 13 |
| LCD BL | 25 |

**Important:** CS must stay LOW during all SPI communication (Waveshare style). Set CS LOW after hardware reset and keep it low for all commands and data.

## Architecture

```
+-------------------------------------------------------------+
|                     JavaScript Layer                         |
+-------------------------------------------------------------+
|  const screen = createScreen({                              |
|    width: 240, height: 240,                                 |
|    driver: gc9a01aDriver  // JS driver module               |
|  });                                                        |
|  screen.fillRect(10, 10, 50, 50, screen.color565(255,0,0)); |
|  screen.flush();  // sends to display via DMA               |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|              Native Module: graphics (C)                     |
+-------------------------------------------------------------+
|  graphics.createBuffer({ width, height })                    |
|  graphics.setPixel(buffer, x, y, color)                     |
|  graphics.fillRect(buffer, x, y, w, h, color)               |
|  graphics.fill(buffer, color)                               |
|  graphics.color565(r, g, b)                                 |
|  graphics.freeBuffer(buffer)                                |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|           Enhanced SPI Module with DMA (C)                   |
+-------------------------------------------------------------+
|  SPI.writeBufferDMA(bus, bufferHandle, length)              |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                   Hardware (GC9A01A)                         |
+-------------------------------------------------------------+
```

## Design Decisions

1. **Single buffer**: One framebuffer at a time (~112KB) to minimize complexity and memory risk
2. **Blocking DMA**: `flush()` waits for DMA completion to avoid race conditions
3. **Pure JS driver**: GC9A01A init/commands in JavaScript for flexibility and easy customization
4. **Composition over `new`**: Factory functions, no class constructors

## Implementation Phases

### Phase 1: Native Graphics Buffer (`host/bindings/graphics.c`)

**Status:** [x] Complete

**API:**
```js
// Create a framebuffer (memory allocated in C, not JS heap)
const buf = graphics.createBuffer({ width: 240, height: 240 });
// Returns a handle (integer) to reference the buffer

// Fill entire buffer with color
graphics.fill(buf, color565);

// Color conversion
const red = graphics.color565(255, 0, 0);  // Returns 0xF800

// Memory management  
graphics.freeBuffer(buf);

// Get buffer info
const info = graphics.getBufferInfo(buf);
// { width: 240, height: 240, byteLength: 115200 }
```

**Files:**
- [ ] `host/bindings/graphics.c` - New
- [ ] `host/bindings/graphics.h` - New
- [ ] `host/bindings/CMakeLists.txt` - Add graphics.c
- [ ] `host/bindings/bindings.h` - Add declarations
- [ ] `host/engine.c` - Register module

**Test (Phase 1a):**
```js
// test-graphics.js - Phase 1 test
const buf = graphics.createBuffer({ width: 240, height: 240 });
console.log('Buffer created:', graphics.getBufferInfo(buf));
console.log('Free memory:', board.freeMemory());

graphics.fill(buf, graphics.color565(0, 0, 255));  // Blue
console.log('Filled with blue');

graphics.freeBuffer(buf);
console.log('Buffer freed');
console.log('Free memory after:', board.freeMemory());
```

---

### Phase 2: Enhanced SPI with DMA

**Status:** [x] Complete

**API:**
```js
// Write buffer via DMA (blocking, waits for completion)
SPI.writeBufferDMA(bus, bufferHandle, byteLength);
```

**Files:**
- [ ] `host/bindings/spi.c` - Add DMA support

**Test (Phase 2a):**
```js
// test-spi-dma.js - Phase 2 test
const buf = graphics.createBuffer({ width: 10, height: 10 });
graphics.fill(buf, 0xF800);  // Red

SPI.init(1, 10, 11, 2, 10000000);  // 10 MHz
GPIO.init(9, GPIO.OUT);  // CS

GPIO.set(9, 0);  // CS low
SPI.writeBufferDMA(1, buf, 200);  // 10x10x2 bytes
GPIO.set(9, 1);  // CS high

console.log('DMA transfer complete');
graphics.freeBuffer(buf);
```

---

### Phase 3: GC9A01A JS Driver

**Status:** [x] Complete

**File:** `examples/waveshare-lcd-1.28/gc9a01a.js`

```js
// GC9A01A display driver for mcujs
// Pure JavaScript implementation for flexibility

function createGC9A01ADriver(options) {
  const { 
    spiBus = 1,
    cs = 9, 
    dc = 8, 
    rst = 12, 
    bl = 25,
    baudrate = 62500000  // 62.5 MHz
  } = options || {};
  
  // Initialize GPIO pins
  function initPins() {
    GPIO.init(cs, GPIO.OUT);
    GPIO.init(dc, GPIO.OUT);
    GPIO.init(rst, GPIO.OUT);
    GPIO.init(bl, GPIO.OUT);
    GPIO.set(cs, 1);  // CS high (inactive)
    GPIO.set(bl, 0);  // Backlight off initially
  }
  
  // Send command byte
  function command(cmd) {
    GPIO.set(dc, 0);  // Command mode
    GPIO.set(cs, 0);
    SPI.transfer(spiBus, cmd);
    GPIO.set(cs, 1);
  }
  
  // Send data bytes
  function data(bytes) {
    GPIO.set(dc, 1);  // Data mode
    GPIO.set(cs, 0);
    if (Array.isArray(bytes)) {
      SPI.transfer(spiBus, bytes);
    } else {
      SPI.transfer(spiBus, bytes);
    }
    GPIO.set(cs, 1);
  }
  
  // Hardware reset
  function reset() {
    GPIO.set(rst, 1);
    // delay handled by sleep_ms in native
    GPIO.set(rst, 0);
    GPIO.set(rst, 1);
  }
  
  // GC9A01A initialization sequence
  function init() {
    initPins();
    SPI.init(spiBus, 10, 11, 2, baudrate);
    
    reset();
    
    // Init sequence based on Waveshare/manufacturer specs
    command(0xEF);
    command(0xEB); data([0x14]);
    command(0xFE);
    command(0xEF);
    command(0xEB); data([0x14]);
    command(0x84); data([0x40]);
    command(0x85); data([0xFF]);
    command(0x86); data([0xFF]);
    command(0x87); data([0xFF]);
    command(0x88); data([0x0A]);
    command(0x89); data([0x21]);
    command(0x8A); data([0x00]);
    command(0x8B); data([0x80]);
    command(0x8C); data([0x01]);
    command(0x8D); data([0x01]);
    command(0x8E); data([0xFF]);
    command(0x8F); data([0xFF]);
    command(0xB6); data([0x00, 0x00]);
    command(0x36); data([0x48]);  // Memory access control
    command(0x3A); data([0x05]);  // 16-bit color (RGB565)
    command(0x90); data([0x08, 0x08, 0x08, 0x08]);
    command(0xBD); data([0x06]);
    command(0xBC); data([0x00]);
    command(0xFF); data([0x60, 0x01, 0x04]);
    command(0xC3); data([0x13]);
    command(0xC4); data([0x13]);
    command(0xC9); data([0x22]);
    command(0xBE); data([0x11]);
    command(0xE1); data([0x10, 0x0E]);
    command(0xDF); data([0x21, 0x0C, 0x02]);
    command(0xF0); data([0x45, 0x09, 0x08, 0x08, 0x26, 0x2A]);
    command(0xF1); data([0x43, 0x70, 0x72, 0x36, 0x37, 0x6F]);
    command(0xF2); data([0x45, 0x09, 0x08, 0x08, 0x26, 0x2A]);
    command(0xF3); data([0x43, 0x70, 0x72, 0x36, 0x37, 0x6F]);
    command(0xED); data([0x1B, 0x0B]);
    command(0xAE); data([0x77]);
    command(0xCD); data([0x63]);
    command(0x70); data([0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03]);
    command(0xE8); data([0x34]);
    command(0x62); data([0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70]);
    command(0x63); data([0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70]);
    command(0x64); data([0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07]);
    command(0x66); data([0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00]);
    command(0x67); data([0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98]);
    command(0x74); data([0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00]);
    command(0x98); data([0x3E, 0x07]);
    command(0x35);  // Tearing effect on
    command(0x21);  // Inversion on
    command(0x11);  // Sleep out
    // Wait 120ms for sleep out
    command(0x29);  // Display on
    // Wait 20ms
    
    GPIO.set(bl, 1);  // Backlight on
  }
  
  // Set drawing window
  function setWindow(x0, y0, x1, y1) {
    command(0x2A);  // Column address set
    data([0x00, x0, 0x00, x1]);
    command(0x2B);  // Row address set
    data([0x00, y0, 0x00, y1]);
    command(0x2C);  // Memory write
  }
  
  // Flush buffer to display via DMA
  function flush(bufferHandle, width, height) {
    setWindow(0, 0, width - 1, height - 1);
    GPIO.set(dc, 1);  // Data mode
    GPIO.set(cs, 0);
    SPI.writeBufferDMA(spiBus, bufferHandle, width * height * 2);
    GPIO.set(cs, 1);
  }
  
  return {
    init,
    command,
    data,
    setWindow,
    flush,
    reset
  };
}

module.exports = { createGC9A01ADriver };
```

**Test (Phase 3a):**
```js
// test-gc9a01a-init.js - Phase 3 test
const { createGC9A01ADriver } = require('./gc9a01a');
const driver = createGC9A01ADriver();
driver.init();
console.log('GC9A01A initialized - backlight should be on!');
```

---

### Phase 4: Screen Factory

**Status:** [x] Complete

**File:** `examples/waveshare-lcd-1.28/screen.js`

```js
// Screen factory - composes buffer + driver
function createScreen(options) {
  const { width = 240, height = 240, driver } = options;
  const buffer = graphics.createBuffer({ width, height });
  
  return {
    width,
    height,
    
    setPixel(x, y, color) {
      graphics.setPixel(buffer, x, y, color);
    },
    
    fillRect(x, y, w, h, color) {
      graphics.fillRect(buffer, x, y, w, h, color);
    },
    
    fill(color) {
      graphics.fill(buffer, color);
    },
    
    color565: graphics.color565,
    
    flush() {
      driver.flush(buffer, width, height);
    },
    
    destroy() {
      graphics.freeBuffer(buffer);
    }
  };
}

module.exports = { createScreen };
```

**Test (Phase 4a):**
```js
// test-screen.js - Phase 4 test
const { createGC9A01ADriver } = require('./gc9a01a');
const { createScreen } = require('./screen');

const driver = createGC9A01ADriver();
driver.init();

const screen = createScreen({ width: 240, height: 240, driver });
screen.fill(screen.color565(0, 0, 255));  // Blue
screen.flush();

console.log('Screen should be blue!');
```

---

### Phase 5: Drawing Primitives

**Status:** [ ] In Progress - Basic primitives done, adding circles/lines/text

**Add to graphics.c:**
```js
graphics.setPixel(buf, x, y, color);
graphics.fillRect(buf, x, y, w, h, color);
// Future: drawLine, drawCircle, fillCircle
```

**Test (Phase 5a):**
```js
// test-drawing.js - Phase 5 test
const { createGC9A01ADriver } = require('./gc9a01a');
const { createScreen } = require('./screen');

const driver = createGC9A01ADriver();
driver.init();

const screen = createScreen({ width: 240, height: 240, driver });

// Clear to dark blue
screen.fill(screen.color565(0, 0, 64));

// Red square
screen.fillRect(20, 20, 80, 80, screen.color565(255, 0, 0));

// Green square
screen.fillRect(100, 100, 80, 80, screen.color565(0, 255, 0));

// White square
screen.fillRect(60, 60, 80, 80, screen.color565(255, 255, 255));

screen.flush();
console.log('Should see colored rectangles!');
```

---

## Progress Checklist

- [x] Phase 1: Native Graphics Buffer
  - [x] graphics.c created
  - [x] createBuffer implemented
  - [x] fill implemented
  - [x] color565 implemented (with byte-swap for big-endian SPI)
  - [x] freeBuffer implemented
  - [x] getBufferInfo implemented
  - [x] Module registered in engine.c
  - [x] Build succeeds
  - [x] Test passes on device

- [x] Phase 1a: Memory test verified

- [x] Phase 2: Enhanced SPI with DMA
  - [x] writeBufferDMA implemented
  - [x] DMA channel allocation
  - [x] Blocking wait for completion
  - [x] Build succeeds
  - [x] Test passes on device

- [x] Phase 2a: DMA transfer verified

- [x] Phase 3: GC9A01A JS Driver
  - [x] gc9a01a.js created
  - [x] Init sequence implemented (full Waveshare sequence)
  - [x] setWindow implemented
  - [x] flush implemented
  - [x] Backlight control works
  - [x] CS stays LOW during communication (Waveshare style)

- [x] Phase 3a: Display initializes

- [x] Phase 4: Screen Factory
  - [x] screen.js created
  - [x] Composition pattern works
  - [x] fill() works
  - [x] flush() works

- [x] Phase 4a: Solid color displays

- [x] Phase 5: Basic Drawing Primitives
  - [x] setPixel implemented
  - [x] fillRect implemented

- [x] Phase 5a: Shapes display correctly

- [ ] Phase 5b: Additional Primitives
  - [ ] drawLine implemented
  - [ ] drawCircle implemented
  - [ ] fillCircle implemented
  - [ ] drawText implemented (requires font data)

## Memory Notes

- RP2040: 264KB SRAM, ~100KB for JS heap
- Full framebuffer: ~112KB (allocated in C, NOT JS heap)
- Single buffer strategy to avoid memory exhaustion
- Always call `freeBuffer()` or `screen.destroy()` when done

## GC9A01A Command Reference

| Command | Name | Description |
|---------|------|-------------|
| 0x11 | Sleep Out | Exit sleep mode |
| 0x29 | Display On | Turn on display |
| 0x2A | Column Address Set | Set X range |
| 0x2B | Row Address Set | Set Y range |
| 0x2C | Memory Write | Begin pixel data |
| 0x36 | Memory Access Control | Set rotation |
| 0x3A | Pixel Format | Set color depth (0x05 = RGB565) |

## RGB565 Color Format

```
MSB                         LSB
RRRRR GGGGGG BBBBB
  5      6      5   bits
```

Formula: `color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)`

**Note:** The GC9A01A expects big-endian byte order over SPI. The `graphics.color565()` function automatically byte-swaps for correct display.

Examples (after byte-swap for SPI):
- Red: 0x00F8
- Green: 0xE007
- Blue: 0x1F00
- White: 0xFFFF
- Black: 0x0000
