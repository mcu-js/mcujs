/* Waveshare RP2350-Touch-LCD-2.8: initial runtime/USB/filesystem port.
 * LCD wiring: official Waveshare bsp_st7789.h and bsp_lcd_brightness.h.
 * Touch, audio, SD, sensors and public GPIO/buses are not qualified.
 */
#ifndef MCUJS_BOARD_CONFIG_H
#define MCUJS_BOARD_CONFIG_H
#include "../flash_config.h"
#define MCUJS_BOARD_NAME "waveshare_rp2350_touch_lcd_2.8"
#define MCUJS_BOARD_CHIP "RP2350"
#define MCUJS_BOARD_CHIP_ID 0x2350
#define MCUJS_HAS_UF2 1
#define MCUJS_FLASH_SIZE FLASH_SIZE_16MB
#define MCUJS_RAM_SIZE (520 * 1024)
#define MCUJS_CPU_FREQ_HZ (150 * 1000 * 1000)
/* Board abstraction sentinel: no qualified onboard LED or NeoPixel. */
#define MCUJS_LED_PIN 255
#define MCUJS_NEOPIXEL_PIN 255

/* ST7789T3, native portrait geometry. Write-only SPI: no MISO pin. */
#define MCUJS_LCD_SPI_BUS 1
#define MCUJS_LCD_SCK_PIN 10
#define MCUJS_LCD_MOSI_PIN 11
#define MCUJS_LCD_CS_PIN 13
#define MCUJS_LCD_DC_PIN 14
#define MCUJS_LCD_RST_PIN 15
#define MCUJS_LCD_BL_PIN 16
#define MCUJS_LCD_WIDTH 240
#define MCUJS_LCD_HEIGHT 320
#define MCUJS_LCD_X_OFFSET 0
#define MCUJS_LCD_Y_OFFSET 0
#define MCUJS_LCD_SPI_MODE 3

/* Use the existing MCU.js RP2350 USB identity. */
#define MCUJS_USB_VID 0x2E8A
#define MCUJS_USB_PID 0x000E
#endif
