# Waveshare RP2040-PiZero Support Roadmap

This document outlines the phased implementation plan for full Waveshare RP2040-PiZero support in mcujs.

## Board Overview

| Property | Value |
|----------|-------|
| **Chip** | RP2040 |
| **Flash** | 16MB |
| **RAM** | 264KB |
| **Form Factor** | Raspberry Pi Zero compatible (40-pin GPIO) |
| **Special Features** | DVI output, MicroSD slot, PIO-USB port, Li-battery charging |

Reference: https://www.waveshare.com/wiki/RP2040-PiZero

---

## Phase 1: Basic Board Support (Current)

**Status:** Complete

### Goals
- Get mcujs running on the RP2040-PiZero
- USB CDC serial REPL working
- Basic GPIO, I2C, SPI, UART functionality
- Filesystem on 16MB flash

### Implementation
- [x] `board/waveshare_rp2040_pizero/board_config.cmake`
- [x] `board/waveshare_rp2040_pizero/board_config.h`
- [x] Build script integration (`build.sh`, `docker-entrypoint.sh`)

### Testing Checklist
- [ ] Firmware builds without errors
- [ ] UF2 flashes and boots
- [ ] REPL responds over USB serial
- [ ] `.info` shows correct board name and 16MB flash
- [ ] Filesystem mounts and `.ls` works
- [ ] GPIO read/write works on header pins
- [ ] I2C scan detects devices
- [ ] SPI communication works

### Notes
- No onboard LED - use `board.led()` returns gracefully
- No NeoPixel - use external WS2812 on any GPIO
- Large flash (16MB) provides ~15.4MB for user files

---

## Phase 2: DVI Display Output

**Status:** Planned

### Goals
- Native DVI/HDMI output support
- Hardware-accelerated TMDS encoding via PIO
- JavaScript API for graphics rendering

### Technical Requirements

#### PIO Resources
- 3 PIO state machines (same PIO instance)
- 6 DMA channels (2 per TMDS lane)
- ~60% CPU on one core for TMDS encoding

#### Pin Mapping (from board_config.h)
```
TMDS Data 0:  GPIO 12/13 (Blue + Sync)
TMDS Data 1:  GPIO 14/15 (Green)
TMDS Data 2:  GPIO 16/17 (Red)
TMDS Clock:   GPIO 18/19
```

#### Memory Constraints
| Resolution | Color Depth | RAM Required | Feasibility |
|------------|-------------|--------------|-------------|
| 320x240 | RGB565 | 153KB | Good fit |
| 320x240 | RGB332 | 76KB | Best balance |
| 400x300 | RGB565 | 240KB | Tight |
| 640x480 | RGB565 | 614KB | Exceeds RAM - needs scanline rendering |

### Implementation Plan

1. **Add PicoDVI dependency to Docker**
   ```dockerfile
   # In Dockerfile
   RUN git clone https://github.com/Wren6991/PicoDVI.git /opt/picodvi
   ENV PICODVI_PATH=/opt/picodvi
   ```

2. **Create native DVI binding**
   - `host/bindings/dvi.c` - Core DVI driver
   - `host/bindings/dvi.h` - Public header
   - Integrate PicoDVI's `libdvi` for TMDS encoding

3. **JavaScript API**
   ```javascript
   var DVI = require('dvi');
   
   // Initialize display
   DVI.init({
     width: 320,
     height: 240,
     colorDepth: 16  // RGB565
   });
   
   // Drawing operations
   DVI.clear(0x0000);
   DVI.setPixel(x, y, color);
   DVI.fillRect(x, y, w, h, color);
   DVI.drawLine(x1, y1, x2, y2, color);
   
   // Flush framebuffer to display
   DVI.flush();
   
   // For advanced users: scanline callback for 640x480
   DVI.onScanline(function(y) {
     return generateLineData(y);
   });
   ```

4. **Update board_config.h**
   - Set `MCUJS_HAS_DVI 1`
   - Verify pin mappings match hardware

### Supported Video Modes
- 640x480 @ 60Hz (VGA) - Primary target
- 800x480 @ 60Hz (WVGA) - Stretch goal
- 720p30 - Requires overclocking to 372 MHz

### Files to Create/Modify
- [ ] `Dockerfile` - Add PicoDVI
- [ ] `host/bindings/dvi.c`
- [ ] `host/bindings/dvi.h`
- [ ] `host/bindings/CMakeLists.txt`
- [ ] `host/bindings/bindings.h`
- [ ] `host/engine.c`
- [ ] `board/waveshare_rp2040_pizero/board_config.h` - Enable DVI flag

---

## Phase 3: MicroSD Card Support

**Status:** Planned

### Goals
- Read/write files on MicroSD card
- FAT32 filesystem support
- JavaScript API for file operations

### Technical Requirements

#### SPI Configuration
```
SD Card SPI Bus: SPI1
SCK:  GPIO 10
MOSI: GPIO 11
MISO: GPIO 12
CS:   GPIO 9
```

### Implementation Plan

1. **Integrate FatFs for SD cards**
   - Already have FatFs in Docker for internal flash
   - Add SPI-based SD card driver

2. **JavaScript API**
   ```javascript
   var SD = require('sd');
   
   // Initialize SD card
   SD.init();
   
   // Check if card is present
   if (SD.isPresent()) {
     console.log('Card size:', SD.size());
   }
   
   // File operations
   var data = SD.readFile('/images/logo.bmp');
   SD.writeFile('/logs/data.txt', 'Hello');
   SD.listDir('/');
   ```

3. **Integration with DVI**
   - Load images from SD card
   - Stream video frames
   - Store large assets

### Files to Create/Modify
- [ ] `host/bindings/sd.c`
- [ ] `host/bindings/sd.h`
- [ ] Update CMakeLists.txt
- [ ] Update board_config.h - Enable SD flag

---

## Phase 4: PIO-USB Support

**Status:** Planned

### Goals
- Secondary USB port via PIO
- USB Host mode (connect keyboards, mice, gamepads)
- USB Device mode (emulate HID devices)

### Technical Requirements

#### Pin Mapping
```
USB D+: GPIO 24
USB D-: GPIO 25
```

#### Dependencies
- Pico-PIO-USB library (https://github.com/sekigon-gonnoc/Pico-PIO-USB)
- TinyUSB host stack

### Implementation Plan

1. **Add Pico-PIO-USB to Docker**

2. **JavaScript API - Host Mode**
   ```javascript
   var USBHost = require('usb-host');
   
   USBHost.init();
   
   USBHost.onDeviceConnect(function(device) {
     console.log('Device connected:', device.vendorId, device.productId);
   });
   
   USBHost.onHIDReport(function(report) {
     // Handle keyboard/mouse/gamepad input
   });
   ```

3. **JavaScript API - Device Mode**
   ```javascript
   var USBDevice = require('usb-device');
   
   // Emulate a keyboard
   USBDevice.initKeyboard();
   USBDevice.typeString('Hello World');
   USBDevice.pressKey('enter');
   ```

### Use Cases
- Connect USB keyboard for text input
- Game controller input for DVI games
- USB mass storage for file transfer
- HID device emulation

---

## Phase 5: Battery Management

**Status:** Future

### Goals
- Battery voltage monitoring
- Charging status detection
- Low battery warnings

### Implementation
- ADC reading of battery voltage divider
- GPIO for charge status LED

---

## Development Notes

### Clock Configuration
- Default: 125 MHz
- DVI mode: 252 MHz (required for VGA timing)
- 720p mode: 372 MHz (requires voltage increase)

### Memory Budget (264KB RAM)
| Component | Typical Usage |
|-----------|---------------|
| JerryScript heap | 64-128KB |
| Framebuffer (320x240 RGB565) | 153KB |
| DMA buffers | ~8KB |
| Stack + misc | ~20KB |

For 640x480, use scanline rendering instead of full framebuffer.

### PIO State Machine Allocation
| PIO | SM | Use |
|-----|-----|-----|
| PIO0 | 0-2 | DVI TMDS lanes (when enabled) |
| PIO0 | 3 | Available |
| PIO1 | 0 | NeoPixel (if used) |
| PIO1 | 1-2 | PIO-USB (when enabled) |
| PIO1 | 3 | Available |

### Testing Hardware Setup
- HDMI monitor/TV for DVI testing
- MicroSD card (FAT32, <32GB recommended)
- USB keyboard/mouse for PIO-USB testing
- LiPo battery for power testing

---

## Contributing

When implementing a new phase:

1. Create feature branch: `feature/pizero-phase-N`
2. Update this roadmap with implementation details
3. Add tests for new functionality
4. Update CHANGELOG.md
5. Submit PR with phase checklist complete
