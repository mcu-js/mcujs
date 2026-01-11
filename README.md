# mcujs

A minimal JavaScript runtime for Raspberry Pi Pico and Pico 2 microcontrollers.

## Features

- **USB Flash Drive**: Mount your Pico as a USB drive and drop in your `index.js`
- **Serial REPL**: Interactive JavaScript console over USB serial
- **Hardware APIs**: GPIO, PWM, I2C, SPI, and timers
- **ES Modules**: Use `require()` for modular code with `/lib/` module resolution
- **Minimal Footprint**: Built on JerryScript for embedded systems

## Quick Start

1. Download the latest `.uf2` file for your board from [Releases](https://github.com/your-repo/mcujs/releases)
2. Hold BOOTSEL and connect your Pico via USB
3. Drag the `.uf2` file to the RPI-RP2 drive
4. The Pico will reboot and appear as a USB drive named "MCUJS"
5. Create an `index.js` file on the drive:

```javascript
// Blink the onboard LED
const LED = 25;

GPIO.init(LED, GPIO.OUTPUT);

setInterval(() => {
    GPIO.toggle(LED);
}, 500);

console.log('Blinking!');
```

6. Reset the Pico - your code runs automatically!

## Serial REPL

Connect to the Pico's serial port (115200 baud) for an interactive JavaScript console:

```
mcujs v0.1.0 on pico
> console.log('Hello!')
Hello!
undefined
> GPIO.set(25, true)
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
| `.uf2` | Reboot into UF2 mode (prompted) |
| `.uf2!` | Reboot into UF2 mode immediately |
| `.usbreset` | Reset USB connection (reboot) |

The `.info` command includes the current build ID (version + git SHA).

### Safe Mode


Hold the **BOOTSEL** button during power-on to skip `index.js` auto-run. This allows recovery from scripts with infinite loops without reflashing.

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
PWM.init(pin, frequency);         // Initialize PWM on pin
PWM.setDuty(pin, duty);           // duty: 0-65535 or 0.0-1.0
PWM.stop(pin);
```

### I2C
```javascript
I2C.init(bus, sda, scl, baudrate);
I2C.write(bus, address, data);    // data: array of bytes
I2C.read(bus, address, length);   // returns array of bytes
```

### SPI
```javascript
SPI.init(bus, sck, mosi, miso, baudrate);
SPI.transfer(bus, data);          // returns received data
```

### Filesystem (fs)
```javascript
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
board.ledPin;                     // Onboard LED pin number
board.led(true);                  // Control onboard LED
board.led();                      // Read LED state
board.freeMemory();               // Free JS heap memory in bytes
board.uniqueId();                 // Board unique ID (hex string)
board.millis();                   // Milliseconds since boot
board.delay(ms);                  // Blocking delay
board.reset();                    // Reset USB connection (reboot)
board.enterUf2();                 // Reboot into UF2 bootloader
```

### Process
```javascript
process.version;                  // mcujs version (e.g., "v0.1.0")
process.arch;                     // CPU architecture (e.g., "RP2040")
process.platform;                 // Always "mcujs"
process.versions;                 // {mcujs, jerryscript, "pico-sdk", tinyusb}
```

## Known Limitations

### Filesystem Sync

The Pico appears as both a USB serial device and a USB flash drive (composite device). There are some sync considerations:

| Direction | Behavior |
|-----------|----------|
| **Host → Device** | Files copied via USB are immediately visible to JavaScript after using REPL commands (`.ls`, `.cat`, `.run`) |
| **Device → Host** | Files written from JavaScript (e.g., `fs.writeFileSync()`) persist correctly but may not appear on the host until you remount or replug |

**Why?** Linux aggressively caches FAT filesystem directories. When the device writes files internally, the host doesn't know to refresh its cache.

**Workaround:** After writing files from JavaScript, either:
- Remount on Linux: `udisksctl unmount -b /dev/sdX1 && udisksctl mount -b /dev/sdX1`
- Or simply unplug and replug the Pico

Files written from JavaScript are always persisted to flash immediately - they will survive power cycles even if not yet visible on the host.

## Building from Source

### Prerequisites

- Docker (recommended) or:
  - ARM GCC toolchain (`gcc-arm-none-eabi`)
  - CMake 3.13+
  - Pico SDK 2.x

### Build with Docker

```bash
# Build for Pico
./build.sh pico

# Build for Pico 2
./build.sh pico2

# Build all boards
./build.sh all
```

Output files are in `build/`:
- `mcujs-0.1.0-pico.uf2`
- `mcujs-0.1.0-pico2.uf2`

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
cmake -DBOARD=pico ..
make -j$(nproc)
```

## Supported Boards

| Board | Chip | Flash | Status |
|-------|------|-------|--------|
| Raspberry Pi Pico | RP2040 | 2MB | Supported |
| Raspberry Pi Pico 2 | RP2350 | 4MB | Supported |
| WaveShare RP2040/RP2350 | Various | Various | Planned |
| Adafruit Feather RP2040/RP2350 | Various | Various | Planned |

## Architecture

```
mcujs/
├── host/                # JavaScript engine and bindings
│   ├── engine.c         # JerryScript wrapper
│   ├── module_loader.c  # CommonJS require() implementation
│   └── bindings/        # Native API bindings (GPIO, I2C, fs, etc.)
├── javascript/          # JerryScript engine (built from source)
├── board/               # Board-specific configurations
│   ├── pico/            # Raspberry Pi Pico
│   └── pico2/           # Raspberry Pi Pico 2
├── src/                 # Core firmware
│   ├── usb/             # USB CDC + MSC composite device
│   └── filesystem/      # FAT12 filesystem with subdirectory support
└── examples/            # Example JavaScript programs
```

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- [JerryScript](https://jerryscript.net/) - Lightweight JavaScript engine
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [TinyUSB](https://github.com/hathach/tinyusb) - USB stack
- [FatFS](http://elm-chan.org/fsw/ff/) - FAT filesystem
