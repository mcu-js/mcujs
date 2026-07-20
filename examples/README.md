# mcujs Examples

Example programs demonstrating MCU.js features across supported boards.

## Usage

Copy the `index.js` file from an example folder to the board's MCU.js filesystem.

1. Mount the Pico filesystem: `udisksctl mount -b /dev/sda`
2. Copy the example: `cp examples/blink/index.js /run/media/$USER/MCUJS/`
3. Reset the Pico or run `.load` in the REPL

## Examples

### hello/
Basic hello world - prints board info and a periodic heartbeat message.

**Concepts:** `console.log`, `board` object, `setInterval`

### blink/
Portable LED blink - uses onboard inventory and toggles the LED every 500ms for 30 seconds.

**Concepts:** `board.devices`, `gpio.init`, strict boolean `gpio.set`, `setInterval`

**Hardware:** A board with a declared onboard LED; managed LEDs use `board.led()` and GPIO LEDs use their declared pin.

### button/
Button input with software debounce - toggles LED on button press.

**Concepts:** `GPIO.INPUT_PULLUP`, `GPIO.get`, debouncing, state tracking

**Hardware:** Push button connected between GPIO 15 and GND

### pwm-fade/
Portable LED breathing effect using PWM - smoothly fades an advertised PWM-capable GPIO LED up and down for 15 seconds.

**Concepts:** `board.capability('pwm')`, `pwm.init`, ratio-only `pwm.setDuty`, safe reruns

**Hardware:** A PWM-capable onboard GPIO LED, or set `globalThis.pwmFadePin` to an advertised PWM pin connected to an LED and resistor. Set `globalThis.pwmFadeActiveLow = true` for active-low wiring.

### i2c-scan/
Scans readable standard addresses on the board-declared default I2C route.

**Concepts:** `board.capability('i2c')`, options-object `i2c.init`,
`defaultRoute`, strict `ENXIO` handling, safe persistent-realm reruns

**Hardware:** A 3.3 V I2C target connected to the printed `defaultRoute`, with
appropriate SDA/SCL pull-ups and a common ground. Read-incompatible targets may
not appear; unexpected bus and native I/O errors stop the scan.

### modules/
Demonstrates the CommonJS `require()` system with custom modules.

**Concepts:** `require()`, `exports`, module caching, `/lib/` directory

**Hardware:** None

### config/
Load application settings from JSON configuration files.

**Concepts:** `require()` with JSON, configuration-driven code

**Hardware:** None (uses onboard LED on GPIO 25)

### waveshare-lcd-1.28/
Examples for Waveshare RP2040 Touch LCD 1.28" (240x240 round GC9A01A display).

**Files:**
- `gc9a01a.js` - Low-level display driver
- `screen.js` - High-level drawing API
- `demo-fps.js` - FPS benchmark with bouncing ball
- `demo-slideshow.js` - Slideshow with shapes, text, colors
- `demo-touch-draw.js` - Touch-enabled drawing demo
- `demo-imu.js` - IMU visualization

**Hardware:** Waveshare RP2040 Touch LCD 1.28" board

### waveshare-lcd-1.47/
Examples for Waveshare RP2350-LCD-1.47-A (320x172 ST7789V3 display).

**Files:**
- `st7789v3.js` - Low-level display driver
- `screen.js` - High-level drawing API
- `demo-fps.js` - FPS benchmark with bouncing balls
- `demo-slideshow.js` - Slideshow with shapes, text, colors
- `demo-shapes.js` - Animated shapes demo
- `demo-rainbow.js` - Rainbow/color effects demo

**Hardware:** Waveshare RP2350-LCD-1.47-A board

## Pin Reference

| Function | Default Pin |
|----------|-------------|
| Onboard LED | GPIO 25 |
| I2C0 SDA | GPIO 4 |
| I2C0 SCL | GPIO 5 |
| I2C1 SDA | GPIO 6 |
| I2C1 SCL | GPIO 7 |
| SPI0 SCK | GPIO 18 |
| SPI0 TX | GPIO 19 |
| SPI0 RX | GPIO 16 |
| SPI0 CS | GPIO 17 |

## Creating Your Own

1. Create a new folder in `examples/`
2. Add an `index.js` file with your code
3. Include a comment header explaining what it does
4. Test on real hardware before committing
