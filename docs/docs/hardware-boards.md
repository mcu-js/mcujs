---
sidebar_position: 8
---

# Hardware and Boards

Start here to see which boards are ready today and what is coming next.

## Supported boards

| Board | Chip | Flash | Status |
| --- | --- | --- | --- |
| Raspberry Pi Pico | RP2040 | 2MB | Supported |
| Raspberry Pi Pico 2 | RP2350 | 4MB | Supported |
| Waveshare RP2040-Zero | RP2040 | 2MB | Supported |
| Waveshare RP2040 Touch LCD 1.28 | RP2040 | 4MB | Supported |

## Pin notes

- Onboard LED: usually [GPIO](./glossary.md#gpio) 25 on Pico
- Waveshare RP2040-Zero uses a NeoPixel on [GPIO](./glossary.md#gpio) 16 (shared with SPI0 MISO)
- Waveshare RP2040 Touch LCD 1.28 uses SPI1 for the LCD and I2C1 for touch/IMU
- Check your board silkscreen for [I2C](./glossary.md#i2c)/[SPI](./glossary.md#spi) pins

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

## Key terms

- [GPIO](./glossary.md#gpio)
- [I2C](./glossary.md#i2c)
- [SPI](./glossary.md#spi)
- [NeoPixel](./glossary.md#neopixel)
- [RP2040](./glossary.md#rp2040)
- [RP2350](./glossary.md#rp2350)

## Planned boards

- Adafruit Feather RP2040/RP2350
- Generic RP2040/RP2350 configs
