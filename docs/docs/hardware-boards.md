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
| LCD CS | 9 |
| LCD DC | 8 |
| LCD RST | 12 |
| LCD BL | 25 |
| Touch/IMU I2C1 SDA | 6 |
| Touch/IMU I2C1 SCL | 7 |

Quickstart: copy both `examples/waveshare-lcd-1.28/index.js` and
`examples/waveshare-lcd-1.28/gc9a01a.js` to the device root, then run:

```js
const { GC9A01A } = require('./gc9a01a');

const display = GC9A01A();
display.init();
display.fill(display.color565(8, 12, 18));
```

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
