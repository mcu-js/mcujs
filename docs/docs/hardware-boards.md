---
sidebar_position: 8
---

# Hardware and Boards

This page lists the boards that are part of the release build. `scripts/lib/boards.sh`
keeps the nine RP build targets in `MCUJS_BOARDS` and the complete cross-platform
package set in `MCUJS_RELEASE_BOARDS`; keep this table in sync with both lanes.

## Supported boards

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

## Authoritative 0.2 runtime feature matrix

The runtime descriptor in `runtime/board-registry.js` is authoritative for
firmware-enabled modules and release capability manifests. A check means the
module is registered in that image; it does not claim an external device is
attached. `NeoPixel` describes the external driver, while the final column lists
only physically onboard shortcuts.

| Target | GPIO | PWM | I2C | SPI | ADC | NeoPixel | Image/graphics | USB keyboard/mouse | Onboard shortcut |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pico` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | LED |
| `pico2` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | LED |
| `pico2_w` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | managed LED |
| `waveshare_rp2040_zero` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | NeoPixel |
| `waveshare_rp2040_pizero` | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ + DVI | ✓ | — |
| `waveshare_rp2040_touch_lcd_1.28` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| `waveshare_rp2350_lcd_1.47_a` | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | ✓ | NeoPixel |
| `waveshare_rp2350_touch_lcd_1.69` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — |
| `adafruit_feather_rp2040` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | LED + NeoPixel |
| `seeed_xiao_esp32s3` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | LED |

All targets also enable `board`, `fs`, `process`, `mcujs:module`, and the shared
console/timer/module-loader runtime. XIAO ESP32-S3 is intentionally constrained:
its current firmware exposes CDC+MSC but not image/graphics or USB HID. The
RP2350-LCD-1.47-A reserves its internal display/storage routes, so its firmware
does not advertise general SPI or ADC. Safe/exposed pin maps omit reserved DVI,
wireless, display, storage, USB, flash, and PSRAM wiring. Every target with the
`fs` capability exposes dynamic `board.storageReady()`. Only XIAO currently
advertises persistent `boot.safeMode` and therefore exposes `board.safeMode()`;
the RP BOOTSEL-at-reset gesture is not misreported as persistent safe-mode state.

## Build names

Release firmware is named `mcujs-<version>-<board-id>.uf2`. For example, the Raspberry Pi Pico artifact for version `0.1.0` is `mcujs-0.1.0-pico.uf2`.

Use the board ID when building from source:

```bash
./build.sh pico
./build.sh waveshare_rp2040_pizero
./build.sh all --clean
```

The XIAO image and its adjacent capability manifest are produced by the ESP32
build/package lane under `platform/esp32/build-docker/`.

## Pin notes

- Onboard LED: usually [GPIO](./glossary.md#gpio) 25 on Pico-style boards.
- Pico 2 W uses the CYW43 wireless chip for the LED, not a direct GPIO LED.
- Waveshare RP2040-Zero uses a NeoPixel on [GPIO](./glossary.md#gpio) 16.
- Waveshare RP2040-PiZero exposes DVI/HDMI output through the board-specific DVI bindings.
- Waveshare LCD boards use board-local display driver examples under `examples/`.
- Check your board silkscreen for [I2C](./glossary.md#i2c), [SPI](./glossary.md#spi), and ADC pins before wiring peripherals.

### Waveshare RP2040 Touch LCD 1.28

| Function | GPIO |
| --- | --- |
| LCD SPI1 SCK | 10 |
| LCD SPI1 MOSI | 11 |
| LCD SPI1 MISO | 12 |
| LCD CS | 9 |
| LCD DC | 8 |
| LCD RST | 13 |
| LCD BL | 25 |
| Touch/IMU I2C1 SDA | 6 |
| Touch/IMU I2C1 SCL | 7 |
| Touch INT | 21 |
| Touch RST | 22 |
| IMU INT1 | 23 |
| IMU INT2 | 24 |
| Battery ADC | 29 |

**Quickstart:** Copy files from `examples/waveshare-lcd-1.28/` to the device:

```js
// Display example
const { GC9A01A } = require('./gc9a01a');
const display = GC9A01A();
display.init();
display.fill(display.color565(8, 12, 18));

// Touch example
const { CST816S } = require('./cst816s');
const touch = CST816S();
touch.init();
const t = touch.read();
if (t.touching) console.log(t.x, t.y);

// IMU example
const { QMI8658 } = require('./qmi8658');
const imu = QMI8658();
imu.init();
const data = imu.read();
console.log('Accel:', data.accel);
```

**Available demos:** `demo-slideshow.js`, `demo-fps.js`, `demo-touch-draw.js`, `demo-imu.js`

### Waveshare RP2350-LCD-1.47-A

| Function | GPIO |
| --- | --- |
| LCD SPI0 SCK | 18 |
| LCD SPI0 MOSI | 19 |
| LCD CS | 17 |
| LCD DC | 16 |
| LCD RST | 20 |
| LCD BL | 21 |
| SD Card SPI1 SCK | 10 |
| SD Card SPI1 MOSI | 11 |
| SD Card SPI1 MISO | 12 |
| SD Card CS | 15 |
| NeoPixel (WS2812B) | 22 |

**Quickstart:** Copy files from `examples/waveshare-lcd-1.47/` to the device:

```js
// Display example (320x172 horizontal, ST7789V3)
var s = require('./screen.js').createScreen();
s.init();
s.clear();
s.fillRect(0, 0, 320, 86, s.RED);
s.fillRect(0, 86, 320, 86, s.BLUE);
s.flush();
```

### Waveshare RP2350-Touch-LCD-1.69

| Function | GPIO |
| --- | --- |
| LCD SPI1 SCK | 10 |
| LCD SPI1 MOSI | 11 |
| LCD SPI1 MISO | 12 |
| LCD CS | 9 |
| LCD DC | 8 |
| LCD RST | 13 |
| LCD BL | 25 |
| Touch/IMU I2C1 SDA | 6 |
| Touch/IMU I2C1 SCL | 7 |
| Touch INT | 21 |
| Touch RST | 22 |
| IMU INT1 | 23 |
| IMU INT2 | 24 |
| Buzzer PWM | 2 |
| Battery ADC | 29 |

**Available demos:** `demo-slideshow.js`, `demo-fps.js`, `demo-touch.js`, `demo-imu.js`, `demo-buzzer.js`

## Adding a board

1. Add `board/<board-id>/board_config.h` and `board/<board-id>/board_config.cmake`.
2. Add a memory map file if the board needs a non-default flash layout.
3. Add the board ID and metadata to `scripts/lib/boards.sh`.
4. Run `scripts/verify-release.sh --allow-dirty`.
5. Build the board with `./build.sh <board-id>`.
6. Document any board-specific examples or pin notes here.

## Key terms

- [GPIO](./glossary.md#gpio)
- [I2C](./glossary.md#i2c)
- [SPI](./glossary.md#spi)
- [NeoPixel](./glossary.md#neopixel)
- [RP2040](./glossary.md#rp2040)
- [RP2350](./glossary.md#rp2350)
