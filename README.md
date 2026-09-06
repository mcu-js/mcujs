# mcujs

A JavaScript runtime for microcontrollers, currently shipping for RP2040,
RP2350, and ESP32-S3 boards, in the same spirit as Node.js for servers.

Docs: https://mcujs.org/

## Features

- **USB Flash Drive**: Mount your board as a USB drive and drop in your `index.js`
- **Serial REPL**: Interactive JavaScript console over USB serial
- **Hardware APIs**: GPIO, PWM, I2C, SPI, ADC, and NeoPixel
- **CommonJS Modules**: Use `require()` for modular code with `/lib/` module resolution
- **Minimal Footprint**: Built on JerryScript for embedded systems

## Supported Boards

Current release board IDs are:

- `pico`
- `pico2`
- `pico2_w`
- `waveshare_rp2040_zero`
- `waveshare_rp2040_pizero`
- `waveshare_rp2040_touch_lcd_1.28`
- `waveshare_rp2350_lcd_1.47_a`
- `waveshare_rp2350_touch_lcd_1.69`
- `adafruit_feather_rp2040`
- `seeed_xiao_esp32s3`

## Quick Start

Download the latest `.uf2` file for your board from
[Releases](https://github.com/mcu-js/mcujs/releases), then follow the matching
update flow.

### RP2040 and RP2350 install or update

1. Hold **BOOTSEL** and connect the board via USB.
2. Copy the `.uf2` to the `RPI-RP2` drive on RP2040 boards or the `RP2350`
   drive on RP2350 boards.
3. The board reboots and appears as a USB drive named `MCUJS`.

### XIAO ESP32-S3 update

The release UF2 is application-only and requires an existing compatible TinyUF2
baseline. Initial provisioning or recovery is an engineering operation; follow
the preservation requirements in
[`platform/esp32/README.md`](platform/esp32/README.md) instead of attempting a
full flash.

1. In the running MCU.js CDC REPL, enter `board.enterUf2()`.
2. Wait for the `XIAOS3BOOT` drive, then copy
   `mcujs-<version>-seeed_xiao_esp32s3.uf2` to it.
3. The board reboots and appears as a USB drive named `MCUJS`.

### Run JavaScript

Create an `index.js` file on the `MCUJS` drive. This portable example uses the
declared onboard-device API rather than a board-specific GPIO number:

```javascript
const board = require('board');

if (!board.devices.led) {
    console.log('This board has no onboard LED');
} else {
    let ledOn = false;
    board.led(ledOn);

    setInterval(() => {
        ledOn = !ledOn;
        board.led(ledOn);
    }, 500);

    console.log('Blinking!');
}
```

Eject the drive, then reset the board. Your code runs automatically.

## Serial REPL

Connect to the board's serial port (115200 baud) for an interactive JavaScript
console. For example, on a Pico:

```
mcujs v0.1.0 on pico
> console.log('Hello!')
Hello!
undefined
> board.led(true)
undefined
> 2 + 2
4
```

### REPL Features

- **Command history**: Use Up/Down arrow keys to browse previous commands
- **Line editing**: Left/Right arrows, Home/End, Backspace anywhere in line
- **Tab completion**: Press Tab to auto-complete (e.g., `cons<Tab>` → `console`)

### REPL Commands

| Command | Description |
|---------|-------------|
| `.help` | Show available commands |
| `.info` | Show board info (chip, memory, filesystem) |
| `.ls` | List files on the device |
| `.cat FILE` | Display file contents |
| `.rm FILE` | Delete a file |
| `.run FILE` | Execute a JavaScript file |
| `.multiline [FILE]` | Multi-line input (end with `.end`) |
| `.uf2` | Reboot into UF2 mode (prompted) |
| `.uf2!` | Reboot into UF2 mode immediately |
| `.usbreset` | Reset USB connection (reboot) |

The `.info` command includes the current build ID (version + git SHA).

### Safe Mode


On RP2040 and RP2350 boards, hold **BOOTSEL** during power-on to skip
`index.js` auto-run. The XIAO ESP32-S3 records failed boot scripts and enters
persistent safe mode on the next reset; see its
[recovery documentation](platform/esp32/README.md#indexjs-and-persistent-safe-mode).

`index.js` runs immediately on boot; the REPL banner prints the first time a CDC serial connection is opened.

## JavaScript API

### Console
```javascript
console.log('message');
console.warn('warning');
console.error('error');
```

### GPIO
```javascript
const GPIO = require('gpio');

GPIO.init(pin, GPIO.OUTPUT);      // or GPIO.INPUT, GPIO.INPUT_PULLUP, GPIO.INPUT_PULLDOWN
GPIO.set(pin, true);              // Set high
GPIO.set(pin, false);             // Set low
GPIO.get(pin);                    // Read pin state (boolean)
GPIO.toggle(pin);                 // Toggle output
```

### Timers
```javascript
const id = setTimeout(callback, ms);
clearTimeout(id);

const id = setInterval(callback, ms);
clearInterval(id);
```

### PWM
```javascript
const PWM = require('pwm');

PWM.init(pin, frequency);         // Initialize PWM on pin
PWM.setDuty(pin, duty);           // duty: 0.0-1.0 ratio
PWM.stop(pin);
```

### I2C
```javascript
const board = require('board');
const I2C = require('i2c');
const limits = board.capability('i2c');

I2C.init({ frequency: 100000 });         // defaultBus + defaultRoute
I2C.write(limits.defaultBus, address, data);
I2C.read(limits.defaultBus, address, length);
```

Use only listed routes and keep transfers at or below `maxTransferBytes`.
The positional `I2C.init(bus, sda, scl, frequency)` form remains available
through 0.x for migration.

### SPI
```javascript
const SPI = require('spi');

SPI.init(bus, sck, mosi, miso, baudrate);
SPI.transfer(bus, data);          // returns received data
```

### Filesystem (fs)
```javascript
const fs = require('fs');

fs.readFileSync(path);            // Read file as string
fs.writeFileSync(path, data);     // Write string to file
fs.appendFileSync(path, data);    // Append to file
fs.existsSync(path);              // Check if file exists
fs.unlinkSync(path);              // Delete file
fs.readdirSync(path);             // List directory (returns array)
fs.statSync(path);                // Get file info {size, isFile, isDirectory}
fs.renameSync(oldPath, newPath);  // Rename/move file
fs.mkdirSync(path);               // Create directory
```

Filesystem capacity is derived from the remaining flash after the firmware image and EEPROM reservation.

### Modules (require)
```javascript
// Relative imports
const utils = require('./utils');       // ./utils.js
const helper = require('../lib/helper'); // ../lib/helper.js

// Absolute imports  
const config = require('/config');      // /config.js

// Bare module imports (searches /lib/)
const math = require('math');           // /lib/math.js

// JSON imports
const config = require('./config.json'); // Parsed as JSON
const pkg = require('package');          // /lib/package.json (if no .js found)

// CommonJS exports
// In /lib/math.js:
exports.add = (a, b) => a + b;
exports.PI = 3.14159;

// Or use module.exports:
module.exports = { add, PI };

// Module info available inside modules:
console.log(__filename);  // e.g., "/lib/math.js"
console.log(__dirname);   // e.g., "/lib"
```

### Board
```javascript
board.name;                       // Board name (e.g., "pico")
board.chip;                       // Chip (e.g., "RP2040")
board.ledPin;                     // Direct GPIO LED pin (absent for managed/no LED)
board.led(true);                  // Control onboard LED
board.led();                      // Read LED state
board.neopixelPin;                // Onboard NeoPixel pin (if present)
board.neopixelLength;             // Onboard NeoPixel count (if present)
board.neopixel([255, 80, 10]);    // Convenience for onboard NeoPixel
board.neopixel({ r: 255, g: 80, b: 10 });
board.neopixel([[255, 0, 0]]);   // Lists must not exceed onboard length
board.freeMemory();               // Free JS heap memory in bytes
board.uniqueId();                 // Board unique ID (hex string)
board.millis();                   // Milliseconds since boot
board.delay(ms);                  // Blocking delay
board.reset();                    // Reset USB connection (reboot)
board.enterUf2();                 // Reboot into UF2 bootloader
```

Missing color values default to 0. Color arrays longer than three bytes and pixel lists longer than the onboard length throw `RangeError`; they are never truncated. Object inputs are RGB; array inputs follow the active `neopixel.init()` order. Array-of-objects stays RGB.

### ADC
```javascript
(function () {
  var boardApi = require('board');
  var adc = require('adc');
  var capability = boardApi.capability('adc');
  var route = capability.channels[0];

  adc.readPin(route.pin);                    // Raw count (0..2^resolutionBits - 1)
  adc.readChannel(route.channel);            // Same route by advertised channel
  adc.readVoltagePin(route.pin);             // Volts; see voltage.calibrated
  adc.readVoltageChannel(route.channel);     // Volts by advertised channel
  if (capability.temperature.supported) {
    adc.readTempC();                         // Internal die temperature (°C)
  }

  if ('TEMP' in adc) adc.TEMP;               // Deprecated RP-only raw channel
  if ('VSYS' in adc) adc.VSYS;               // Deprecated RP-only VSYS/3 channel
}());
```

### Process
```javascript
process.version;                  // mcujs version (e.g., "v0.1.0")
process.arch;                     // CPU architecture (e.g., "RP2040")
process.platform;                 // Always "mcujs"
process.versions;                 // {mcujs, jerryscript, "pico-sdk", tinyusb}
```

### NeoPixel
```javascript
const neopixel = require('neopixel');

neopixel.init({ pin: 16, length: 1, order: 'GRB' });
// order can be "GRB" (default) or "RGB"
neopixel.setPixel(0, 255, 80, 10);
neopixel.show();
```

### Built-in Modules
```javascript
const { builtinModules } = require('mcujs:module');
// alias: require('node:module')
// This frozen list is board-dependent. Inspect it, or call
// require('mcujs:module').has(name), before requiring an optional module.
```

## Known Limitations

### Filesystem Sync

The board appears as both a USB serial device and a USB flash drive (composite device). There are some sync considerations:

| Direction | Behavior |
|-----------|----------|
| **Host → Device** | Files copied via USB are immediately visible to JavaScript after using REPL commands (`.ls`, `.cat`, `.run`) |
| **Device → Host** | Files written from JavaScript (e.g., `fs.writeFileSync()`) persist correctly but may not appear on the host until you remount or replug |

**Why?** Linux aggressively caches FAT filesystem directories. When the device writes files internally, the host doesn't know to refresh its cache.

**Workaround:** After writing files from JavaScript, either:
- Remount on Linux: `udisksctl unmount -b /dev/sdX1 && udisksctl mount -b /dev/sdX1`
- Or simply unplug and replug the Pico

## Development

```bash
scripts/verify-release.sh --allow-dirty
./build.sh pico
bun run e2e
```

Build every release board and package deterministic artifacts:

```bash
scripts/release.sh
```

See [CONTRIBUTING.md](CONTRIBUTING.md), [RELEASING.md](RELEASING.md), and the Docusaurus docs under `docs/docs/` for the full contributor workflow.

Files written from JavaScript are always persisted to flash immediately - they will survive power cycles even if not yet visible on the host.

## Building from Source

### Prerequisites

- Docker (recommended) or:
  - ARM GCC toolchain (`gcc-arm-none-eabi`)
  - CMake 3.13+
  - Pico SDK 2.2.0

### Build with Docker

```bash
# Prepare SDK images explicitly (may download dependencies)
./build.sh pico --prepare-image
./build.sh seeed_xiao_esp32s3 --prepare-image

# Build for Pico
./build.sh pico

# Build for Pico 2
./build.sh pico2

# Build all RP boards (the existing default)
./build.sh all

# Build for Seeed XIAO ESP32-S3
./build.sh seeed_xiao_esp32s3
```

Compilation is networkless and never builds or pulls a missing SDK image. Source
is mounted read-only; outputs use the caller's UID/GID. RP artifacts are written
to `build/`, with UF2 names `mcujs-<version>-<board>.uf2`. XIAO artifacts stay in
`platform/esp32/build-docker/`. Override local builders with
`MCUJS_RP_DOCKER_IMAGE` or `MCUJS_ESP32_DOCKER_IMAGE`.
See [building options](docs/docs/advanced-building.md) for preparation-only network
flags, the changed `--rebuild-image` alias, and architecture/verification limits.

### XIAO ESP32-S3 Release Build

The Seeed Studio XIAO ESP32-S3 is part of the mandatory release board matrix.
Build its application-only UF2 and capability manifest with the pinned ESP-IDF
5.3.2 Docker lane:

```bash
./build.sh seeed_xiao_esp32s3
```

See [`platform/esp32/README.md`](platform/esp32/README.md) for dependency pins,
partition ownership, app-only flashing, recovery, and hardware smoke tests.

### End-to-End Tests (Bun)

The Bun test suite builds firmware, flashes UF2 if needed, and exercises REPL, filesystem, and JS APIs.

```bash
bun run e2e
```

Requirements:
- Pico connected via USB (CDC + MSC visible)
- `bun`, `udisksctl`, and `lsblk` available

### Manual Build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk

mkdir build && cd build
cmake -DMCUJS_PLATFORM=rp2 -DBOARD=pico ..
make -j$(nproc)
```

## Supported Boards

| Board ID | Board | Chip | Flash | Notes |
| --- | --- | --- | --- | --- |
| `pico` | Raspberry Pi Pico | RP2040 | 2MB | Onboard LED |
| `pico2` | Raspberry Pi Pico 2 | RP2350 | 4MB | Onboard LED |
| `pico2_w` | Raspberry Pi Pico 2 W | RP2350 | 4MB | CYW43 LED support |
| `waveshare_rp2040_zero` | Waveshare RP2040-Zero | RP2040 | 2MB | Onboard NeoPixel |
| `waveshare_rp2040_pizero` | Waveshare RP2040-PiZero | RP2040 | 16MB | DVI/HDMI output |
| `waveshare_rp2040_touch_lcd_1.28` | Waveshare RP2040 Touch LCD 1.28 | RP2040 | 4MB | Round LCD, touch, IMU |
| `waveshare_rp2350_lcd_1.47_a` | Waveshare RP2350-LCD-1.47-A | RP2350 | 16MB | LCD, NeoPixel |
| `waveshare_rp2350_touch_lcd_1.69` | Waveshare RP2350-Touch-LCD-1.69 | RP2350 | 16MB | LCD, touch, IMU, buzzer |
| `adafruit_feather_rp2040` | Adafruit Feather RP2040 | RP2040 | 8MB | NeoPixel, STEMMA QT |
| `seeed_xiao_esp32s3` | Seeed Studio XIAO ESP32-S3 | ESP32-S3 | 8MB | Native USB, onboard LED |

## Architecture

```
mcujs/
├── host/                # Engine boundary, module loader, and bindings
│   ├── engine.*         # JerryScript adapter
│   ├── module_loader.c  # CommonJS require() implementation
│   └── bindings/        # Native API bindings (GPIO, I2C, fs, etc.)
├── javascript/          # JerryScript build adapter
├── board/               # Board-specific configurations by board ID
│   └── <board-id>/      # board_config.h and board_config.cmake
├── platform/            # MCU/SDK-specific implementations and build hooks
│   └── rp2/             # RP2040/RP2350 Pico SDK backend
├── src/                 # Shared firmware contracts and runtime code
│   ├── usb/             # USB CDC/MSC/HID interfaces
│   └── filesystem/      # Shared FAT filesystem plus storage interfaces
├── examples/            # Example JavaScript programs
└── scripts/             # Board registry, verification, and release tooling
```

The selected `MCUJS_PLATFORM` supplies the firmware entrypoint, JerryScript
port, board/boot behavior, storage, USB implementation, and hardware bindings.
See [`platform/README.md`](platform/README.md) for the backend contract. Shared
runtime sources are prohibited from directly including platform SDK headers.

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- [JerryScript](https://jerryscript.net/) - Lightweight JavaScript engine
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [TinyUSB](https://github.com/hathach/tinyusb) - USB stack
- [FatFS](http://elm-chan.org/fsw/ff/) - FAT filesystem
